<script>
  /**
   * Files
   * A file explorer for the clock's LittleFS, above the log in the debug tab.
   *
   * **Not NVS.** That is a key-value store - one JSON string under `settings`,
   * another under `curve` - with no tree and nothing to download. What has
   * files is the partition this page is served from, which is also the whole
   * danger: deleting index.html leaves a clock that answers every endpoint and
   * shows nothing, and the way back is a USB cable. Hence the inline
   * confirmation on delete and the warning in the hint.
   *
   * The tree is expanded lazily, one directory per request, because that is
   * what the firmware offers - a directory somebody filled costs one slow
   * response rather than making every response slow. State is kept flat, as a
   * path-keyed object of listings plus a set of open paths, rather than as a
   * recursive component: there is one place a refresh has to touch, and a
   * delete three levels down does not have to find its own node.
   *
   * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
   * @version  2.2
   * @created  22.8.2026
   * @updated  22.8.2026
   */
  import { onMount } from 'svelte';
  import * as api from '../lib/api.js';
  import { dict } from '../lib/i18n.svelte.js';

  const t = $derived(dict());

  /** Listings by directory path. Plain objects, reassigned - a $state Map
      would need SvelteMap to be reactive, and there is nothing to gain. */
  let dirs = $state({});
  let open = $state({ '/': true });
  let volume = $state(null);
  let error = $state(null);

  /** The row something is happening to, so its buttons can be disabled. */
  let busy = $state(null);
  let confirming = $state(null);
  let editing = $state(null);
  let note = $state(null);

  /** Where the upload and the new folder land: the last folder clicked. */
  let target = $state('/');
  let progress = $state(null);
  let creating = $state(null);
  let picker = $state(null);

  const join = (dir, name) => (dir === '/' ? `/${name}` : `${dir}/${name}`);

  async function load(path) {
    try {
      const answer = await api.fetchDirectory(path);
      volume = { total: answer.total, used: answer.used, editMax: answer.editMax };
      // Directories first, then by name - the order LittleFS hands them out in
      // is the order they were written, which is no order at all to read.
      const entries = [...answer.entries].sort((a, b) =>
        a.dir === b.dir ? a.name.localeCompare(b.name) : a.dir ? -1 : 1
      );
      dirs = { ...dirs, [path]: { entries, truncated: !!answer.truncated } };
      error = null;
    } catch (err) {
      error = err.message;
    }
  }

  onMount(() => load('/'));

  async function toggle(path) {
    target = path;
    if (open[path]) {
      open = { ...open, [path]: false };
      return;
    }
    open = { ...open, [path]: true };
    if (!dirs[path]) await load(path);
  }

  /** The visible rows, flattened out of whatever is open and already loaded. */
  const rows = $derived.by(() => {
    const out = [];
    const walk = (path, depth) => {
      const here = dirs[path];
      if (!here) return;
      for (const entry of here.entries) {
        const full = join(path, entry.name);
        out.push({ ...entry, path: full, depth });
        if (entry.dir && open[full]) walk(full, depth + 1);
      }
      if (here.truncated) out.push({ truncated: true, path: `${path}#more`, depth });
    };
    walk('/', 0);
    return out;
  });

  /** The directory a path sits in, which is the listing to refresh after it. */
  const parentOf = (path) => {
    const cut = path.lastIndexOf('/');
    return cut <= 0 ? '/' : path.substring(0, cut);
  };

  async function remove(path) {
    busy = path;
    confirming = null;
    try {
      const answer = await api.deleteEntry(path);
      volume = { ...volume, used: answer.used };
      // Whatever was known about it below is gone too, listing included.
      const { [path]: _dropped, ...rest } = dirs;
      dirs = rest;
      await load(parentOf(path));
      if (editing?.path === path) editing = null;
      note = null;
    } catch (err) {
      error = err.message;
    } finally {
      busy = null;
    }
  }

  async function edit(entry) {
    busy = entry.path;
    try {
      const text = await api.fetchFile(entry.path);
      editing = { path: entry.path, text, saving: false };
      note = null;
      error = null;
    } catch (err) {
      error = err.message;
    } finally {
      busy = null;
    }
  }

  async function save() {
    editing.saving = true;
    try {
      const answer = await api.saveFile(editing.path, editing.text);
      volume = { ...volume, used: answer.used };
      await load(parentOf(editing.path));
      note = t.fsSaved;
      editing = null;
      error = null;
    } catch (err) {
      error = err.message;
    } finally {
      if (editing) editing.saving = false;
    }
  }

  async function upload(event) {
    const file = event.target.files?.[0];
    // Cleared straight away, or picking the same file twice fires nothing.
    event.target.value = '';
    if (!file) return;

    progress = 0;
    try {
      const answer = await api.uploadFile(join(target, file.name), file, (done) => {
        progress = Math.round(done * 100);
      });
      volume = { ...volume, used: answer.used };
      await load(target);
      open = { ...open, [target]: true };
      error = null;
    } catch (err) {
      error = err.message;
    } finally {
      progress = null;
    }
  }

  async function createFolder() {
    const name = creating.trim();
    if (!name) { creating = null; return; }
    try {
      await api.makeDirectory(join(target, name));
      await load(target);
      open = { ...open, [target]: true };
      creating = null;
      error = null;
    } catch (err) {
      error = err.message;
    }
  }

  const bytes = (n) =>
    n < 1024 ? `${n} B` : n < 1024 * 1024 ? `${(n / 1024).toFixed(1)} kB`
                                          : `${(n / 1048576).toFixed(2)} MB`;
