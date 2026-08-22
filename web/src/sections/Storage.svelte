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
    <Explorer store={stores.nvs} hint={t.storageNvsHint} warning={t.storageNvsWarn} />
  {/if}
</section>

<style>
  .switch {
    display: flex;
    gap: 0.35rem;
    margin: 0 0 1rem;
    padding: 0.2rem;
    background: var(--bg);
    border: 1px solid var(--border);
    border-radius: var(--radius);
    /* Two of them, so they share the width rather than huddling on the left. */
    width: fit-content;
  }

  .switch button {
    background: none;
    border: none;
    border-radius: calc(var(--radius) - 3px);
    padding: 0.35rem 0.9rem;
    font: inherit;
    font-size: 0.9rem;
    color: var(--muted);
    cursor: pointer;
  }

  .switch button.on {
    background: var(--card);
    color: var(--fg);
    font-weight: 600;
    box-shadow: 0 1px 3px rgb(0 0 0 / 0.12);
  }
</style>
