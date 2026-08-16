<script>
  /**
   * Ota
   * Update tab: shows the installed versions and flashes an image picked in the
   * browser. The clock has two independent images - the firmware and the
   * filesystem holding this web UI - and works out from the file itself which
   * one it was handed.
   *
   * The second card covers the release channel: what the manifest of the
   * selected channel offers, and installing it - either on request or, if
   * switched on, by itself at night.
   *
   * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
   * @version  2.0
   * @created  15.8.2026
   * @updated  15.8.2026
   */
  import { onMount, onDestroy } from 'svelte';
  import * as api from '../lib/api.js';
  import { dict } from '../lib/i18n.svelte.js';

  const t = $derived(dict());

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
      error = t.noResponseAfterReboot;
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

  // ---- update from the release channel ----

  let checking = $state(false);
  let channelError = $state(null);

  const CHECK_INTERVALS = [0, 6, 12, 24, 72, 168];

  /** Poll while the clock downloads, then wait out the reboot. */
  async function follow() {
    for (let i = 0; i < 200 && !destroyed; i++) {
      await sleep(1500);
      if (!(await loadInfo({ quiet: true }))) continue;
      if (info.state === 'downloading') continue;
      if (info.state === 'installed') {
        phase = 'rebooting';
        if (await waitForClock()) {
          phase = 'done';
          await sleep(1500);
          location.reload();
        } else {
          phase = 'failed';
          error = t.noResponseAfterReboot;
        }
        return;
      }
      if (info.state === 'failed') {
        channelError = info.error;
        return;
      }
      return;
    }
  }

  async function check() {
    checking = true;
    channelError = null;
    try {
      info = await api.checkForUpdate();
      if (info.error) channelError = info.error;
    } catch (err) {
      channelError = err.message;
    }
    checking = false;
  }

  async function install() {
    channelError = null;
    try {
      info = await api.installUpdate();
      follow();
    } catch (err) {
      channelError = err.message;
    }
  }

  async function pushConfig(patch) {
    channelError = null;
    try {
      info = await api.setOtaConfig(patch);
    } catch (err) {
      channelError = err.message;
    }
  }

  /** "just now" / "12 min ago" / "3 h ago" from a count of seconds. */
  function since(seconds) {
    if (seconds === undefined || seconds < 0) return t.neverChecked;
    if (seconds < 90) return t.justNow;
    if (seconds < 5400) return t.minutesAgo(Math.round(seconds / 60));
    return t.hoursAgo(Math.round(seconds / 3600));
  }

  const busy = $derived(
    checking || info?.state === 'downloading' || phase === 'uploading' || phase === 'rebooting'
  );

  const kindLabel = $derived({
    firmware: t.firmwareImage,
    filesystem: t.filesystemImage
  });

  function size(bytes) {
    if (!bytes && bytes !== 0) return '—';
    return `${(bytes / 1024 / 1024).toFixed(2)} MB`;
  }

  onMount(loadInfo);
</script>

