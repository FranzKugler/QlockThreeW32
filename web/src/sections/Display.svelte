<script>
  import * as api from '../lib/api.js';

  let { state } = $props();

  // Values match the STD_MODE_* / EXT_MODE_* defines in src/main .cpp.
  const MODES = [
    { value: 1, label: 'Uhrzeit' },
    { value: 6, label: 'Uhrzeit mit WiFi-Status' },
    { value: 0, label: 'Aus (dunkel)' },
    { value: 2, label: 'Sekunden' },
    { value: 3, label: 'Test' },
    { value: 4, label: 'Status' }
  ];

  // Values match the LANGUAGE_* defines in src/Renderer.h.
  const LANGUAGES = [
    'Deutsch',
    'Schwäbisch',
    'Bayrisch',
    'Sächsisch',
    'Schweizerisch',
    'Englisch',
    'Französisch',
    'Italienisch',
    'Niederländisch',
    'Spanisch'
  ];

  const pushConfiguration = () =>
    api.setConfiguration({
      language: state.language,
      cornerColor: state.cornerColor,
      cornerDirection: state.cornerDirection
    });
</script>

<section class="card">
  <h2>Anzeige</h2>
  {#each MODES as mode (mode.value)}
    <label class="choice">
      <input
        type="radio"
        name="display"
        value={mode.value}
        bind:group={state.display}
        onchange={() => api.setDisplay(state.display)}
      />
      {mode.label}
    </label>
  {/each}
</section>

<section class="card">
  <h2>Darstellung</h2>

  <div class="field">
    <label for="language">Sprache</label>
    <select id="language" bind:value={state.language} onchange={pushConfiguration}>
      {#each LANGUAGES as label, value (value)}
        <option {value}>{label}</option>
      {/each}
    </select>
  </div>

  <div class="field">
    <label for="cornerDirection">Ecken</label>
    <select
      id="cornerDirection"
      bind:value={state.cornerDirection}
      onchange={pushConfiguration}
    >
      <option value={1}>im Uhrzeigersinn</option>
      <option value={0}>gegen den Uhrzeigersinn</option>
    </select>
  </div>

  <div class="field">
    <label for="cornerColor">Minuten</label>
    <select id="cornerColor" bind:value={state.cornerColor} onchange={pushConfiguration}>
      <option value={0}>monochrom</option>
      <option value={1}>farbig</option>
    </select>
  </div>
</section>
