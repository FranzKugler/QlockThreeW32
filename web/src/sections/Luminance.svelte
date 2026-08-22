<script>
  /**
   * Luminance
   * The workbench for the automatic brightness, at #luminance.
   *
   * Not a tab, and not a setting: it shows what the clock has been taught and
   * what it concluded, so the curve can be judged with numbers instead of by
   * squinting at a wall. Read-only - the one write is the reset button in the
   * colour tab, which is where somebody would look for it.
   *
   * Deliberately not behind expert mode either. There is no secret in a
   * brightness curve, and having to unlock the clock to look at one would put
   * a lock in the way of something it has nothing to do with.
   *
   * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
   * @version  2.1
   * @created  22.8.2026
   * @updated  22.8.2026
   */
  import { onMount } from 'svelte';
  import * as api from '../lib/api.js';
  import { dict } from '../lib/i18n.svelte.js';

  const t = $derived(dict());

  let data = $state(null);
  let error = $state(null);

  // Faster than the colour tab, because this is the screen somebody watches
  // while dragging the slider to see what the clock does with it.
  const POLL_MS = 1000;

  async function refresh() {
    try {
      data = await api.fetchLuminance();
      error = null;
    } catch (err) {
      error = err.message;
    }
  }

  onMount(() => {
    refresh();
    const timer = setInterval(refresh, POLL_MS);
    return () => clearInterval(timer);
  });

  /** Uptime seconds as h:mm:ss, for "when was this point made". */
  function since(seconds) {
    const s = Math.max(0, Math.round(seconds));
    const h = Math.floor(s / 3600);
    const m = Math.floor((s % 3600) / 60);
    return `${h}:${String(m).padStart(2, '0')}:${String(s % 60).padStart(2, '0')}`;
  }

  /*
   * The chart is drawn in log light, because that is the axis the line is
   * straight in - plotting it against plain lux would show a curve and hide
   * the one thing worth seeing, which is whether the points sit on a line at
   * all. Three decades either side of the points, so a single reading does not
   * fill the whole width.
   */
  const W = 320;
  const H = 160;
  const PAD = 4;

  const range = $derived.by(() => {
    const xs = (data?.points ?? []).map((p) => Math.log10(Math.max(p.lux, 0.01)));
    if (data?.lux != null) xs.push(Math.log10(Math.max(data.lux, 0.01)));
    if (xs.length === 0) return { lo: -2, hi: 3 };
    const lo = Math.min(...xs) - 0.5;
    const hi = Math.max(...xs) + 0.5;
    return hi - lo < 1 ? { lo: lo - 0.5, hi: hi + 0.5 } : { lo, hi };
  });

  const xOf = (lux) => {
    const x = Math.log10(Math.max(lux, 0.01));
    return PAD + ((x - range.lo) / (range.hi - range.lo)) * (W - 2 * PAD);
  };
  // 0 % at the bottom, 100 % at the top.
  const yOf = (percent) => H - PAD - (percent / 100) * (H - 2 * PAD);

  /** The fitted line across the whole width, clamped the way the clock clamps. */
  const linePath = $derived.by(() => {
    if (!data) return '';
    const at = (logLux) => {
      const raw = data.slope * logLux + data.offset;
      return Math.min(data.maxPercent, Math.max(data.minPercent, raw));
    };
    const steps = 48;
    return Array.from({ length: steps + 1 }, (_, i) => {
      const logLux = range.lo + ((range.hi - range.lo) * i) / steps;
      const x = PAD + (i / steps) * (W - 2 * PAD);
      return `${i === 0 ? 'M' : 'L'}${x.toFixed(1)},${yOf(at(logLux)).toFixed(1)}`;
    }).join(' ');
  });
</script>

<section class="card">
  <h2>{t.lumTitle}</h2>
  <p class="hint">{t.lumHint}</p>

  {#if error}
    <p class="banner">{error}</p>
  {:else if !data}
    <p class="hint">{t.loading}</p>
  {:else}
    <div class="field">
      <span class="key">{t.measured}</span>
      <span>
        {#if data.available}
          {data.lux.toFixed(2)} lx <small class="raw">({data.raw.toFixed(2)} lx)</small>
          → {data.brightness} %
        {:else}
          {t.loadingShort}
        {/if}
      </span>
    </div>

    <div class="field">
      <span class="key">{t.lumLine}</span>
      <!-- Per decade, because that is the unit the slope is in: ten times the
           light, this many percent more face. -->
      <span>{data.slope.toFixed(1)} %/dec, {data.offset.toFixed(1)} % @ 1 lx</span>
    </div>

    <div class="field">
      <span class="key">{t.lumSlope}</span>
      <span>{data.fitted ? t.lumSlopeFitted : t.lumSlopeKept}</span>
    </div>

    {#if data.adjusting}
      <p class="hint">{t.lumAdjusting(data.wanted, Math.round(data.settleMs / 1000))}</p>
    {/if}

    <svg viewBox="0 0 {W} {H}" role="img" aria-label={t.lumTitle}>
      <path class="line" d={linePath} />
      {#each data.points as point, i (i)}
        <circle class="point" cx={xOf(point.lux)} cy={yOf(point.percent)} r="4" />
      {/each}
      {#if data.available}
        <line class="now" x1={xOf(data.lux)} y1={PAD} x2={xOf(data.lux)} y2={H - PAD} />
      {/if}
    </svg>

    <h3>{t.lumPoints} ({data.points.length}/{data.capacity})</h3>
    {#if data.points.length === 0}
      <p class="hint">{t.lumEmpty}</p>
    {:else}
      <div class="table" role="table">
        <div class="row head" role="row">
          <span>lx</span><span>{t.lumWanted}</span><span>{t.lumCurve}</span><span>{t.lumWhen}</span>
        </div>
        <!-- Keyed by position: two points can carry the same numbers. -->
        {#each data.points as point, i (i)}
          <div class="row" role="row">
            <span>{point.lux.toFixed(2)}</span>
            <span>{point.percent} %</span>
            <span class:off={point.curve !== point.percent}>{point.curve} %</span>
            <span>{since(point.seconds)}</span>
          </div>
        {/each}
      </div>
    {/if}
  {/if}
</section>

<style>
  svg {
    width: 100%;
    height: auto;
    margin: 0.8rem 0;
    border: 1px solid var(--border);
    border-radius: 7px;
    background: var(--surface);
  }

  .line {
    fill: none;
    stroke: var(--accent);
    stroke-width: 2;
  }

  .point {
    fill: var(--text);
  }

  /* Where the clock is right now, so a point can be compared against it. */
  .now {
    stroke: var(--muted);
    stroke-width: 1;
    stroke-dasharray: 3 3;
  }

  h3 {
    margin: 1rem 0 0.4rem;
    font-size: 0.95rem;
  }

  .table {
    font-variant-numeric: tabular-nums;
    font-size: 0.9rem;
  }

  .row {
    display: grid;
    grid-template-columns: repeat(4, minmax(0, 1fr));
    gap: 0.5rem;
    padding: 0.25rem 0;
    border-bottom: 1px solid var(--border);
  }

  .row.head {
    color: var(--muted);
    font-size: 0.8rem;
  }

  /* The fit does not go through every point, and seeing by how much is the
     whole reason this screen exists. */
  .off {
    color: var(--muted);
  }

  .raw {
    color: var(--muted);
  }
</style>
