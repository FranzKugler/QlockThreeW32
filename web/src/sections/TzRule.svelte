<script>
  /**
   * TzRule
   * One timezone changeover rule, mirroring a TimeChangeRule in the firmware:
   * "on the <week> <day> of <month> at <hour>, switch to <offset> minutes".
   *
   * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
   * @version  2.0
   * @created  15.8.2026
   * @updated  15.8.2026
   */
  import { dict } from '../lib/i18n.svelte.js';

  let {
    // Prefix for the field ids; separate from the title, which is translated.
    id,
    title,
    name = $bindable(),
    week = $bindable(),
    dow = $bindable(),
    month = $bindable(),
    hour = $bindable(),
    offset = $bindable(),
    // Whole rule is irrelevant (no DST configured at all).
    disabled = false,
    // Only the changeover moment is irrelevant, the offset still applies.
    scheduleDisabled = false,
    onchange
  } = $props();

  const t = $derived(dict());

  const HOURS = Array.from({ length: 24 }, (_, h) => h);

  const scheduleOff = $derived(disabled || scheduleDisabled);
</script>

<div class="rule">
  <h3>{title}</h3>
  <div class="grid">
    <div>
      <label for="{id}-name">{t.abbreviation}</label>
      <input
        id="{id}-name"
        type="text"
        maxlength="9"
        bind:value={name}
        {disabled}
        {onchange}
      />
    </div>
    <div>
      <label for="{id}-week">{t.week}</label>
      <select id="{id}-week" bind:value={week} disabled={scheduleOff} {onchange}>
        {#each t.weeks as label, value (value)}
          <option {value}>{label}</option>
        {/each}
      </select>
    </div>
    <div>
      <label for="{id}-dow">{t.day}</label>
      <select id="{id}-dow" bind:value={dow} disabled={scheduleOff} {onchange}>
        {#each t.days as label, i (i)}
          <option value={i + 1}>{label}</option>
        {/each}
      </select>
    </div>
    <div>
      <label for="{id}-month">{t.month}</label>
      <select id="{id}-month" bind:value={month} disabled={scheduleOff} {onchange}>
        {#each t.months as label, i (i)}
          <option value={i + 1}>{label}</option>
        {/each}
      </select>
    </div>
    <div>
      <label for="{id}-hour">{t.hour}</label>
      <select id="{id}-hour" bind:value={hour} disabled={scheduleOff} {onchange}>
        {#each HOURS as h (h)}
          <option value={h}>{h}</option>
        {/each}
      </select>
    </div>
    <div>
      <label for="{id}-offset">{t.offsetMin}</label>
      <input
        id="{id}-offset"
        type="number"
        step="15"
        bind:value={offset}
        {disabled}
        {onchange}
      />
    </div>
  </div>
</div>
