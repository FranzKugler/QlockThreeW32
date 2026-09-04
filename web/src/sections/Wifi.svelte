<script>
  /**
   * Wifi
   * WLAN tab: connection status, network scan and switching networks.
   * First-time setup is not covered here - without a connection the SPA is
   * unreachable and the setup portal (Portal.svelte) takes over instead.
   *
   * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
   * @version  2.0
   * @created  15.8.2026
   * @updated  15.8.2026
   */
  import { onMount, onDestroy } from 'svelte';
  import * as api from '../lib/api.js';
  import { dict } from '../lib/i18n.svelte.js';
  import { errorText } from '../lib/errors.js';
  import { setAppName } from '../lib/appname.svelte.js';
  import { bars } from '../lib/signal.js';
  import NetworkList from '../lib/NetworkList.svelte';

  const t = $derived(dict());

  // Renaming the clock. Held apart from `status` so the field keeps what is
  // being typed while the status block refreshes underneath it.
  let hostname = $state('');
  let renaming = $state(false);
  let renameError = $state(null);
  let renameNote = $state(null);

  let status = $state(null);
  let statusError = $state(null);
  let selected = $state('');
  let password = $state('');
  let connecting = $state(false);
  let note = $state(null);

  let destroyed = false;
  onDestroy(() => (destroyed = true));

  const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

  async function loadStatus({ quiet = false } = {}) {
    try {
      status = await api.fetchWifi();
      statusError = null;
      return true;
    } catch (err) {
      if (!quiet) statusError = err.message;
      return false;
    }
  }

  async function connect(event) {
    event.preventDefault();
    if (!selected || connecting) return;

    connecting = true;
    note = null;
    await api.connectWifi({ ssid: selected, password });
    password = '';

    // The clock leaves the network to try the new one, so it is unreachable for
    // a while. Keep polling quietly until it settles, one way or the other.
    for (let i = 0; i < 30 && !destroyed; i++) {
      await sleep(2000);
      if (await loadStatus({ quiet: true })) {
        if (!status.switching) {
          connecting = false;
          if (!status.error) note = t.connectedTo(status.ssid);
          return;
        }
      }
    }
    connecting = false;
    note = t.noResponse;
  }

  /**
   * Only what can be a DNS label, so the field cannot offer something the
   * firmware would silently reduce. It applies the same rule server-side, since
   * the endpoint is reachable without this page.
   */
  const cleanHostname = (value) => value.replace(/[^A-Za-z0-9-]/g, '').slice(0, 32);

  const renameable = $derived(
    hostname.replace(/^-+|-+$/g, '').length > 0 && hostname !== status?.hostname
  );

  /**
   * Renames the clock, which restarts it: the name is read in six places and
   * only mDNS can be changed while it runs. The clock answers first and
   * restarts a second later, so this waits for it to come back rather than
   * leaving the page looking as though nothing happened.
   */
  async function rename(event) {
    event.preventDefault();
    if (!renameable || renaming) return;

    renaming = true;
    renameError = null;
    renameNote = null;
    try {
      // The firmware answers with what it stored, which is what should end up
      // on screen - it may have trimmed a trailing hyphen.
      const { hostname: stored, restarting } = await api.setHostname(hostname);
      hostname = stored;
      if (status) status.hostname = stored;
      setAppName(stored);

      if (!restarting) {
        renameNote = t.hostnameSaved(stored);
      } else {
        renameNote = t.restarting;
        await sleep(3000);

        renameNote = t.noResponse;
        for (let i = 0; i < 20 && !destroyed; i++) {
          if (await loadStatus({ quiet: true })) {
            renameNote = t.hostnameSaved(stored);
            break;
          }
          await sleep(1500);
        }
      }
    } catch (err) {
      renameError = err.message;
    }
    renaming = false;
  }

  onMount(async () => {
    await loadStatus();
    selected = status?.ssid ?? '';
    hostname = status?.hostname ?? '';
  });
</script>

<section class="card">
  <h2>{t.connection}</h2>

  {#if statusError}
    <p class="banner">{t.statusUnavailable} — {statusError}</p>
  {:else if status}
    <div class="field"><span class="key">{t.network}</span><span>{status.ssid || '—'}</span></div>
    <div class="field"><span class="key">{t.address}</span><span>{status.ip}</span></div>
    <div class="field">
      <span class="key">{t.signal}</span>
      <span>{t.quality[bars(status.rssi) - 1]} ({status.rssi} dBm)</span>
    </div>
    <div class="field"><span class="key">{t.hostname}</span><span>{status.hostname}.local</span></div>
    <div class="field"><span class="key">{t.mac}</span><span>{status.mac}</span></div>

    {#if status.error}
      <p class="banner">{errorText(t, status.error, status.errorDetail)}</p>
    {/if}
  {:else}
    <p class="hint">{t.loadingShort}</p>
  {/if}
</section>

<section class="card">
  <h2>{t.clockName}</h2>

  <form onsubmit={rename}>
    <div class="field">
      <label for="hostname">{t.name}</label>
      <input
        id="hostname"
        type="text"
        maxlength="32"
        autocomplete="off"
        spellcheck="false"
        placeholder="QlockThreeW32"
        bind:value={hostname}
        oninput={() => (hostname = cleanHostname(hostname))}
      />
    </div>

    <button type="submit" disabled={!renameable || renaming}>
      {renaming ? t.restarting : t.saveAndRestart}
    </button>
  </form>

  {#if renameError}
    <p class="banner">{errorText(t, renameError, null)}</p>
  {:else if renameNote}
    <p class="hint">{renameNote}</p>
  {/if}

  <p class="hint">{t.hostnameHint()}</p>
</section>

<section class="card">
  <h2>{t.availableNetworks}</h2>
  <NetworkList poll={api.fetchWifiScan} bind:selected busy={connecting} />
</section>

<section class="card">
  <h2>{t.switchNetwork}</h2>

  <form onsubmit={connect}>
    <div class="field">
      <label for="ssid">{t.network}</label>
      <input id="ssid" type="text" bind:value={selected} placeholder="SSID" />
    </div>
    <div class="field">
      <label for="pass">{t.password}</label>
      <input
        id="pass"
        type="password"
        bind:value={password}
        autocomplete="off"
        placeholder={t.passwordPlaceholder}
      />
    </div>

    <button type="submit" disabled={connecting || !selected}>
      {connecting ? t.connecting : t.connect}
    </button>
  </form>

  {#if note}
    <p class="hint">{note}</p>
  {/if}

  <p class="hint">{t.wifiHint(status?.hostname ?? 'QlockThreeW32')}</p>
</section>
