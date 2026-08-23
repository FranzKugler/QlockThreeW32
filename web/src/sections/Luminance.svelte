<script>
  /**
   * Luminance
   * Everything about the automatic brightness: what the clock was taught, what
   * it concluded, and how much of what its sensor sees is its own face.
   *
   * **One screen, two ways in, and the lock decides what it offers.** Unlocked
   * it is the eighth tab and the writes are there; locked it is still reachable
   * at #luminance and shows the same numbers read-only. That split is the whole
   * design: looking at the curve is what somebody does when the automatic feels
   * wrong, and a password in front of a diagnosis helps nobody - while editing
   * a curve, or throwing away a measurement that took twenty minutes on a
   * ladder, is a different act. The firmware draws the same line: GET
   * /luminance is open, POST /luminance is not.
   *
   * Two components would have been the other option, and would have been two
   * things to keep in step.
   *
   * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
   * @version  2.2
   * @created  22.8.2026
   * @updated  23.8.2026
   */
  import { onMount } from 'svelte';
  import * as api from '../lib/api.js';
  import { dict } from '../lib/i18n.svelte.js';

  const t = $derived(dict());

  // Passed by the shell rather than fetched: it already tracks the lock state
  // and refetches it on the way into #expert, and a second poller here would
  // only be a second answer to disagree with.
  let { expert = null } = $props();
  const editable = $derived(expert?.unlocked === true);

  let data = $state(null);
  let error = $state(null);
  let busy = $state(false);

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

  /**
   * Every write answers with the curve as it now stands, so the screen redraws
   * from what the clock says rather than from what it assumes happened. Guarded
   * against a second click while one is in flight: the points are addressed by
   * position, so two deletes racing would remove the wrong one.
   */
  async function write(action) {
    if (busy) return;
    busy = true;
    try {
      data = await action();
      error = null;
    } catch (err) {
      error = err.message;
    } finally {
      busy = false;
    }
  }

  const forget = (index) => write(() => api.forgetLightPoint(index));
  const forgetAll = () => write(() => api.resetLightPoints());

  // The coupling lives on /light, not /luminance - it is a property of the
  // sensor rather than of the curve - so this one has to refetch the curve
  // afterwards instead of taking the answer.
  const dropCoupling = () =>
    write(async () => {
      await api.resetCoupling();
      return api.fetchLuminance();
    });

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
  {#if !editable}
    <p class="hint">{t.lumReadOnly}</p>
  {/if}

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

    <!-- What the curve wants, and what the clock is actually showing. The two
         differ on purpose - during a nudge the clock shows the nudge, and
         afterwards it eases towards the curve by an eighth a second - and
         without both numbers there is no way to tell either apart from a
         fault. -->
    <div class="field">
      <span class="key">{t.lumApplied}</span>
      <span class:off={data.applied !== data.brightness}>{data.applied} %</span>
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

    <!-- How much of the raw reading was the clock's own face. Zero cells means
         nothing is being subtracted, which is a different thing from a dark
         face and has to read differently. -->
    <div class="field">
      <span class="key">{t.lumCoupling}</span>
      <span>
        {#if data.coupled > 0}
          {t.lumCoupledCells(data.coupled)}
          <small class="raw">
            — {t.lumDisplayShare(data.display.toFixed(2), data.raw.toFixed(2))}
          </small>
        {:else}
          <small class="raw">{t.lumCoupledNone}</small>
        {/if}
      </span>
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
      <div class="table" class:editable role="table">
        <div class="row head" role="row">
          <span>lx</span><span>{t.lumWanted}</span><span>{t.lumCurve}</span><span>{t.lumWhen}</span>
          {#if editable}<span></span>{/if}
        </div>
        <!-- Keyed by position: two points can carry the same numbers. -->
        {#each data.points as point, i (i)}
          <div class="row" role="row">
            <span>{point.lux.toFixed(2)}</span>
            <span>{point.percent} %</span>
            <span class:off={point.curve !== point.percent}>{point.curve} %</span>
            <span>{since(point.seconds)}</span>
            {#if editable}
              <span class="act">
                <button class="link" title={t.lumForgetTitle} disabled={busy}
                        onclick={() => forget(i)}>{t.lumForget}</button>
              </span>
            {/if}
          </div>
        {/each}
      </div>
    {/if}

    {#if editable}
      <div class="buttons">
        <button disabled={busy || data.points.length === 0} onclick={forgetAll}>
          {t.lumResetPoints}
        </button>
        <button disabled={busy || data.coupled === 0} onclick={dropCoupling}>
          {t.lumResetCoupling}
        </button>
      </div>
      {#if data.coupled > 0}
        <p class="hint">{t.lumResetCouplingHint}</p>
      {/if}
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

  /* The fifth column only exists while the writes are offered, so the header
     and the rows have to agree about it - a grid whose head has four columns
     and whose rows have five puts every label over the wrong number. */
  .table.editable .row {
    grid-template-columns: repeat(4, minmax(0, 1fr)) auto;
  }

  .row.head {
    color: var(--muted);
    font-size: 0.8rem;
  }

  .act {
    text-align: right;
  }

  /* A link rather than a button, because a delete per row drawn as five real
     buttons is the clutter the storage tab's context menu was built to remove -
     and here there is only ever one action, so a menu would be heavier still. */
  .link {
    padding: 0;
    border: none;
    background: none;
    color: var(--muted);
    font-size: 0.8rem;
    cursor: pointer;
    text-decoration: underline;
  }

  .link:hover:not(:disabled) {
    color: var(--danger);
  }

  .link:disabled {
    cursor: default;
    opacity: 0.5;
  }

  .buttons {
    display: flex;
    flex-wrap: wrap;
    gap: 0.5rem;
    margin-top: 1rem;
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
