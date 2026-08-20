<script>
  /**
   * Color
   * Colour tab: wheel for hue and saturation, sliders for hue, saturation and
   * brightness, and a preview of the lit clock face.
   *
   * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
   * @version  2.0
   * @created  15.8.2026
   * @updated  15.8.2026
   */
  import iro from '@jaames/iro';
  import { onMount } from 'svelte';
  import * as api from '../lib/api.js';
  import { throttle } from '../lib/throttle.js';
  import { cells } from '../lib/panel.js';
  import { dict } from '../lib/i18n.svelte.js';
  import { errorText } from '../lib/errors.js';
  import SliderRow from './SliderRow.svelte';

  // Aliased: a local binding called `state` would turn every `$state(...)` in
  // this component into a store subscription instead of the rune, and the
  // compiler only warns. See the note in CLAUDE.md.
  let { state: clock } = $props();

  const t = $derived(dict());

  // Null until asked, and left null when the clock cannot answer - in both
  // cases the automatic section stays hidden rather than appearing and then
  // vanishing again.
  let sensor = $state(null);

  /**
   * The face, polled separately from the sensor and more slowly: the letters
   * change every five minutes and the corners once a minute, so there is
   * nothing to gain from asking at the sensor's rate - and the clock's web
   * server answers one request at a time.
   */
  let panel = $state(null);
  const PANEL_POLL_MS = 5000;

  // Set when the clock refuses a calibration point, cleared on the next try.
  let calibrationError = $state(null);

  // Matches the sampling interval in the firmware, so the number moves as fast
  // as it can and no faster. Only while this tab is open: the clock's web
  // server is single-threaded and this is the only tab that needs live data.
  const POLL_MS = 2000;

  /*
   * What the brightness slider shows while the automatic is on: the level the
   * curve currently asks for, which is not clock.lum - that stays the manual
   * setting, to be given back when the automatic goes off again.
   */
  let autoLum = $state(0);

  // Polling must not yank the slider out from under a finger, nor undo an
  // adjustment that the next poll has not caught up with yet.
  let adjustedAt = 0;
  const SETTLE_MS = 2000;

  async function refresh() {
    try {
      const next = await api.fetchLight();
      sensor = next;
      if (Date.now() - adjustedAt > SETTLE_MS && next.brightness != null) {
        autoLum = next.brightness;
      }
    } catch {
      /* older firmware has no /light; treat that as no sensor */
    }
  }

  /*
   * Throttled like the colour wheel, and for the same reason: one POST per
   * pointer move would bury the clock's single-threaded web server. Each one
   * is measured against the curve as it stands, so a drag converges on the
   * value asked for rather than adding up shifts.
   */
  const pushNudge = throttle(async (want) => {
    const result = await api.nudgeBrightness(want);
    if (!result.error) sensor = result;
  }, 250);

  function nudge() {
    adjustedAt = Date.now();
    pushNudge(autoLum);
  }

  async function refreshPanel() {
    try {
      panel = await api.fetchPanel();
    } catch {
      /* older firmware has no /panel; the face stays empty rather than wrong */
    }
  }

  onMount(() => {
    refresh();
    refreshPanel();
    const timer = setInterval(refresh, POLL_MS);
    const faceTimer = setInterval(refreshPanel, PANEL_POLL_MS);
    return () => {
      clearInterval(timer);
      clearInterval(faceTimer);
    };
  });

  /**
   * Stores "this much light should look like this much display" for one end of
   * the curve, taking the light from the sensor and the brightness from the
   * slider - which is why this needs the automatic off: with it on, the slider
   * is not what the display is doing.
   */
  async function calibrate(end) {
    // The smoothed reading, not the raw one: that is what the curve is fed at
    // runtime, so calibrating against anything else builds in an offset. The
    // raw number is shown above for placing the sensor, which is a different
    // job.
    const here = sensor.lux;
    const point = end === 'low'
      ? { luxLow: here, brightLow: wantedNow, luxHigh: sensor.luxHigh, brightHigh: sensor.brightHigh }
      : { luxLow: sensor.luxLow, brightLow: sensor.brightLow, luxHigh: here, brightHigh: wantedNow };

    const result = await api.setLightCurve(point);
    calibrationError = result.error ?? null;
    if (!result.error) sensor = result;
  }

  /*
   * The two images are updated independently, so this UI can find itself
   * talking to a firmware from before the curve existed: it answers /light
   * with a reading and no curve at all. Everything below is gated on this
   * rather than on `present`, because `sensor.luxLow.toFixed()` on a missing
   * field would take the whole tab down.
   */
  const hasCurve = $derived(sensor?.luxLow != null);

  // Whether the ambient light is actually in charge of the display right now.
  const autoActive = $derived(sensor?.present && clock.automaticLum && hasCurve);

  // Whichever slider is on screen is the one a calibration point is taken
  // from - both mean "this is how bright I want it here".
  const wantedNow = $derived(autoActive ? autoLum : clock.lum);

  const canCalibrate = $derived(sensor?.available);

  async function resetCurve() {
    const result = await api.setLightCurve({ reset: true });
    calibrationError = result.error ?? null;
    if (!result.error) sensor = result;
  }

  const push = throttle((hue, sat, lum) => api.setColor({ hue, sat, lum }), 120);
  const send = () => push(clock.hue, clock.sat, clock.lum);

  let picker = null;
  // Set while we move the wheel ourselves, so its change event doesn't bounce
  // back into the slider that caused it.
  let applying = false;

  // Drives both the wheel and the preview square, so the two stay the same size.
  const WHEEL_SIZE = 200;

  // The wheel picks hue and saturation only. Brightness is a separate value in
  // the firmware (CHSV(hue, sat, 255), scaled afterwards), so it stays out of
  // the wheel: feeding it in as HSL lightness would wash the colour out towards
  // white and silently change the saturation.
  function wheel(node) {
    picker = new iro.ColorPicker(node, {
      width: WHEEL_SIZE,
      borderWidth: 2,
      borderColor: 'rgba(128,128,128,0.35)',
      layout: [{ component: iro.ui.Wheel }],
      color: { h: clock.hue, s: clock.sat, v: 100 }
    });

    const onChange = (color) => {
      if (applying) return;
      clock.hue = Math.round(color.hsv.h);
      clock.sat = Math.round(color.hsv.s);
      send();
    };
    picker.on('color:change', onChange);

    return {
      destroy() {
        picker.off('color:change', onChange);
        picker = null;
      }
    };
  }

  /** Moves the wheel handle after a slider changed hue or saturation. */
  function syncWheel() {
    if (!picker) return;
    applying = true;
    picker.color.set({ h: clock.hue, s: clock.sat, v: 100 });
    applying = false;
  }

  function onWheelValueChange() {
    syncWheel();
    send();
  }

  /**
   * Same model the firmware uses: CHSV(hue, sat, 255) scaled by brightness,
   * which is a plain per-channel multiply - so brightness maps onto HSV value.
   */
  function hsvRgb(h, s, v) {
    const S = s / 100;
    const V = v / 100;
    const c = V * S;
    const x = c * (1 - Math.abs(((h / 60) % 2) - 1));
    const m = V - c;
    let rgb;
    if (h < 60) rgb = [c, x, 0];
    else if (h < 120) rgb = [x, c, 0];
    else if (h < 180) rgb = [0, c, x];
    else if (h < 240) rgb = [0, x, c];
    else if (h < 300) rgb = [x, 0, c];
    else rgb = [c, 0, x];
    return rgb.map((part) => (part + m) * 255);
  }

  const css = (rgb) => `rgb(${rgb.map(Math.round).join(' ')})`;

  /**
   * The clock's face, which unlit letters blend into. Must match .preview's
   * background in app.css - that is the light theme's --bg, written out there
   * for the same reason it is written out here: a physical clock face stays
   * pale whichever theme the browser is in.
   */
  const FACE = [244, 245, 247];

  const HUE_TRACK =
    'linear-gradient(to right, #f00 0%, #ff0 17%, #0f0 33%, #0ff 50%, #00f 67%, #f0f 83%, #f00 100%)';

  // Saturation runs from white to the pure hue; brightness from black to the
  // colour at full brightness.
  const satTrack = $derived(
    `linear-gradient(to right, ${css(hsvRgb(clock.hue, 0, 100))}, ${css(hsvRgb(clock.hue, 100, 100))})`
  );
  const lumTrack = $derived(
    `linear-gradient(to right, #000, ${css(hsvRgb(clock.hue, clock.sat, 100))})`
  );

  /**
   * How a lit letter reads against the face: blended from the unlit face colour
   * towards the full colour as brightness rises, so at 0 % the text disappears
   * into the face the way dark LEDs do. A straight additive mix would wash
   * every colour out to near-white against a face this light.
   */
  const face = $derived(cells(panel));

  const ledColor = $derived(
    css(
      hsvRgb(clock.hue, clock.sat, 100).map(
        (part, i) => FACE[i] + (part - FACE[i]) * (wantedNow / 100)
      )
    )
  );
