<script>
  /**
   * NetworkList
   * The scan result as a list of networks to pick from.
   *
   * This is the component the setup portal and the WLAN tab share, and it is
   * the reason the two look alike rather than merely similar: there is one
   * list, one set of signal bars, one padlock, and one answer to what a mesh
   * network's four identical SSIDs should look like.
   *
   * It owns the polling too. The clock scans asynchronously - the first
   * request starts a scan and answers `{scanning: true}` - so a component that
   * only rendered a prop would leave every caller writing the same loop. The
   * caller passes the endpoint to poll and gets a finished list.
   *
   * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
   * @version  1.0
   * @created  4.9.2026
   * @updated  4.9.2026
   */
  import { onMount, onDestroy } from 'svelte';
  import { dict } from './i18n.svelte.js';
  import { bars } from './signal.js';

  const t = $derived(dict());

  let {
    /** One poll of the scan. `() => Promise<{scanning} | {networks}>`. */
    poll,
    /** The chosen SSID, bindable so the form beside the list follows it. */
    selected = $bindable(''),
    /** Set while the caller is busy connecting; the rescan button waits. */
    busy = false
  } = $props();

  let networks = $state([]);
  let scanning = $state(false);

  let destroyed = false;
  onDestroy(() => (destroyed = true));

  const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

  /**
   * One entry per network name, strongest first.
   *
   * A mesh or a dual-band router answers a scan several times under the same
   * SSID. You join a network by name, not by radio, so the repeats are noise -
   * and they are fatal here: the list below is keyed by SSID, and Svelte throws
   * on a duplicate key, which takes the whole list down rather than one row.
   * Hidden networks come back nameless and cannot be picked, so they go too.
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

  /**
   * Polls until the clock reports a result.
   *
   * The ceiling is a guard against a scan that never finishes rather than a
   * timeout anybody should hit: fifteen rounds is eighteen seconds.
   */
  async function scan() {
    if (scanning) return;
    scanning = true;
    networks = [];
    for (let i = 0; i < 15 && !destroyed; i++) {
      try {
        const res = await poll();
        if (!res.scanning) {
          networks = strongestPerName(res.networks ?? []);
          scanning = false;
          return;
        }
      } catch {
        // Keep going: a scan makes the radio briefly unresponsive, and in the
        // portal it is the very access point this page arrived over.
      }
      await sleep(1200);
    }
    scanning = false;
  }

  onMount(scan);
</script>

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

<button type="button" class="secondary" onclick={scan} disabled={scanning || busy}>
  {t.rescan}
</button>
