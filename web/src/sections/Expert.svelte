<script>
  /**
   * Expert
   * Setting, entering and clearing the password that opens the update and
   * debug tabs.
   *
   * Not a tab of its own: there is nothing here for someone who has not gone
   * looking, and a chip in the row would only invite guessing. It is reached
   * through `#expert` in the address, which the shell watches for.
   *
   * The three states it can be in are the clock's, not this component's, and
   * they come from `/expert` on every answer - so a second browser that
   * enrolled the clock a moment ago is noticed here rather than argued with.
   *
   * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
   * @version  2.1
   * @created  20.8.2026
   * @updated  20.8.2026
   */
  import * as api from '../lib/api.js';
  import { dict } from '../lib/i18n.svelte.js';
  import { errorText } from '../lib/errors.js';

  let { expert, onchange } = $props();

  const t = $derived(dict());

  let password = $state('');
  let busy = $state(false);
  let error = $state(null);

  const MIN_LENGTH = 6; // the firmware refuses anything shorter

  async function submit() {
    if (busy || password.length < MIN_LENGTH) return;
    busy = true;
    error = null;

    const answer = await api.setExpert({ password });
    // Cleared either way. A wrong one is not worth offering again unchanged,
    // and a right one has no business staying in a form field.
    password = '';
    busy = false;

    if (answer.error) {
      error = errorText(t, answer.error);
      return;
    }
    onchange(answer);
  }

  async function lock() {
    busy = true;
    const answer = await api.setExpert({ off: true });
    busy = false;
    if (!answer.error) onchange(answer);
  }

  async function clearPassword() {
    busy = true;
    error = null;
    const answer = await api.setExpert({ reset: true });
    busy = false;
    if (answer.error) {
      error = errorText(t, answer.error);
      return;
    }
    onchange(answer);
  }

  /** The countdown on the reset window, as m:ss. */
  const graceText = $derived(
    `${Math.floor(expert.grace / 60)}:${String(expert.grace % 60).padStart(2, '0')}`
  );
</script>

<section class="card">
  <h2>{t.expertTitle}</h2>

  {#if expert.unlocked}
    <p class="hint">{t.expertUnlocked}</p>
    <div class="actions">
      <button type="button" disabled={busy} onclick={lock}>{t.expertLock}</button>
    </div>
  {:else}
    <p class="hint">{expert.enrolled ? t.expertEnterHint : t.expertSetHint}</p>

    <div class="field">
      <label for="expert-password">{t.expertPassword}</label>
      <input
        id="expert-password"
        type="password"
        autocomplete={expert.enrolled ? 'current-password' : 'new-password'}
        bind:value={password}
        disabled={busy || expert.lockedOut}
        onkeydown={(e) => e.key === 'Enter' && submit()}
      />
    </div>

    <div class="actions">
      <button
        type="button"
        disabled={busy || expert.lockedOut || password.length < MIN_LENGTH}
        onclick={submit}
      >
        {expert.enrolled ? t.expertUnlock : t.expertSet}
      </button>
    </div>

    {#if !expert.enrolled}
      <p class="hint">{t.expertMinLength(MIN_LENGTH)}</p>
    {/if}

    {#if expert.lockedOut}
      <p class="banner">{t.expertLockedOut}</p>
    {/if}
  {/if}

  {#if error}
    <p class="banner">{error}</p>
  {/if}
</section>

{#if expert.grace > 0 && expert.enrolled}
  <!--
    Only while the window is open, and the window only opens on a power-on
    reset - so this card is not something a visitor can conjure up over the
    network. It is the way back from a forgotten password, and it says so.
  -->
  <section class="card">
    <h2>{t.expertForgotten}</h2>
    <p class="hint">{t.expertResetHint(graceText)}</p>
    <div class="actions">
      <button type="button" class="danger" disabled={busy} onclick={clearPassword}>
        {t.expertReset}
      </button>
    </div>
  </section>
{/if}

<style>
  .actions {
    display: flex;
    gap: 0.5rem;
    margin-top: 0.75rem;
  }

  .danger {
    border-color: var(--danger);
    color: var(--danger);
  }
</style>