</script>

<section class="card">
  <h2>{t.fsTitle}</h2>
  <p class="hint">{t.fsHint}</p>

  {#if error}<p class="banner">{error}</p>{/if}
  {#if note}<p class="hint ok">{note}</p>{/if}

  {#if volume}
    <div class="field">
      <span class="key">{t.fsUsage(bytes(volume.used), bytes(volume.total))}</span>
      <span class="bar" aria-hidden="true">
        <span style="width: {Math.min(100, (volume.used / volume.total) * 100)}%"></span>
      </span>
    </div>
  {/if}

  <div class="actions">
    <button type="button" onclick={() => picker.click()} disabled={progress !== null}>
      {progress === null ? t.fsUpload : t.fsUploading(progress)}
    </button>
    <button type="button" onclick={() => (creating = '')} disabled={creating !== null}>
      {t.fsNewFolder}
    </button>
    <!-- Which folder the two above act on. Clicking a folder moves it. -->
    <code class="target">{target}</code>
    <input type="file" bind:this={picker} onchange={upload} hidden />
  </div>

  {#if creating !== null}
    <div class="actions">
      <input type="text" bind:value={creating} placeholder={t.fsFolderName}
             onkeydown={(e) => e.key === 'Enter' && createFolder()} />
      <button type="button" onclick={createFolder}>{t.fsSave}</button>
      <button type="button" onclick={() => (creating = null)}>{t.fsCancel}</button>
    </div>
  {/if}

  <div class="tree" role="tree">
    <!-- The root is a row of its own so it can be the upload target too. -->
    <div class="row" class:here={target === '/'} role="treeitem" aria-selected={target === '/'}>
      <button type="button" class="name" onclick={() => (target = '/')}>
        <span class="twist">▾</span><span class="folder">{t.fsRoot}</span>
      </button>
    </div>

    {#each rows as row (row.path)}
      {#if row.truncated}
        <div class="row note" style="--depth: {row.depth + 1}">{t.fsTruncated}</div>
      {:else}
        <div class="row" class:here={target === row.path} style="--depth: {row.depth + 1}"
             role="treeitem" aria-selected={target === row.path}
             aria-expanded={row.dir ? !!open[row.path] : undefined}>
          {#if row.dir}
            <button type="button" class="name" onclick={() => toggle(row.path)}>
              <span class="twist">{open[row.path] ? '▾' : '▸'}</span>
              <span class="folder">{row.name}</span>
            </button>
          {:else}
            <span class="name file"><span class="twist"></span>{row.name}</span>
            <span class="size">{bytes(row.size)}</span>
          {/if}

          <span class="tools">
            {#if confirming === row.path}
              <button type="button" class="danger" onclick={() => remove(row.path)}>
                {t.fsConfirmDelete(row.name)}
              </button>
              <button type="button" onclick={() => (confirming = null)}>{t.fsCancel}</button>
            {:else}
              {#if !row.dir}
                <a href={api.fileUrl(row.path)} download={row.name}>{t.fsDownload}</a>
                {#if row.edit}
                  <button type="button" onclick={() => edit(row)} disabled={busy === row.path}>
                    {t.fsEdit}
                  </button>
                {/if}
              {/if}
              <button type="button" onclick={() => (confirming = row.path)}
                      disabled={busy === row.path}>{t.fsDelete}</button>
            {/if}
          </span>
        </div>
      {/if}
    {/each}

    {#if dirs['/'] && dirs['/'].entries.length === 0}
      <p class="hint">{t.fsEmpty}</p>
    {/if}
  </div>

  {#if editing}
    <div class="editor">
      <code class="target">{editing.path}</code>
      <textarea bind:value={editing.text} spellcheck="false" rows="18"></textarea>
      <div class="actions">
        <button type="button" onclick={save} disabled={editing.saving}>{t.fsSave}</button>
        <button type="button" onclick={() => (editing = null)}>{t.fsCancel}</button>
      </div>
    </div>
  {/if}
</section>

<style>
  .bar {
    flex: 1;
    height: 8px;
    margin-left: 0.75rem;
    border-radius: 4px;
    background: var(--border);
    overflow: hidden;
  }

  .bar > span {
    display: block;
    height: 100%;
    background: var(--accent);
  }

  .actions {
    display: flex;
    gap: 0.5rem;
    align-items: center;
    flex-wrap: wrap;
    margin: 0.75rem 0;
  }

  .target {
    font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
    font-size: 0.8rem;
    color: var(--muted);
  }

  .tree {
    border: 1px solid var(--border);
    border-radius: var(--radius);
    background: var(--bg);
    padding: 0.35rem 0.5rem;
    font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
    font-size: 0.82rem;
    /* A deep tree or a long name scrolls here rather than widening the page. */
    overflow-x: auto;
  }

  .row {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    padding: 0.15rem 0;
    /* One step of indent per level, set from the row itself. */
    padding-left: calc(var(--depth, 0) * 1.1rem);
    border-radius: 4px;
  }

  .row.here {
    background: color-mix(in srgb, var(--accent) 12%, transparent);
  }

  .row.note {
    color: var(--muted);
    font-style: italic;
  }

  .name {
    display: flex;
    align-items: center;
    gap: 0.35rem;
    flex: 1;
    min-width: 0;
    /* The folder row is a button, so it needs stripping back to a plain row. */
    background: none;
    border: none;
    padding: 0;
    margin: 0;
    font: inherit;
    color: inherit;
    text-align: left;
    cursor: pointer;
    white-space: nowrap;
  }

  .name.file {
    cursor: default;
  }

  .twist {
    display: inline-block;
    width: 1em;
    flex: none;
    color: var(--muted);
  }

  .folder {
    font-weight: 600;
  }

  .size {
    color: var(--muted);
    font-variant-numeric: tabular-nums;
    flex: none;
  }

  .tools {
    display: flex;
    gap: 0.35rem;
    align-items: center;
    flex: none;
  }

  .tools button,
  .tools a {
    font-size: 0.75rem;
    padding: 0.1rem 0.45rem;
  }

  .tools a {
    color: var(--accent);
  }

  .tools .danger {
    color: var(--danger);
    border-color: var(--danger);
  }

  .editor {
    margin-top: 1rem;
  }

  .editor textarea {
    width: 100%;
    box-sizing: border-box;
    font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
    font-size: 0.8rem;
    line-height: 1.45;
    white-space: pre;
    /* No wrapping: a config file is read by its columns. */
    overflow-wrap: normal;
    overflow-x: auto;
  }

  .hint.ok {
    color: var(--accent);
  }
</style>
