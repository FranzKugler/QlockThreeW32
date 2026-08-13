<script>
  import iro from '@jaames/iro';
  import * as api from '../lib/api.js';
  import { throttle } from '../lib/throttle.js';

  let { state } = $props();

  // The firmware stores hue 0-359, saturation 0-100 and lum 0-100, where lum
  // doubles as the LED brightness — same mapping the old iro.js UI used.
  const push = throttle(
    (hue, sat, lum) => api.setColor({ hue, sat, lum }),
    120
  );

  function wheel(node) {
    const picker = new iro.ColorPicker(node, {
      width: 220,
      borderWidth: 2,
      borderColor: 'rgba(128,128,128,0.35)',
      wheelLightness: false,
      color: { h: state.hue, s: state.sat, l: state.lum }
    });

    // iro's off() looks the handler up by identity, so keep a reference.
    const onChange = (color) => {
      const { h, s, l } = color.hsl;
      state.hue = Math.round(h);
      state.sat = Math.round(s);
      state.lum = Math.round(l);
      push(state.hue, state.sat, state.lum);
    };
    picker.on('color:change', onChange);

    return {
      destroy() {
        picker.off('color:change', onChange);
      }
    };
  }
</script>

<section class="card">
  <h2>Farbe</h2>

  <div class="wheel" use:wheel></div>

  <div class="readout">
    <span>Farbton {state.hue}°</span>
    <span>Sättigung {state.sat}%</span>
    <span>Helligkeit {state.lum}%</span>
  </div>
</section>

<section class="card">
  <h2>Helligkeit</h2>

  <div class="field">
    <label for="autoLum">Automatik</label>
    <span class="switch">
      <input
        id="autoLum"
        type="checkbox"
        bind:checked={state.automaticLum}
        onchange={() => api.setAutoLuminance(state.automaticLum)}
      />
      <span></span>
    </span>
  </div>

  <p class="hint">
    Ohne Wirkung, solange die LDR-Auswertung in der Firmware auskommentiert ist
    (siehe src/LDR.cpp).
  </p>
</section>