<section class="card">
  <h2>{t.installed}</h2>

  {#if infoError}
    <p class="banner">{t.statusUnavailable} — {infoError}</p>
  {:else if info}
    <div class="field">
      <span class="key">{t.firmware}</span><span>{info.firmwareVersion || '—'}</span>
    </div>
    <div class="field">
      <span class="key">{t.webUi}</span><span>{info.fsVersion || t.unknown}</span>
    </div>
    <div class="field"><span class="key">{t.used}</span><span>{size(info.sketchSize)}</span></div>
    <div class="field">
      <span class="key">{t.roomForUpdate}</span><span>{size(info.freeSpace)}</span>
    </div>
    {#if info.partition}
      <div class="field"><span class="key">{t.runningFrom}</span><span>{info.partition}</span></div>
    {/if}
  {:else}
    <p class="hint">{t.loadingShort}</p>
  {/if}
</section>

<section class="card">
  <h2>{t.updateSource}</h2>

  {#if info}
    <div class="field">
      <label for="channel">{t.channel}</label>
      <select
        id="channel"
        value={info.channel}
        disabled={busy}
        onchange={(e) => pushConfig({ channel: Number(e.currentTarget.value) })}
      >
        <option value={0}>{t.channelStable}</option>
        <option value={1}>{t.channelEdge}</option>
      </select>
    </div>

    <div class="field">
      <label for="interval">{t.checkInterval}</label>
      <select
        id="interval"
        value={info.checkInterval}
        disabled={busy}
        onchange={(e) => pushConfig({ checkInterval: Number(e.currentTarget.value) })}
      >
        {#each CHECK_INTERVALS as value (value)}
          <option {value}>{value === 0 ? t.checkNever : t.hours(value)}</option>
        {/each}
      </select>
    </div>

    <div class="field">
      <label for="auto">{t.autoUpdate}</label>
      <span class="switch">
        <input
          id="auto"
          type="checkbox"
          checked={info.autoUpdate}
          disabled={busy}
          onchange={(e) => pushConfig({ autoUpdate: e.currentTarget.checked })}
        />
        <span></span>
      </span>
    </div>
    <p class="hint">{t.autoUpdateHint}</p>

    <div class="field">
      <span class="key">{t.available}</span>
      <span>{info.availableVersion || '—'}</span>
    </div>

    {#if info.availableNotes}
      <p class="hint">{info.availableNotes}</p>
    {/if}

    {#if info.state === 'downloading'}
      <div class="progress" role="progressbar" aria-valuenow={info.progress}>
        <div class="bar" style="width: {info.progress}%"></div>
      </div>
      <p class="hint">{t.downloading(info.progress)}</p>
    {:else if info.updateAvailable}
      <button type="button" class="primary" onclick={install} disabled={busy}>
        {t.installNow}
      </button>
    {:else if info.lastCheck >= 0}
      <p class="hint success">{t.upToDate}</p>
    {/if}

    {#if channelError}
      <p class="banner">{channelError}</p>
    {/if}

    <div class="field">
      <span class="key">{since(info.lastCheck)}</span>
      <button type="button" class="secondary" onclick={check} disabled={busy}>
        {checking ? t.checking : t.checkNow}
      </button>
    </div>
  {:else}
    <p class="hint">{t.loadingShort}</p>
  {/if}
</section>

<section class="card">
  <h2>{t.uploadImage}</h2>

  <div class="field">
    <span class="key">{t.file}</span>
    <span class="filepick">
      <span class="filename" class:empty={!file}>{file ? file.name : t.noFile}</span>
      <!-- The input is hidden but still focusable; the label opens it. -->
      <input
        id="image"
        type="file"
        accept=".bin,application/octet-stream"
        onchange={pick}
        disabled={phase === 'uploading' || phase === 'rebooting'}
      />
      <label class="iconbutton" for="image" title={t.chooseFile} aria-label={t.chooseFile}>
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7"
             stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">
          <path d="M3 7.5A1.5 1.5 0 0 1 4.5 6h4L11 8h6a1.5 1.5 0 0 1 1.5 1.5V10" />
          <path d="M3 10h18l-2.1 7.6a1.5 1.5 0 0 1-1.45 1.4H4.5A1.5 1.5 0 0 1 3 17.5z" />
        </svg>
      </label>
    </span>
  </div>

  {#if file}
    <div class="field">
      <span class="key">{t.detectedAs}</span>
      <span>{kindLabel[kind]} · {size(file.size)}</span>
    </div>
  {/if}

  {#if kind === 'filesystem'}
    <p class="hint">{t.filesystemHint}</p>
  {/if}

  {#if phase === 'uploading' || phase === 'rebooting'}
    <div class="progress" role="progressbar" aria-valuenow={Math.round(progress * 100)}>
      <div class="bar" style="width: {Math.round(progress * 100)}%"></div>
    </div>
    <p class="hint">
      {#if phase === 'uploading'}
        {t.writing(Math.round(progress * 100))}
      {:else}
        {t.rebooting}
      {/if}
    </p>
  {/if}

  {#if phase === 'done'}
    <p class="hint success">
      {t.updateDone(info?.firmwareVersion, info?.fsVersion || t.unknown)}
    </p>
  {/if}

  {#if error}
    <p class="banner">{error}</p>
  {/if}

  <button
    type="button"
    class="primary"
    onclick={upload}
    disabled={!file || phase === 'uploading' || phase === 'rebooting'}
  >
    {phase === 'uploading' || phase === 'rebooting' ? t.running : t.upload}
  </button>

  <p class="hint">{t.buildHint}</p>
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
