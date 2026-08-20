<script>
  /**
   * Display
   * Display tab: operating mode, language and the corner LED options.
   *
   * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
   * @version  2.0
   * @created  15.8.2026
   * @updated  15.8.2026
   */
  import * as api from '../lib/api.js';
  import { dict, languages } from '../lib/i18n.svelte.js';

  let { state, expert } = $props();

  const t = $derived(dict());

  // Values match the STD_MODE_* / EXT_MODE_* defines in src/main .cpp; the
  // labels are t.modes in the same order. 4 was an uptime counter left over
  // from the AVR and DCF77 days and is gone; the firmware falls back to normal
  // display if it still finds it stored.
  const MODE_VALUES = [1, 6, 0, 2, 3];

  /**
   * The languages this clock can actually show.
   *
   * A clock has one panel of milled letters and no setting changes that, so
   * outside expert mode only the languages cut into the same sheet are on
   * offer - the four German dialects share one, every other language is alone
   * on its own. Where that leaves a single choice there is nothing to choose,
   * and the field goes away rather than sitting there disabled.
   *
   * The firmware refuses the others as well (POST /configuration); this only
   * keeps the UI from offering what it would refuse.
   */
  const offered = $derived.by(() => {
    const all = languages();
    if (expert?.unlocked) return all;

    const current = all.find((l) => l.value === state.language);
    // An unknown language, or a firmware that does not report panels: say
    // nothing about what fits and offer everything, as before.
    if (!current || current.panel === undefined) return all;

    return all.filter((l) => l.panel === current.panel);
  });

  const pushConfiguration = () =>
    api.setConfiguration({
      language: state.language,
      cornerColor: state.cornerColor,
      cornerDirection: state.cornerDirection
    });
</script>

<section class="card">
  <h2>{t.displayTitle}</h2>
  {#each MODE_VALUES as value, i (value)}
    <label class="choice">
      <input
        type="radio"
        name="display"
        {value}
        bind:group={state.display}
        onchange={() => api.setDisplay(state.display)}
      />
      {t.modes[i]}
    </label>
  {/each}
</section>

<section class="card">
  <h2>{t.appearance}</h2>

  {#if offered.length > 1}
  <div class="field">
    <label for="language">{t.language}</label>
    <select id="language" bind:value={state.language} onchange={pushConfiguration}>
      <!-- The clock says which languages it has and what they are called,
           in their own language. Keyed by the stored number, which is what
           makes it safe that the names are not translated. -->
      {#each offered as language (language.value)}
        <option value={language.value}>{language.name}</option>
      {/each}
    </select>
  </div>
  {/if}

  <div class="field">
    <label for="cornerDirection">{t.corners}</label>
    <select
      id="cornerDirection"
      bind:value={state.cornerDirection}
      onchange={pushConfiguration}
    >
      <option value={1}>{t.clockwise}</option>
      <option value={0}>{t.counterClockwise}</option>
    </select>
  </div>

  <div class="field">
    <label for="cornerColor">{t.minutes}</label>
    <select id="cornerColor" bind:value={state.cornerColor} onchange={pushConfiguration}>
      <option value={0}>{t.monochrome}</option>
      <option value={1}>{t.colored}</option>
    </select>
  </div>
</section>
