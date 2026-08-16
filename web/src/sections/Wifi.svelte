<script>
  /**
   * Wifi
   * WLAN tab: connection status, network scan and switching networks.
   * First-time setup is not covered here - without a connection the SPA is
   * unreachable and WiFiManager's own portal takes over.
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

  const t = $derived(dict());

  let status = $state(null);
  let statusError = $state(null);
  let networks = $state([]);
  let scanning = $state(false);
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

  /**
   * One entry per network name, strongest first.
   *
   * A mesh or a dual-band router answers a scan several times under the same
   * SSID. You join a network by name, not by radio, so the repeats are noise -
   * and they were fatal here: the list below is keyed by SSID, and Svelte
   * throws on a duplicate key, which took the whole list down rather than one
   * row. Hidden networks come back nameless and cannot be picked, so they go
   * as well.
   */
  function strongestPerName(found) {
    const best = new Map();
    for (const net of found) {
      if (!net.ssid) continue;
      const seen = best.get(net.ssid);
      if (!seen || net.rssi > seen.rssi) best.set(net.ssid, net);
    }
    return [...best.values()].sort((a, b) => b.rssi - a.rssi);
  }

  // The clock scans asynchronously, so poll until it reports a result.
  async function scan() {
    scanning = true;
    networks = [];
    for (let i = 0; i < 15 && !destroyed; i++) {
      try {
        const res = await api.fetchWifiScan();
        if (!res.scanning) {
          networks = strongestPerName(res.networks ?? []);
          scanning = false;
          return;
        }
      } catch {
        // keep polling; a scan makes the clock briefly unresponsive
      }
      await sleep(1200);
    }
    scanning = false;
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

  /** Signal strength as 1..4 bars; t.quality holds the matching labels. */
  function bars(rssi) {
    if (rssi >= -55) return 4;
    if (rssi >= -67) return 3;
    if (rssi >= -75) return 2;
    return 1;
  }

  onMount(async () => {
    await loadStatus();
    selected = status?.ssid ?? '';
    scan();
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
  <h2>{t.availableNetworks}</h2>

  {#if scanning}
    <p class="hint">{t.scanning}</p>
  {:else if networks.length === 0}
    <p class="hint">{t.noNetworks}</p>
  {:else}
    <ul class="netlist">
      {#each networks as net (net.ssid)}
        <li>
          <label class="choice">
            <input type="radio" name="ssid" value={net.ssid} bind:group={selected} />
            <span class="netname">{net.ssid}</span>
            <span class="bars" title="{net.rssi} dBm" aria-label={t.quality[bars(net.rssi) - 1]}>
              {#each [1, 2, 3, 4] as bar (bar)}
                <i class:on={bar <= bars(net.rssi)}></i>
              {/each}
            </span>
            {#if net.secure}<span class="lock" title={t.encrypted}>🔒</span>{/if}
          </label>
        </li>
      {/each}
    </ul>
  {/if}

  <button type="button" class="secondary" onclick={scan} disabled={scanning || connecting}>
    {t.rescan}
  </button>
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
