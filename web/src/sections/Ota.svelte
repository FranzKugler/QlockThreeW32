<script>
  /**
   * Ota
   * Update tab: shows the installed versions and flashes an image picked in the
   * browser. The clock has two independent images - the firmware and the
   * filesystem holding this web UI - and works out from the file itself which
   * one it was handed.
   *
   * The GitHub path (checking a manifest, optional automatic updates) will be
   * added here as a second card.
   *
   * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
   * @version  2.0
   * @created  15.8.2026
   * @updated  15.8.2026
   */
  import { onMount, onDestroy } from 'svelte';
  import * as api from '../lib/api.js';

  let info = $state(null);
  let infoError = $state(null);

  let file = $state(null);
  /** 'firmware' | 'filesystem', read from the image's first byte. */
  let kind = $state(null);
  /** 'idle' | 'uploading' | 'rebooting' | 'done' | 'failed' */
  let phase = $state('idle');
  let progress = $state(0);
  let error = $state(null);

  let destroyed = false;
  onDestroy(() => (destroyed = true));

  const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

  async function loadInfo({ quiet = false } = {}) {
    try {
      info = await api.fetchOtaStatus();
      infoError = null;
      return true;
    } catch (err) {
      if (!quiet) infoError = err.message;
      return false;
    }
  }

  async function pick(event) {
    error = null;
    phase = 'idle';
    progress = 0;
    file = event.target.files?.[0] ?? null;
    kind = null;
    if (!file) return;

    // Same test the firmware makes: ESP32 application images start with 0xE9.
    // Doing it here too means the warning below can be shown before uploading.
    const head = new Uint8Array(await file.slice(0, 1).arrayBuffer());
    kind = head[0] === 0xe9 ? 'firmware' : 'filesystem';
  }

  async function upload() {
    if (!file || phase === 'uploading') return;

    phase = 'uploading';
    progress = 0;
    error = null;

    try {
      await api.uploadImage(file, (fraction) => (progress = fraction));
    } catch (err) {
      // The connection dropping at the very end is expected - the clock reboots
      // as soon as it has answered. Only treat it as a failure if it does not
      // come back.
      if (progress < 0.99) {
        phase = 'failed';
        error = err.message;
        return;
      }
    }

    phase = 'rebooting';
    const back = await waitForClock();
    if (destroyed) return;

    if (!back) {
      phase = 'failed';
      error =
        'Die Uhr hat sich nach dem Neustart nicht zurückgemeldet. ' +
        'Sie startet bei einem fehlerhaften Image mit der bisherigen Version.';
      return;
    }

    phase = 'done';
    // The page itself came out of the image that was just replaced, so it has
    // to be re-fetched to match what is now in flash.
    if (kind === 'filesystem') {
      await sleep(1500);
      location.reload();
    }
  }

  /** Polls until the clock answers again, for up to a minute. */
  async function waitForClock() {
    for (let i = 0; i < 30 && !destroyed; i++) {
      await sleep(2000);
      if (await loadInfo({ quiet: true })) return true;
    }
    return false;
  }

  const KIND_LABEL = { firmware: 'Firmware-Image', filesystem: 'Dateisystem-Image (Weboberfläche)' };

  function size(bytes) {
    if (!bytes && bytes !== 0) return '—';
    return `${(bytes / 1024 / 1024).toFixed(2)} MB`;
  }

  onMount(loadInfo);
</script>

<section class="card">
  <h2>Installiert</h2>

  {#if infoError}
    <p class="banner">Status nicht abrufbar — {infoError}</p>
  {:else if info}
    <div class="field"><span class="key">Firmware</span><span>{info.firmwareVersion || '—'}</span></div>
    <div class="field">
      <span class="key">Weboberfläche</span><span>{info.fsVersion || 'unbekannt'}</span>
    </div>
    <div class="field"><span class="key">Belegt</span><span>{size(info.sketchSize)}</span></div>
    <div class="field"><span class="key">Platz für Update</span><span>{size(info.freeSpace)}</span></div>
  {:else}
    <p class="hint">wird geladen …</p>
  {/if}
</section>

<section class="card">
  <h2>Image hochladen</h2>

  <div class="field">
    <label for="image">Datei</label>
    <input
      id="image"
      type="file"
      accept=".bin,application/octet-stream"
      onchange={pick}
      disabled={phase === 'uploading' || phase === 'rebooting'}
    />
  </div>

  {#if file}
    <div class="field">
      <span class="key">Erkannt als</span>
      <span>{KIND_LABEL[kind]} · {size(file.size)}</span>
    </div>
  {/if}

  {#if kind === 'filesystem'}
    <p class="hint">
      Das Dateisystem-Image überschreibt die gesamte Partition. Die Uhr sichert
      ihre Einstellungen vorher im NVS und stellt sie beim Neustart wieder her.
    </p>
  {/if}

  {#if phase === 'uploading' || phase === 'rebooting'}
    <div class="progress" role="progressbar" aria-valuenow={Math.round(progress * 100)}>
      <div class="bar" style="width: {Math.round(progress * 100)}%"></div>
    </div>
    <p class="hint">
      {#if phase === 'uploading'}
        Wird geschrieben … {Math.round(progress * 100)} %
      {:else}
        Die Uhr startet neu — bitte warten.
      {/if}
    </p>
  {/if}

  {#if phase === 'done'}
    <p class="hint success">
      Update eingespielt. Firmware {info?.firmwareVersion}, Weboberfläche
      {info?.fsVersion || 'unbekannt'}.
    </p>
  {/if}

  {#if error}
    <p class="banner">{error}</p>
  {/if}

  <button
    type="button"
    onclick={upload}
    disabled={!file || phase === 'uploading' || phase === 'rebooting'}
  >
    {phase === 'uploading' || phase === 'rebooting' ? 'Läuft …' : 'Hochladen und neu starten'}
  </button>

  <p class="hint">
    <em>firmware.bin</em> und <em>littlefs.bin</em> entstehen mit
    <em>pio run</em> bzw. <em>pio run -t buildfs</em> im Ordner
    <em>.pio/build/seeed_xiao_esp32s3/</em>. Die Uhr prüft die Prüfsumme, bevor
    sie auf das neue Image umschaltet — ein abgebrochener Upload macht also
    nichts kaputt, sie startet dann einfach mit der bisherigen Version.
  </p>
</section>

<style>
  .progress {
    height: 10px;
    margin: 0.75rem 0 0.25rem;
    border-radius: 999px;
    background: var(--border);
    overflow: hidden;
  }

  .bar {
    height: 100%;
    background: var(--accent);
    transition: width 0.15s linear;
  }

  .success {
    color: var(--accent);
  }
</style>
