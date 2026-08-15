<script>
  /**
   * TzRule
   * One timezone changeover rule, mirroring a TimeChangeRule in the firmware:
   * "on the <week> <day> of <month> at <hour>, switch to <offset> minutes".
   *
   * @autor    Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
   * @version  2.0
   * @created  15.8.2026
   * @updated  15.8.2026
   */
  let {
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

  const WEEKS = [
    { value: 0, label: 'Letzter' },
    { value: 1, label: '1.' },
    { value: 2, label: '2.' },
    { value: 3, label: '3.' },
    { value: 4, label: '4.' }
  ];

  // 1 = Sunday, matching the Timezone library's day-of-week numbering.
  const DAYS = ['So.', 'Mo.', 'Di.', 'Mi.', 'Do.', 'Fr.', 'Sa.'];

  const MONTHS = [
    'Jan.', 'Feb.', 'Mär.', 'Apr.', 'Mai', 'Jun.',
    'Jul.', 'Aug.', 'Sep.', 'Okt.', 'Nov.', 'Dez.'
  ];

  const HOURS = Array.from({ length: 24 }, (_, h) => h);

  const scheduleOff = $derived(disabled || scheduleDisabled);
</script>

<div class="rule">
  <h3>{title}</h3>
  <div class="grid">
    <div>
      <label for="{title}-name">Kürzel</label>
      <input
        id="{title}-name"
        type="text"
        maxlength="9"
        bind:value={name}
        {disabled}
        {onchange}
      />
    </div>
    <div>
      <label for="{title}-week">Woche</label>
      <select id="{title}-week" bind:value={week} disabled={scheduleOff} {onchange}>
        {#each WEEKS as w (w.value)}
          <option value={w.value}>{w.label}</option>
        {/each}
      </select>
    </div>
    <div>
      <label for="{title}-dow">Tag</label>
      <select id="{title}-dow" bind:value={dow} disabled={scheduleOff} {onchange}>
        {#each DAYS as label, i (i)}
          <option value={i + 1}>{label}</option>
        {/each}
      </select>
    </div>
    <div>
      <label for="{title}-month">Monat</label>
      <select id="{title}-month" bind:value={month} disabled={scheduleOff} {onchange}>
        {#each MONTHS as label, i (i)}
          <option value={i + 1}>{label}</option>
        {/each}
      </select>
    </div>
    <div>
      <label for="{title}-hour">Stunde</label>
      <select id="{title}-hour" bind:value={hour} disabled={scheduleOff} {onchange}>
        {#each HOURS as h (h)}
          <option value={h}>{h}</option>
        {/each}
      </select>
    </div>
    <div>
      <label for="{title}-offset">Offset (min)</label>
      <input
        id="{title}-offset"
        type="number"
        step="15"
        bind:value={offset}
        {disabled}
        {onchange}
      />
    </div>
  </div>
</div>
