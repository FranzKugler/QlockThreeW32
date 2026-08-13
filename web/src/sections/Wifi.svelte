<script>
  import { onMount, onDestroy } from 'svelte';
  import * as api from '../lib/api.js';

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

  // The clock scans asynchronously, so poll until it reports a result.
  async function scan() {
    scanning = true;
    networks = [];
    for (let i = 0; i < 15 && !destroyed; i++) {
      try {
        const res = await api.fetchWifiScan();
        if (!res.scanning) {
          networks = res.networks ?? [];
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
          if (!status.error) note = `Verbunden mit ${status.ssid}.`;
          return;
        }
      }
    }
    connecting = false;
    note =
      'Keine Rückmeldung. Wenn die Uhr in ein anderes Netz gewechselt ist, ist ' +
      'sie unter dieser Adresse nicht mehr erreichbar.';
  }

  function quality(rssi) {
    if (rssi >= -55) return { label: 'sehr gut', bars: 4 };
    if (rssi >= -67) return { label: 'gut', bars: 3 };
    if (rssi >= -75) return { label: 'mittel', bars: 2 };
    return { label: 'schwach', bars: 1 };
  }

  onMount(async () => {
    await loadStatus();
    selected = status?.ssid ?? '';
    scan();
  });
</script>

<section class="card">
  <h2>Verbindung</h2>

  {#if statusError}
    <p class="banner">Status nicht abrufbar — {statusError}</p>
  {:else if status}
    <div class="field"><span class="key">Netz</span><span>{status.ssid || '—'}</span></div>
    <div class="field"><span class="key">Adresse</span><span>{status.ip}</span></div>
    <div class="field">
      <span class="key">Signal</span>
      <span>{quality(status.rssi).label} ({status.rssi} dBm)</span>
    </div>
    <div class="field"><span class="key">Hostname</span><span>{status.hostname}.local</span></div>
    <div class="field"><span class="key">MAC</span><span>{status.mac}</span></div>

    {#if status.error}
      <p class="banner">{status.error}</p>
    {/if}
  {:else}
    <p class="hint">wird geladen …</p>
  {/if}
</section>

<section class="card">
  <h2>Verfügbare Netze</h2>

  {#if scanning}
    <p class="hint">Suche läuft …</p>
  {:else if networks.length === 0}
    <p class="hint">Keine Netze gefunden.</p>
  {:else}
    <ul class="netlist">
      {#each networks as net (net.ssid)}
        <li>
          <label class="choice">
            <input type="radio" name="ssid" value={net.ssid} bind:group={selected} />
            <span class="netname">{net.ssid}</span>
            <span class="bars" title="{net.rssi} dBm" aria-label={quality(net.rssi).label}>
              {#each [1, 2, 3, 4] as bar (bar)}
                <i class:on={bar <= quality(net.rssi).bars}></i>
              {/each}
            </span>
            {#if net.secure}<span class="lock" title="verschlüsselt">🔒</span>{/if}
          </label>
        </li>
      {/each}
    </ul>
  {/if}

  <button type="button" class="secondary" onclick={scan} disabled={scanning || connecting}>
    Erneut suchen
  </button>
</section>

<section class="card">
  <h2>Netz wechseln</h2>

  <form onsubmit={connect}>
    <div class="field">
      <label for="ssid">Netz</label>
      <input id="ssid" type="text" bind:value={selected} placeholder="SSID" />
    </div>
    <div class="field">
      <label for="pass">Passwort</label>
      <input
        id="pass"
        type="password"
        bind:value={password}
        autocomplete="off"
        placeholder="leer lassen für offene Netze"
      />
    </div>

    <button type="submit" disabled={connecting || !selected}>
      {connecting ? 'Verbinde …' : 'Verbinden'}
    </button>
  </form>

  {#if note}
    <p class="hint">{note}</p>
  {/if}

  <p class="hint">
    Die Uhr trennt sich kurz vom Netz. Klappt die Verbindung nicht, kehrt sie
    automatisch ins bisherige Netz zurück. Nach einem Wechsel kann sich die
    Adresse ändern — dann ist sie unter
    <em>{status?.hostname ?? 'QlockThreeW32'}.local</em> erreichbar.
  </p>
</section>
