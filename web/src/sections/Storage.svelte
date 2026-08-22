<script>
  /**
   * Storage
   * The clock's two persistent stores, side by side.
   *
   * They are genuinely different things - one is a filesystem, the other a
   * key-value store - and they are shown the same way because the question
   * somebody arrives with is the same for both: what is in there, and can I
   * get it out. Naming them by what they are rather than by a metaphor is
   * deliberate; "LittleFS" and "NVS" are the words in every ESP32 document and
   * in this project's own logs, so somebody searching for why their settings
   * vanished finds the tab that holds them.
   *
   * What each one is gets a line of its own below the switch, because the
   * difference matters the moment anything is edited: LittleFS is wiped whole
   * by a filesystem update, NVS survives one. That is the whole reason the
   * settings live where they do.
   *
   * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
   * @version  2.2
   * @created  22.8.2026
   * @updated  22.8.2026
   */
  import Explorer from './Explorer.svelte';
  import { littleFs, nvs } from '../lib/explorers.js';
  import { dict } from '../lib/i18n.svelte.js';
  import { restartClock } from '../lib/api.js';

  const t = $derived(dict());

  /* Built once. The NVS adapter caches a snapshot of the whole partition, so
     rebuilding it on every render would throw that away on each keystroke. */
  const stores = { littlefs: littleFs(), nvs: nvs() };

  let which = $state('littlefs');
</script>

<section class="card">
  <h2>{t.storageTitle}</h2>

  <div class="switch" role="tablist" aria-label={t.storageTitle}>
    <button type="button" role="tab" class:on={which === 'littlefs'}
            aria-selected={which === 'littlefs'} onclick={() => (which = 'littlefs')}>
      {t.storageFs}
    </button>
    <button type="button" role="tab" class:on={which === 'nvs'}
            aria-selected={which === 'nvs'} onclick={() => (which = 'nvs')}>
      {t.storageNvs}
    </button>
  </div>

  {#if which === 'littlefs'}
    <Explorer store={stores.littlefs} hint={t.storageFsHint} warning={t.storageFsWarn} />
  {:else}
    <!-- Only NVS gets the restart button: it is the answer to that panel's
         warning, and nothing on the filesystem side needs one. -->
    <Explorer store={stores.nvs} hint={t.storageNvsHint} warning={t.storageNvsWarn}
              onRestart={restartClock} />
  {/if}
</section>

<style>
  /*
   * Underlined tabs, the same idiom as the tab row at the top of the page.
   * A segmented control was tried first and read badly: two greys on a third
   * grey, with the selected one told apart only by a faint shadow. Tabs say
   * "these are two views of one thing" without needing contrast to carry the
   * whole message, and the page already teaches the gesture once.
   */
  .switch {
    display: flex;
    gap: 0.25rem;
    margin: 0 0 1rem;
    border-bottom: 1px solid var(--border);
  }

  .switch button {
    appearance: none;
    background: none;
    border: none;
    border-bottom: 2px solid transparent;
    margin-bottom: -1px;
    padding: 0.5rem 0.9rem;
    font: inherit;
    font-size: 0.9rem;
    color: var(--muted);
    cursor: pointer;
  }

  .switch button:hover {
    color: var(--text);
  }

  .switch button.on {
    color: var(--accent);
    border-bottom-color: var(--accent);
    font-weight: 600;
  }

  .switch button:focus-visible {
    outline: 2px solid var(--accent);
    outline-offset: -2px;
  }
</style>
