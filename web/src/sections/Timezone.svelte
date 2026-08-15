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
  import TzRule from './TzRule.svelte';

  let { state } = $props();

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
  <h2>Zeitserver</h2>
  <div class="field">
    <label for="ntpServer">NTP Server</label>
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
  <h2>Zeitzone</h2>

  <div class="field">
    <label for="useDs">Sommerzeit</label>
    <span class="switch">
      <input id="useDs" type="checkbox" bind:checked={state.useDs} onchange={push} />
      <span></span>
    </span>
  </div>

  <TzRule
    title="Normalzeit"
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
    title="Sommerzeit"
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
    <p class="hint">
      Ohne Sommerzeit gilt durchgehend der Offset der Normalzeit; die
      Umschaltzeitpunkte werden nicht ausgewertet.
    </p>
  {/if}
</section>
