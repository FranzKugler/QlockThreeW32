<script>
  /**
   * Portal
   * The setup portal, as it is seen from a phone that has just joined the
   * clock's own access point.
   *
   * The same shell as the configuration SPA - one header, cards, the shared
   * network list - minus everything that needs a clock on a network. There is
   * no tab row, because there is exactly one thing to do here.
   *
   * The whole point of this file is that it is short. Everything that makes it
   * look like the rest of the UI comes from app.css and NetworkList.svelte,
   * which the WLAN tab uses too; nothing here restates a colour, a card or a
   * signal bar. If the two ever drift apart, it will be because something was
   * added here rather than to the shared parts, and that is the thing to
   * resist.
   *
   * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
   * @version  1.0
   * @created  4.9.2026
   * @updated  4.9.2026
   */
  import { onMount, onDestroy } from 'svelte';
  import * as api from './lib/api.js';
  import { dict } from './lib/i18n.svelte.js';
  import { errorText } from './lib/errors.js';
  import NetworkList from './lib/NetworkList.svelte';

  const t = $derived(dict());

  let info = $state(null);
  let selected = $state('');
  let password = $state('');
  let error = $state(null);

  let timer = null;
  let destroyed = false;
  onDestroy(() => {
    destroyed = true;
    clearInterval(timer);
  });

  const connecting = $derived(info?.state === 'connecting');
  const connected = $derived(info?.state === 'connected');

  async function loadStatus() {
    try {
      info = await api.fetchPortalStatus();
      // The clock reports the failure of an attempt in the same answer, so
      // the banner is driven by its state rather than by what this page
      // remembers about the request it sent.
      if (info.state === 'failed' && info.error) {
        error = errorText(t, info.error, info.errorDetail);
      }
    } catch {
      // The access point stutters while the clock scans or joins a network.
      // Keeping the last answer on screen is better than flashing an error at
      // somebody who is watching a progress bar.
    }
  }

  async function connect(event) {
    event.preventDefault();
    if (!selected || connecting) return;

    error = null;
    try {
      info = await api.portalConnect({ ssid: selected, password });
      password = '';
    } catch (err) {
      error = err.message;
    }
  }

  onMount(() => {
    loadStatus();
    // Fast enough to follow an attempt, slow enough not to fight the radio
    // while it is scanning or associating.
    timer = setInterval(loadStatus, 1500);
  });
</script>

<div class="app">
  <header>
    <h1>{info?.hostname ?? 'QlockThreeW32'}</h1>
  </header>

  <section class="card">
    <h2>{t.portalTitle}</h2>
    <p class="hint">{t.portalIntro(info?.apName ?? 'QlockThreeW32')}</p>
  </section>

  {#if connected}
    <!-- The end of the story: the clock is about to restart and this access
         point goes with it, so this card replaces the form rather than sitting
         above it. Nothing here is worth doing twice. -->
    <section class="card">
      <h2>{t.connection}</h2>
      <p class="hint">{t.portalConnected(info.ssid, info.ip)}</p>
      <p class="hint">{t.portalDone}</p>
    </section>
  {:else}
    <section class="card">
      <h2>{t.availableNetworks}</h2>
      <!-- The component the WLAN tab uses; it owns the polling of the scan. -->
      <NetworkList poll={api.fetchPortalScan} bind:selected busy={connecting} />
    </section>

    <section class="card">
      <h2>{t.connect}</h2>

      <form onsubmit={connect}>
        <div class="field">
          <label for="ssid">{t.network}</label>
          <!-- Typable as well as pickable: a hidden network never shows up in
               a scan and is joined by name. -->
          <input id="ssid" type="text" bind:value={selected} placeholder="SSID"
                 disabled={connecting} />
        </div>
        <div class="field">
          <label for="pass">{t.password}</label>
          <input
            id="pass"
            type="password"
            bind:value={password}
            autocomplete="off"
            placeholder={t.passwordPlaceholder}
            disabled={connecting}
          />
        </div>

        <button type="submit" disabled={connecting || !selected}>
          {connecting ? t.connecting : t.connect}
        </button>
      </form>

      {#if connecting}
        <!-- Indeterminate on purpose: the clock cannot say how far along an
             association is, and a bar creeping to 90 % and stopping would be a
             worse lie than one that only says "still going". -->
        <div class="progress indeterminate" role="progressbar" aria-valuetext={t.connecting}>
          <div class="bar"></div>
        </div>
        <p class="hint">{t.portalConnecting(info?.ssid ?? selected)}</p>
      {/if}

      {#if error}
        <p class="banner">{error}</p>
      {/if}
    </section>
  {/if}
</div>
