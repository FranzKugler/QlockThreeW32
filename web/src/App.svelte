<script>
  /**
   * App
   * Shell of the configuration SPA: loads the clock's settings once, then
   * hands them to the section shown by the selected tab.
   *
   * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
   * @version  2.0
   * @created  15.8.2026
   * @updated  15.8.2026
   */
  import { onMount } from 'svelte';
  import { fetchState } from './lib/api.js';
  import { status } from './lib/status.svelte.js';
  import Display from './sections/Display.svelte';
  import Color from './sections/Color.svelte';
  import Timezone from './sections/Timezone.svelte';
  import Wifi from './sections/Wifi.svelte';
  import Ota from './sections/Ota.svelte';

  const TABS = [
    { id: 'display', label: 'Anzeige' },
    { id: 'color', label: 'Farbe' },
    { id: 'timezone', label: 'Timezone' },
    { id: 'wifi', label: 'WLAN' },
    { id: 'ota', label: 'Update' }
  ];

  let active = $state('display');
  let clock = $state(null);
  let loadError = $state(null);

  async function load() {
    loadError = null;
    try {
      clock = await fetchState();
    } catch (err) {
      loadError = err.message;
    }
  }

  onMount(load);
</script>

<div class="app">
  <header>
    <h1>QlockThreeW32</h1>
  </header>

  {#if clock}
    <nav>
      {#each TABS as tab (tab.id)}
        <button
          type="button"
          aria-current={active === tab.id ? 'page' : undefined}
          onclick={() => (active = tab.id)}
        >
          {tab.label}
        </button>
      {/each}
    </nav>

    {#if status.error}
      <p class="banner">Übertragung fehlgeschlagen — {status.error}</p>
    {/if}

    {#if active === 'display'}
      <Display state={clock} />
    {:else if active === 'color'}
      <Color state={clock} />
    {:else if active === 'timezone'}
      <Timezone state={clock} />
    {:else if active === 'wifi'}
      <Wifi />
    {:else}
      <Ota />
    {/if}
  {:else if loadError}
    <p class="centered">
      Uhr nicht erreichbar — {loadError}
      <br />
      <button type="button" onclick={load}>Erneut versuchen</button>
    </p>
  {:else}
    <p class="centered">Einstellungen werden geladen …</p>
  {/if}
</div>
