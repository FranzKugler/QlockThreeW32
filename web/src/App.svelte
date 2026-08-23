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
  import { fetchState, fetchExpert, fetchLanguages } from './lib/api.js';
  import { status } from './lib/status.svelte.js';
  import { dict, setLanguage, setLanguageList } from './lib/i18n.svelte.js';
  import { appName, setAppName } from './lib/appname.svelte.js';
  import Display from './sections/Display.svelte';
  import Color from './sections/Color.svelte';
  import Timezone from './sections/Timezone.svelte';
  import Wifi from './sections/Wifi.svelte';
  import Ota from './sections/Ota.svelte';
  import Debug from './sections/Debug.svelte';
  import Storage from './sections/Storage.svelte';
  import Expert from './sections/Expert.svelte';
  import Luminance from './sections/Luminance.svelte';

  // Labels come from t.tabs, in this order. The last four are only offered
  // once the clock is unlocked; t.tabs keeps all eight either way, so the label
  // of a tab is found by its place in this list rather than in the visible one.
  const ALL_TABS = ['display', 'color', 'timezone', 'wifi', 'ota', 'debug', 'storage', 'luminance'];
  const OPEN_TABS = 4;

  let active = $state('display');
  let clock = $state(null);
  let loadError = $state(null);

  // Until the clock says otherwise it is locked. That is the safe way round:
  // an older firmware with no /expert at all answers 404, and the tabs stay
  // away rather than being offered against endpoints that refuse them.
  let expert = $state({ enrolled: false, unlocked: false, grace: 0, lockedOut: false });

  // Only relevant on narrow screens, where the tab row collapses into a menu.
  let menuOpen = $state(false);
  let navEl = $state(null);

  const t = $derived(dict());
  const tabIds = $derived(expert.unlocked ? ALL_TABS : ALL_TABS.slice(0, OPEN_TABS));
  const labelOf = (id) => t.tabs[ALL_TABS.indexOf(id)];
  // Two screens are reachable by address as well as - in one case - by chip.
  // #expert has no chip at all, on purpose. #luminance has one once the clock
  // is unlocked, and stays reachable by address while it is locked: the
  // brightness screen is read-only then, which is what somebody wants when the
  // automatic feels wrong and they have no password to hand.
  const HASH_SCREENS = { '#expert': 'expert', '#luminance': 'luminance' };
  const HASH_TITLES = $derived({ expert: t.expertTitle, luminance: t.lumTitle });
  // The chip wins when there is one, or the tab would carry a heading that
  // does not match the chip the reader just clicked.
  const activeLabel = $derived(tabIds.includes(active) ? labelOf(active) : HASH_TITLES[active]);

  function select(id) {
    active = id;
    menuOpen = false;
    // Leave the hash behind, or reloading the page would land back on it.
    if (location.hash) history.replaceState(null, '', location.pathname);
  }

  /**
   * The expert screen has no chip in the tab row - there is nothing there for
   * someone who has not gone looking, and a visible one would only invite
   * guessing. `#expert` in the address is the way in.
   */
  async function syncFromHash() {
    const screen = HASH_SCREENS[location.hash];
    if (!screen) return;

    active = screen;
    if (screen !== 'expert') return;

    // Fetched again on the way in: the reset window is counting down, and a
    // value read when the page loaded would be stale by minutes.
    try {
      expert = await fetchExpert();
    } catch {
      /* keep what we have */
    }
  }

  /**
   * Takes the state the clock answered with. Locking while an expert tab is
   * open would leave it on screen with every request behind it refused, so
   * step back to the front.
   */
  function applyExpert(next) {
    expert = next;
    // The brightness screen survives locking, because it is still readable at
    // #luminance - it simply stops offering the writes. Everything else behind
    // the lock would be left on screen with every request behind it refused.
    if (!next.unlocked && active !== 'luminance' && ALL_TABS.indexOf(active) >= OPEN_TABS) {
      active = 'display';
    }
  }

  // Close the menu the way a menu is expected to close.
  function onWindowKeydown(event) {
    if (event.key === 'Escape') menuOpen = false;
  }

  function onWindowPointerdown(event) {
    if (menuOpen && navEl && !navEl.contains(event.target)) menuOpen = false;
  }

  // The UI speaks whatever the clock speaks, so changing the language in the
  // display tab switches this page over as well.
  $effect(() => {
    if (clock) setLanguage(clock.language);
  });

  // Heading, browser tab and home screen label all follow the clock's own name,
  // which is the only thing distinguishing two of them on one network. The
  // WLAN tab updates it in place after a rename.
  $effect(() => {
    if (clock?.hostname) setAppName(clock.hostname);
  });

  async function load() {
    loadError = null;
    try {
      clock = await fetchState();
    } catch (err) {
      loadError = err.message;
    }

    // Which languages the clock has, and what it calls them. Asked once:
    // it cannot change without a firmware update, and a firmware update
    // reloads this page anyway.
    try {
      setLanguageList(await fetchLanguages());
    } catch {
      // Firmware from before /languages existed. The built-in list is that
      // firmware's list, so the picker is right for it.
    }

    try {
      expert = await fetchExpert();
    } catch {
      // Not fatal, and not a reason to unlock anything: the default above
      // already says locked.
    }
  }

  onMount(async () => {
    await load();
    await syncFromHash();
  });
</script>

<svelte:window
  onkeydown={onWindowKeydown}
  onpointerdown={onWindowPointerdown}
  onhashchange={syncFromHash}
/>

<div class="app">
  <!--
    The nav sits inside the header so that a narrow screen can lay the two out
    as one bar - title left, menu button right - instead of stacking them.
  -->
  <header>
    <h1>{appName()}</h1>

    {#if clock}
      <nav bind:this={navEl} class:open={menuOpen}>
        <!-- Shown instead of the row when the viewport is too narrow for it. -->
        <button
          type="button"
          class="menu-toggle"
          aria-expanded={menuOpen}
          aria-controls="tabs"
          onclick={() => (menuOpen = !menuOpen)}
        >
          <span class="burger" aria-hidden="true"></span>
          <span>{activeLabel}</span>
        </button>

        <div class="tabs" id="tabs">
          {#each tabIds as id (id)}
            <button
              type="button"
              aria-current={active === id ? 'page' : undefined}
              onclick={() => select(id)}
            >
              {labelOf(id)}
            </button>
          {/each}
        </div>
      </nav>
    {/if}
  </header>

  {#if clock}
    {#if status.error}
      <p class="banner">{t.writeFailed} — {status.error}</p>
    {/if}

    {#if active === 'display'}
      <Display state={clock} {expert} />
    {:else if active === 'color'}
      <Color state={clock} />
    {:else if active === 'timezone'}
      <Timezone state={clock} />
    {:else if active === 'wifi'}
      <Wifi />
    {:else if active === 'ota'}
      <Ota />
    {:else if active === 'debug'}
      <Debug />
    {:else if active === 'storage'}
      <Storage />
    {:else if active === 'luminance'}
      <!-- The same screen either way; the lock decides whether it offers the
           writes. Two components would be two things to keep in step. -->
      <Luminance {expert} />
    {:else}
      <Expert {expert} onchange={applyExpert} />
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
