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
  import { dict } from '../lib/i18n.svelte.js';
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

  onMount(async () => {
    try {
      sensor = await api.fetchLight();
    } catch {
      /* older firmware has no /light; treat that as no sensor */
    }
  });

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
  const ledColor = $derived(
    css(
      hsvRgb(clock.hue, clock.sat, 100).map(
        (part, i) => FACE[i] + (part - FACE[i]) * (clock.lum / 100)
      )
    )
  );
</script>

<section class="card">
  <h2>{t.colorTitle}</h2>

  <div class="picker">
    <div class="wheel" use:wheel></div>

    <div class="preview" style="--led: {ledColor}; --size: {WHEEL_SIZE}px">
      <!-- Keyed by position: two identical lines in some future translation
           would otherwise throw, the same way the network list did. -->
      {#each t.preview as line, i (i)}
        <span>{line}</span>
      {/each}
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

  <SliderRow
    id="lum"
    label={t.brightness}
    unit="%"
    bind:value={clock.lum}
    track={lumTrack}
    onchange={send}
  />
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
      <span>{sensor.available ? `${sensor.lux.toFixed(1)} lx` : t.loadingShort}</span>
    </div>

    <p class="hint">{t.ldrHint}</p>
  </section>
{/if}
