<script>
  /**
   * Timezone
   * Timezone tab: NTP server, a region/place picker, and both changeover rules
   * of the clock.
   *
   * The picker is a shortcut, not a layer: it fills the same fourteen fields
   * the rules below expose, and they stay editable afterwards. Editing one by
   * hand clears the picked name, because the two would otherwise disagree and
   * the label would be the one lying.
   *
   * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
   * @version  2.1
   * @created  15.8.2026
   * @updated  16.8.2026
   */
  import { onMount } from 'svelte';
  import * as api from '../lib/api.js';
  import { dict } from '../lib/i18n.svelte.js';
  import { loadZones } from '../lib/zones.js';
  import { timezoneFields, zoneArea, zonePlace } from '../lib/posixtz.js';
  import TzRule from './TzRule.svelte';

  // Aliased on the way in: the prop is called `state` everywhere else, but a
  // local binding of that name turns every `$state(...)` in this component
  // into a store subscription instead of the rune, and the compiler only
  // warns about it - the selects would simply never re-render.
  let { state: clock } = $props();

  const t = $derived(dict());

  let zones = $state(null);
  let zonesError = $state(null);
  // Region shown in the first select, and the entry shown in the second. The
  // latter is held separately from clock.tzZone so that switching region can
  // empty it without touching what the clock is actually set to.
  let area = $state('');
  let zone = $state('');

  // Every change posts the complete timezone object, as the firmware rebuilds
  // both TimeChangeRules from it in one go.
  const push = () =>
    api.setTimezone({
      ntpServer: clock.ntpServer,
      useDs: clock.useDs,
      tzZone: clock.tzZone ?? '',
      tzName: clock.tzName,
      tzWeek: clock.tzWeek,
      tzDoW: clock.tzDoW,
      tzMonth: clock.tzMonth,
      tzHour: clock.tzHour,
      tzOffset: clock.tzOffset,
      tzDsName: clock.tzDsName,
      tzDsWeek: clock.tzDsWeek,
      tzDsDoW: clock.tzDsDoW,
      tzDsMonth: clock.tzDsMonth,
      tzDsHour: clock.tzDsHour,
      tzDsOffset: clock.tzDsOffset
    });

  // Any edit to the rules themselves means they are no longer the ones the
  // picked place stands for.
  function pushEdited() {
    clock.tzZone = '';
    zone = '';
    push();
  }

  const areas = $derived(
    zones ? [...new Set(Object.keys(zones.zones).map(zoneArea))].sort() : []
  );

  const places = $derived(
    zones && area
      ? Object.keys(zones.zones)
          .filter((name) => zoneArea(name) === area)
          .sort((a, b) => zonePlace(a).localeCompare(zonePlace(b)))
      : []
  );

  onMount(async () => {
    try {
      zones = await loadZones();
    } catch (err) {
      zonesError = err.message;
      return;
    }
    // Show what is stored, if the rules came from the list at all.
    if (clock.tzZone && zones.zones[clock.tzZone]) {
      area = zoneArea(clock.tzZone);
      zone = clock.tzZone;
    }
  });

  /** A different region only refills the second select; nothing is sent yet. */
  function onArea() {
    zone = '';
  }

  function onZone() {
    const fields = timezoneFields(zone, zones.zones[zone]);
    if (!fields) return;
    Object.assign(clock, fields);
    push();
  }
</script>

<section class="card">
  <h2>{t.timeServer}</h2>
  <div class="field">
    <label for="ntpServer">{t.ntpServer}</label>
    <input
      id="ntpServer"
      type="text"
      placeholder="pool.ntp.org"
      bind:value={clock.ntpServer}
      onchange={push}
    />
  </div>
</section>

<section class="card">
  <h2>{t.tzPickerTitle}</h2>

  {#if zonesError}
    <p class="hint">{t.tzListUnavailable}</p>
  {:else}
    <div class="field">
      <label for="tzArea">{t.tzRegion}</label>
      <select id="tzArea" bind:value={area} onchange={onArea} disabled={!zones}>
        <option value="">{zones ? t.tzChoose : t.loadingShort}</option>
        {#each areas as name (name)}
          <option value={name}>{name}</option>
        {/each}
      </select>
    </div>

    <div class="field">
      <label for="tzZone">{t.tzPlace}</label>
      <select id="tzZone" bind:value={zone} onchange={onZone} disabled={!area}>
        <option value="">{clock.tzZone ? t.tzChoose : t.tzCustom}</option>
        {#each places as name (name)}
          <option value={name}>{zonePlace(name)}</option>
        {/each}
      </select>
    </div>

    <p class="hint">{t.tzPickerHint}{zones ? ` ${t.tzDataVersion(zones.tzdata)}` : ''}</p>
  {/if}
</section>

<section class="card">
  <h2>{t.timezoneTitle}</h2>

  <div class="field">
    <label for="useDs">{t.dst}</label>
    <span class="switch">
      <input id="useDs" type="checkbox" bind:checked={clock.useDs} onchange={pushEdited} />
      <span></span>
    </span>
  </div>

  <TzRule
    id="std"
    title={t.standardTime}
    bind:name={clock.tzName}
    bind:week={clock.tzWeek}
    bind:dow={clock.tzDoW}
    bind:month={clock.tzMonth}
    bind:hour={clock.tzHour}
    bind:offset={clock.tzOffset}
    scheduleDisabled={!clock.useDs}
    onchange={pushEdited}
  />

  <TzRule
    id="dst"
    title={t.dst}
    bind:name={clock.tzDsName}
    bind:week={clock.tzDsWeek}
    bind:dow={clock.tzDsDoW}
    bind:month={clock.tzDsMonth}
    bind:hour={clock.tzDsHour}
    bind:offset={clock.tzDsOffset}
    disabled={!clock.useDs}
    onchange={pushEdited}
  />

  {#if !clock.useDs}
    <p class="hint">{t.noDstHint}</p>
  {/if}
</section>
