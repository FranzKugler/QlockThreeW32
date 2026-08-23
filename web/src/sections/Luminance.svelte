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
  import { errorText } from '../lib/errors.js';

  // Calibration::Phase in the firmware. Only the one that has to be told apart
  // from the others is named here; the rest are looked up by index in
  // t.lumCalibratePhases, which keeps the two lists the same length by
  // construction rather than by discipline.
  const PHASE_DONE = 7;

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

  /**
   * The regulated range. Held locally while being typed, because the poll
   * lands once a second and would otherwise pull a half-typed number out from
   * under the cursor; committed on change, which is when the field is left or
   * the spinner is clicked.
   */
  let lowEdit = $state(null);
  let highEdit = $state(null);
  const low = $derived(lowEdit ?? data?.minPercent ?? 20);
  const high = $derived(highEdit ?? data?.maxPercent ?? 100);

  function commitRange() {
    const wantLow = Number(low);
    const wantHigh = Number(high);
    lowEdit = null;
    highEdit = null;
    if (!data || (wantLow === data.minPercent && wantHigh === data.maxPercent)) return;
    write(() => api.setLightRange(wantLow, wantHigh));
  }

  /**
   * The self-calibration. Both writes answer with /light, whose shape is not
   * this screen's, so the curve is refetched afterwards - and the poll a second
   * later would pick the progress up anyway. Refetching immediately is what
   * makes the button feel like it did something.
   */
  const calibrate = () =>
    write(async () => {
      await api.startCalibration();
      return api.fetchLuminance();
    });

  const abortCalibration = () =>
    write(async () => {
      await api.abortCalibration();
      return api.fetchLuminance();
    });

  // Idle, done and failed are all "not running"; only the middle phases carry
  // a bar. Guarded against an older firmware with no calibration block at all.
  const calib = $derived(data?.calibration ?? null);
  const calibrating = $derived(calib?.running === true);
  const calibPercent = $derived(
    !calib || !calib.total ? 0 : Math.min(100, Math.round((calib.done / calib.total) * 100))
  );
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
   * straight in - against plain lux it would be a curve, hiding the one thing
   * worth seeing, which is whether the points sit on a line at all.
   *
   * The margins are not decoration. Without a scale a reader can see that the
   * points are scattered but not by how much, and the difference between "10 %
   * out" and "40 % out" is the difference between a curve worth keeping and
   * one worth throwing away.
   */
  const W = 340;
  const H = 200;
  const PAD_L = 30;   // room for "100 %"
  const PAD_R = 8;
  const PAD_T = 10;
  const PAD_B = 24;   // room for the decade labels

  const PLOT_W = W - PAD_L - PAD_R;
  const PLOT_H = H - PAD_T - PAD_B;

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
    return PAD_L + ((x - range.lo) / (range.hi - range.lo)) * PLOT_W;
  };
  const xOfLog = (x) => PAD_L + ((x - range.lo) / (range.hi - range.lo)) * PLOT_W;
  // 0 % at the bottom, 100 % at the top.
  const yOf = (percent) => PAD_T + PLOT_H - (percent / 100) * PLOT_H;

  /*
   * Nine ticks per decade - 1, 2, 3 ... 9 - because their uneven spacing is
   * what makes the axis legible as a log axis at a glance. Whole decades were
   * the first attempt and read as a linear axis with odd numbers on it.
   *
   * Labels are thinned by distance rather than by rule: a decade is always
   * labelled, and anything else only when it clears the last label already
   * placed. So the sparse left-hand end of a decade gets 0.01, 0.02, 0.03 and
   * the crowded right-hand end quietly drops to 0.06, 0.08 - without anybody
   * having to decide in advance which numbers those are, which changes with
   * the width of the range on screen.
   */
  const MIN_LABEL_PX = 24;

  const ticks = $derived.by(() => {
    const out = [];
    for (let k = Math.floor(range.lo); k <= Math.ceil(range.hi); k++) {
      for (let m = 1; m <= 9; m++) {
        const value = Number((m * 10 ** k).toPrecision(3));
        const at = Math.log10(value);
        if (at < range.lo || at > range.hi) continue;
        out.push({ value, x: xOfLog(at), decade: m === 1, label: m === 1,
                   text: format(value) });
      }
    }

    // Two passes, and the order matters. The decades claim their labels first;
    // the minor ticks then fill whatever is left, each one measured against
    // every label already placed rather than only against the one before it.
    // Done in a single left-to-right pass, a decade would shoulder its way in
    // 19 px after a minor label and the two would overlap - which is what the
    // first version did, at exactly the place a reader looks first.
    const placed = out.filter((tick) => tick.decade).map((tick) => tick.x);
    for (const tick of out) {
      if (tick.decade) continue;
      if (placed.every((x) => Math.abs(tick.x - x) >= MIN_LABEL_PX)) {
        tick.label = true;
        placed.push(tick.x);
      }
    }
    return out;
  });

  /** 0.01, 0.06, 1, 20 - as many decimals as the value needs and no more. */
  function format(value) {
    if (value >= 1) return String(Math.round(value));
    return value.toFixed(Math.max(0, -Math.floor(Math.log10(value))));
  }

  /*
   * The point the line is pinned to. Marked, because the chart is otherwise
   * unreadable without knowing which one it is - and because with the table
   * sorted by light there is no other way to tell.
   */
  const newest = $derived(
    data?.points?.length ? data.points[data.points.length - 1] : null
  );

  /*
   * The points by light rather than by when they were made, which is the order
   * they appear in on the chart above and therefore the only order in which
   * the two can be read together. `at` keeps each point's real position,
   * because that is what forgetting one addresses it by.
   */
  const byLight = $derived(
    (data?.points ?? [])
      .map((point, at) => ({ point, at, last: at === data.points.length - 1 }))
      .sort((a, b) => a.point.lux - b.point.lux)
  );

  const levels = [0, 20, 40, 60, 80, 100];

  /**
   * The fitted line across the whole width, clamped the way the clock clamps.
   *
   * The clamping is the point of drawing it this way: between the two ends it
   * is a straight line in log light, and outside them it is flat, so the shape
   * on screen is what the clock will actually do rather than what the formula
   * says. Sampled rather than drawn as three segments - the corners then land
   * wherever they land instead of having to be solved for.
   */
  const linePath = $derived.by(() => {
    if (!data) return '';
    const at = (logLux) => {
      const raw = data.slope * logLux + data.offset;
      return Math.min(data.maxPercent, Math.max(data.minPercent, raw));
    };
    const steps = 96;
    return Array.from({ length: steps + 1 }, (_, i) => {
      const logLux = range.lo + ((range.hi - range.lo) * i) / steps;
      const x = PAD_L + (i / steps) * PLOT_W;
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
      <!-- Brightness, every twenty per cent. -->
      {#each levels as level (level)}
        <line class="grid" x1={PAD_L} y1={yOf(level)} x2={W - PAD_R} y2={yOf(level)} />
        <text class="tick y" x={PAD_L - 5} y={yOf(level) + 3}>{level}</text>
      {/each}

      <!-- Light, nine to the decade. The minor lines are fainter, or the
           unevenness reads as a mistake rather than as the scale. -->
      {#each ticks as tick (tick.value)}
        <line class="grid" class:minor={!tick.decade}
              x1={tick.x} y1={PAD_T} x2={tick.x} y2={PAD_T + PLOT_H} />
        {#if tick.label}
          <text class="tick x" x={tick.x} y={H - 10}>{tick.text}</text>
        {/if}
      {/each}

      <!-- Where the clock stops following the line. Drawn, because the flat
           ends of the curve are otherwise indistinguishable from a curve that
           happens to be flat there. -->
      <line class="clamp" x1={PAD_L} y1={yOf(data.minPercent)} x2={W - PAD_R} y2={yOf(data.minPercent)} />
      <line class="clamp" x1={PAD_L} y1={yOf(data.maxPercent)} x2={W - PAD_R} y2={yOf(data.maxPercent)} />

      <path class="line" d={linePath} />

      {#each data.points as point, i (i)}
        <circle class="point" cx={xOf(point.lux)} cy={yOf(point.percent)} r="4" />
      {/each}
      {#if newest}
        <circle class="anchor" cx={xOf(newest.lux)} cy={yOf(newest.percent)} r="7" />
      {/if}

      {#if data.available}
        <line class="now" x1={xOf(data.lux)} y1={PAD_T} x2={xOf(data.lux)} y2={PAD_T + PLOT_H} />
      {/if}

      <text class="axis x" x={PAD_L + PLOT_W / 2} y={H - 1}>lx</text>
      <text class="axis y" x={4} y={PAD_T + 4}>%</text>
    </svg>

    <h3>{t.lumPoints} ({data.points.length}/{data.capacity})</h3>
    <!-- Said out loud, because the chart looks like a bad least-squares fit
         and is not one: the line is pinned to the newest point on purpose, and
         someone reading it without knowing that reasonably concludes the
         arithmetic is broken. -->
    {#if data.points.length > 1}
      <p class="hint">{t.lumAnchor}</p>
    {/if}
    {#if data.points.length === 0}
      <p class="hint">{t.lumEmpty}</p>
    {:else}
      <div class="table" class:editable role="table">
        <div class="row head" role="row">
          <span>lx</span><span>{t.lumWanted}</span><span>{t.lumCurve}</span><span>{t.lumWhen}</span>
          {#if editable}<span></span>{/if}
        </div>
        <!-- Keyed by the point's real position, which is also what forgetting
             one addresses it by - the row order here is the chart's, not the
             clock's. -->
        {#each byLight as row (row.at)}
          <div class="row" role="row" class:anchored={row.last}>
            <span>{row.point.lux.toFixed(row.point.lux < 1 ? 3 : 2)}</span>
            <span>
              {row.point.percent} %{#if row.last}<span class="mark" title={t.lumNewest}>•</span>{/if}
            </span>
            <span class:off={row.point.curve !== row.point.percent}>{row.point.curve} %</span>
            <span>{since(row.point.seconds)}</span>
            {#if editable}
              <span class="act">
                <button class="link" title={t.lumForgetTitle} disabled={busy}
                        onclick={() => forget(row.at)}>{t.lumForget}</button>
              </span>
            {/if}
          </div>
        {/each}
      </div>
    {/if}

    {#if editable}
      <h3>{t.lumRange}</h3>
      <p class="hint">{t.lumRangeHint}</p>
      <div class="range">
        <label>
          {t.lumRangeMin}
          <input type="number" min="1" max="100" disabled={busy || calibrating}
                 value={low} oninput={(e) => (lowEdit = e.currentTarget.value)}
                 onchange={commitRange} />
          %
        </label>
        <label>
          {t.lumRangeMax}
          <input type="number" min="1" max="100" disabled={busy || calibrating}
                 value={high} oninput={(e) => (highEdit = e.currentTarget.value)}
                 onchange={commitRange} />
          %
        </label>
      </div>

      <h3>{t.lumCoupling}</h3>

      <!-- The clock measuring its own map, with no script and no laptop. The
           progress comes from the same poll as everything else on this screen,
           so there is nothing here that keeps its own idea of what is going
           on. -->
      {#if calibrating}
        <p class="hint">{t.lumCalibratePhases[calib.phase] ?? ''}</p>
        <div class="bar"><div class="fill" style="width: {calibPercent}%"></div></div>
        <div class="buttons">
          <button disabled={busy} onclick={abortCalibration}>{t.lumCalibrateAbort}</button>
        </div>
      {:else}
        <p class="hint">{t.lumCalibrateHint(calib?.maxAmbient ?? 1)}</p>
        {#if calib?.error}
          <!-- Kept on screen after the run rather than cleared: a calibration
               that refuses because the room is lit has to say so for longer
               than the second between two polls. -->
          <p class="banner">
            {errorText(t, calib.error) ?? calib.error}
            {#if calib.error === 'calibTooBright'}
              — {t.lumCalibrateAmbient(calib.ambient.toFixed(2))}
            {/if}
          </p>
        {:else if calib?.phase === PHASE_DONE}
          <p class="hint">{t.lumCalibrateResult(calib.kept, calib.rung)}</p>
        {/if}
        <div class="buttons">
          <button disabled={busy} onclick={calibrate}>{t.lumCalibrate}</button>
        </div>
      {/if}

      <div class="buttons">
        <button disabled={busy || calibrating || data.points.length === 0} onclick={forgetAll}>
          {t.lumResetPoints}
        </button>
        <button disabled={busy || calibrating || data.coupled === 0} onclick={dropCoupling}>
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

  .grid {
    stroke: var(--border);
    stroke-width: 1;
  }

  .grid.minor {
    opacity: 0.45;
  }

  /* The point the line is pinned to. A ring rather than a bigger dot, so the
     point itself stays where it is and the mark reads as a mark. */
  .anchor {
    fill: none;
    stroke: var(--accent);
    stroke-width: 1.5;
  }

  /* The two ends of the regulated range. Dotted rather than dashed, to stay
     apart from the dashed line marking the light right now. */
  .clamp {
    stroke: var(--muted);
    stroke-width: 1;
    stroke-dasharray: 1 3;
  }

  .tick {
    fill: var(--muted);
    font-size: 9px;
  }

  .tick.y {
    text-anchor: end;
  }

  .tick.x {
    text-anchor: middle;
  }

  .axis {
    fill: var(--muted);
    font-size: 9px;
    text-anchor: middle;
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

  .mark {
    margin-left: 0.3rem;
    color: var(--accent);
  }

  .row.anchored {
    font-weight: 600;
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

  .range {
    display: flex;
    flex-wrap: wrap;
    gap: 1rem;
    margin: 0.5rem 0 0.2rem;
  }

  .range label {
    display: flex;
    align-items: center;
    gap: 0.4rem;
    font-size: 0.9rem;
  }

  .range input {
    width: 4.5rem;
  }

  .buttons {
    display: flex;
    flex-wrap: wrap;
    gap: 0.5rem;
    margin-top: 1rem;
  }

  .bar {
    height: 0.5rem;
    margin: 0.5rem 0;
    border-radius: 999px;
    background: var(--border);
    overflow: hidden;
  }

  .fill {
    height: 100%;
    background: var(--accent);
    transition: width 0.3s linear;
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
