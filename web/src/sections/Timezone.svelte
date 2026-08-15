<script>
  /**
   * Timezone
   * Timezone tab: NTP server and both changeover rules of the clock.
   *
   * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
   * @version  2.0
   * @created  15.8.2026
   * @updated  15.8.2026
   */
  import * as api from '../lib/api.js';
  import { dict } from '../lib/i18n.svelte.js';
  import TzRule from './TzRule.svelte';

  let { state } = $props();

  const t = $derived(dict());

  // Every change posts the complete timezone object, as the firmware rebuilds
  // both TimeChangeRules from it in one go.
  const push = () =>
    api.setTimezone({
      ntpServer: state.ntpServer,
      useDs: state.useDs,
      tzName: state.tzName,
      tzWeek: state.tzWeek,
      tzDoW: state.tzDoW,
      tzMonth: state.tzMonth,
      tzHour: state.tzHour,
      tzOffset: state.tzOffset,
      tzDsName: state.tzDsName,
      tzDsWeek: state.tzDsWeek,
      tzDsDoW: state.tzDsDoW,
      tzDsMonth: state.tzDsMonth,
      tzDsHour: state.tzDsHour,
      tzDsOffset: state.tzDsOffset
    });
</script>

<section class="card">
  <h2>{t.timeServer}</h2>
  <div class="field">
    <label for="ntpServer">{t.ntpServer}</label>
    <input
      id="ntpServer"
      type="text"
      placeholder="pool.ntp.org"
      bind:value={state.ntpServer}
      onchange={push}
    />
  </div>
</section>

<section class="card">
  <h2>{t.timezoneTitle}</h2>

  <div class="field">
    <label for="useDs">{t.dst}</label>
    <span class="switch">
      <input id="useDs" type="checkbox" bind:checked={state.useDs} onchange={push} />
      <span></span>
    </span>
  </div>

  <TzRule
    id="std"
    title={t.standardTime}
    bind:name={state.tzName}
    bind:week={state.tzWeek}
    bind:dow={state.tzDoW}
    bind:month={state.tzMonth}
    bind:hour={state.tzHour}
    bind:offset={state.tzOffset}
    scheduleDisabled={!state.useDs}
    onchange={push}
  />

  <TzRule
    id="dst"
    title={t.dst}
    bind:name={state.tzDsName}
    bind:week={state.tzDsWeek}
    bind:dow={state.tzDsDoW}
    bind:month={state.tzDsMonth}
    bind:hour={state.tzDsHour}
    bind:offset={state.tzDsOffset}
    disabled={!state.useDs}
    onchange={push}
  />

  {#if !state.useDs}
    <p class="hint">{t.noDstHint}</p>
  {/if}
</section>