</script>

<section class="card">
  <h2>{t.colorTitle}</h2>

  <div class="picker">
    <div class="wheel" use:wheel></div>

    <!--
      The real face, not a mock-up of one: the letters are the panel of the
      language that is running and the lit ones are read off the clock's frame
      buffer. What is on screen here is what is on the wall, a wrong render
      included - which is the point of not rebuilding the grammar in JavaScript.
    -->
    <div
      class="preview"
      style="--led: {ledColor}; --size: {WHEEL_SIZE}px"
      role="img"
      aria-label={panel?.text || t.colorTitle}
    >
      <span class="corner tl" class:on={panel?.corners?.[0]}></span>
      <span class="corner tr" class:on={panel?.corners?.[1]}></span>
      <span class="corner br" class:on={panel?.corners?.[2]}></span>
      <span class="corner bl" class:on={panel?.corners?.[3]}></span>

      <!-- Keyed by position, not by letter: a panel carries the same letter
           many times over and a keyed each would throw on the repeat. -->
      <div class="letters" aria-hidden="true">
        {#each face as cell (cell.key)}
          <span class:on={cell.on}>{cell.letter}</span>
        {/each}
      </div>
    </div>
  </div>

  <SliderRow
    id="hue"
    label={t.hue}
    unit="°"
    min={0}
    max={359}
    stepSize={2}
    wrap
    bind:value={clock.hue}
    track={HUE_TRACK}
    onchange={onWheelValueChange}
  />

  <SliderRow
    id="sat"
    label={t.saturation}
    unit="%"
    bind:value={clock.sat}
    track={satTrack}
    onchange={onWheelValueChange}
  />

  <!--
    Two meanings, one control. With the automatic off it is the brightness.
    With it on it is "at this light, I want this much" - the clock shifts its
    whole curve to match and keeps the preference at every other light level.
    Which is also the only way it can ever learn anything: nudging the slider
    is the one signal that says the automatic got it wrong.
  -->
  {#if autoActive}
    <SliderRow
      id="lum"
      label={t.brightness}
      unit="%"
      bind:value={autoLum}
      track={lumTrack}
      onchange={nudge}
    />
  {:else}
    <SliderRow
      id="lum"
      label={t.brightness}
      unit="%"
      bind:value={clock.lum}
      track={lumTrack}
      onchange={send}
    />
  {/if}
</section>

<!--
  Only shown when the clock has a light sensor. Most do not: the firmware is
  the same for every build of the clock and the sensor is an addition, so a
  switch that cannot do anything would be worse than no switch at all.
-->
{#if sensor?.present}
  <section class="card">
    <h2>{t.automatic}</h2>

    <div class="field">
      <label for="autoLum">{t.autoBrightness}</label>
      <span class="switch">
        <input
          id="autoLum"
          type="checkbox"
          bind:checked={clock.automaticLum}
          onchange={() => api.setAutoLuminance(clock.automaticLum)}
        />
        <span></span>
      </span>
    </div>

    <div class="field">
      <span class="key">{t.measured}</span>
      <span>
        {#if sensor.available}
          {sensor.lux.toFixed(1)} lx
          <!-- The unsmoothed reading beside it: moving the sensor around
               behind the panel is only possible with a number that reacts. -->
          <small class="raw">({sensor.raw.toFixed(1)} lx)</small>
        {:else}
          {t.loadingShort}
        {/if}
      </span>
    </div>

    {#if hasCurve}
      <h3>{t.calibration}</h3>

      <div class="field">
        <span class="key">{t.calDark}</span>
        <span class="point">
          {sensor.luxLow.toFixed(1)} lx → {sensor.brightLow} %
          <button type="button" class="secondary" onclick={() => calibrate('low')} disabled={!canCalibrate}>
            {t.calCapture}
          </button>
        </span>
      </div>

      <div class="field">
        <span class="key">{t.calBright}</span>
        <span class="point">
          {sensor.luxHigh.toFixed(1)} lx → {sensor.brightHigh} %
          <button type="button" class="secondary" onclick={() => calibrate('high')} disabled={!canCalibrate}>
            {t.calCapture}
          </button>
        </span>
      </div>

      {#if calibrationError}
        <p class="hint error">{errorText(t, calibrationError, '')}</p>
      {/if}

      <p class="hint">{t.calHint}</p>

      <button type="button" class="secondary" onclick={resetCurve}>{t.calReset}</button>
    {/if}

    <p class="hint">{t.ldrHint}</p>
  </section>
{/if}

<style>
  h3 {
    margin: 1.4rem 0 0.2rem;
    font-size: 0.95rem;
  }
  .raw {
    opacity: 0.6;
  }
  .point {
    display: flex;
    align-items: center;
    gap: 0.6rem;
  }
  .error {
    color: var(--danger, #c0392b);
  }
</style>
