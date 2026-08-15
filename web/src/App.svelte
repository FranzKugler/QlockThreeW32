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
  import { dict, setLanguage } from './lib/i18n.svelte.js';
  import Display from './sections/Display.svelte';
  import Color from './sections/Color.svelte';
  import Timezone from './sections/Timezone.svelte';
  import Wifi from './sections/Wifi.svelte';
  import Ota from './sections/Ota.svelte';

  // Labels come from t.tabs, in this order.
  const TAB_IDS = ['display', 'color', 'timezone', 'wifi', 'ota'];

  let active = $state('display');
  let clock = $state(null);
  let loadError = $state(null);

  const t = $derived(dict());

  // The UI speaks whatever the clock speaks, so changing the language in the
  // display tab switches this page over as well.
  $effect(() => {
    if (clock) setLanguage(clock.language);
  });

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
      {#each TAB_IDS as id, i (id)}
        <button
          type="button"
          aria-current={active === id ? 'page' : undefined}
          onclick={() => (active = id)}
        >
          {t.tabs[i]}
        </button>
      {/each}
    </nav>

    {#if status.error}
      <p class="banner">{t.writeFailed} — {status.error}</p>
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
      {t.clockUnreachable} — {loadError}
      <br />
      <button type="button" onclick={load}>{t.retry}</button>
    </p>
  {:else}
    <p class="centered">{t.loading}</p>
  {/if}
</div>
