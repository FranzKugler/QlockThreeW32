<script>
  /**
   * LuminanceSurface
   * The colour-aware model, drawn over both of its axes at once.
   *
   * The curve above this answers one question - how bright at how much light -
   * and the factory model answers two, because what a percentage is worth
   * depends on the colour the face is showing: the same setting emits about a
   * tenth as much light in full blue as in the green this clock runs. Six flat
   * charts, one per hue, would be six charts nobody compares. One surface is
   * the shape of the thing being described.
   *
   * **No 3D library.** A WebGL or Plotly bundle is hundreds of kilobytes into
   * a 3.5 MB partition that also holds the rest of this SPA, and nothing here
   * may come from a CDN - the clock has to work on a network with no internet
   * at all. What is actually needed is a projection and a painter's algorithm,
   * which is web/src/lib/surface3d.js, tested under `node --test` because the
   * parts that can be wrong without *looking* wrong - which cell is where,
   * whether the hue axis closes on itself - are exactly the parts a screenshot
   * cannot check.
   *
   * It does not rotate, and that is a choice rather than a shortfall: a fixed
   * isometric view is one a reader can compare against the one they saw
   * yesterday, and a draggable one is a control that has to be got back to
   * where it was before two of them mean the same thing.
   *
   * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
   * @version  2.2
   * @created  27.8.2026
   * @updated  27.8.2026
   */
  import { dict } from '../lib/i18n.svelte.js';
  import { projectSurface, bandColour } from '../lib/surface3d.js';

  const t = $derived(dict());

  let { surface = null, target = null, lux = null } = $props();

  const W = 340;
  const H = 230;

  // The operating point rides on the surface rather than beside it: the
  // percentage is already in the read-out above, and what the diagram adds is
  // *where on the model* the clock currently is.
  const point = $derived(
    surface?.valid && target && Number.isFinite(lux)
      ? { lux, hue: target.hue ?? 0, percent: target.percent ?? 0 }
      : null
  );

  const drawn = $derived(projectSurface(surface, { width: W, height: H, point }));

  /*
   * The whole picture in one sentence, for a reader who is not looking at it.
   *
   * An isometric surface is the least screen-reader-friendly thing on this
   * page, and `role="img"` with a label that said "diagram" would be worse
   * than nothing. What the diagram actually says is a range and a direction,
   * and both are numbers already in hand.
   */
  const summary = $derived.by(() => {
    if (drawn.empty) return t.lumSurfaceNone;
    const all = surface.percent.flat();
    return t.lumSurfaceSummary(
      surface.lux[0], surface.lux[surface.lux.length - 1],
      Math.min(...all), Math.max(...all));
  });
</script>

<h3>{t.lumSurfaceTitle}</h3>
<p class="hint">{t.lumSurfaceHint}</p>

{#if drawn.empty}
  <!-- Said rather than hidden. A diagram that quietly is not there reads as a
       page that failed to load, and the two want completely different things
       from the reader: one is a filesystem image older than the model, the
       other is a clock that did not answer. -->
  <p class="hint empty">{t.lumSurfaceNone}</p>
{:else}
  <svg viewBox="0 0 {W} {H}" role="img" aria-label={summary}>
    <!-- The floor, so the surface has something to stand on and the two
         bottom edges read as axes rather than as more surface. -->
    <path class="floor" d={`M${drawn.axes.lux[0].x} ${drawn.axes.lux[0].y}`
      + `L${drawn.axes.lux[drawn.axes.lux.length - 1].x} ${drawn.axes.lux[drawn.axes.lux.length - 1].y}`} />

    {#each drawn.quads as quad, i (i)}
      <!-- Painter's order: surface3d sorts by depth, so the cell nearest the
           reader is last in the array and therefore last on the screen. -->
      <path class="cell" class:limited={quad.limited} class:bound={quad.bound}
            d={quad.d} fill={quad.colour} />
    {/each}

    <!-- The wireframe over the fill rather than instead of it: the fill
         carries the hue, the wire carries the shape, and on a phone the fill
         alone reads as a flat patchwork. -->
    {#each drawn.wires as wire, i (i)}
      <path class="wire" d={wire.d} />
    {/each}

    {#each drawn.axes.lux as tick (tick.value)}
      <text class="tick" x={tick.x} y={tick.y + 11} text-anchor="middle">{tick.label}</text>
    {/each}
    {#each drawn.axes.hue as tick (tick.value)}
      <circle class="swatch" cx={tick.x} cy={tick.y + 8} r="3" fill={tick.colour} />
      <text class="tick" x={tick.x} y={tick.y + 20} text-anchor="middle">{tick.label}</text>
    {/each}
    {#each drawn.axes.percent as tick (tick.value)}
      <text class="tick" x={tick.x - 8} y={tick.y + 3} text-anchor="end">{tick.label}</text>
    {/each}

    {#if drawn.point}
      <!-- Two circles rather than one: the ring survives being drawn over a
           band of its own colour, which a filled dot does not. -->
      <circle class="here ring" cx={drawn.point.x} cy={drawn.point.y} r="6" />
      <circle class="here dot" cx={drawn.point.x} cy={drawn.point.y} r="2.5" />
    {/if}

    <text class="axis" x={W / 2} y={H - 2} text-anchor="middle">lx · °· %</text>
  </svg>

  <ul class="legend">
    <li><span class="chip here-chip"></span>{t.lumSurfaceHere}</li>
    <li><span class="chip limited-chip"></span>{t.lumSurfaceLimited}</li>
    <li><span class="chip bound-chip"></span>{t.lumSurfaceBound}</li>
  </ul>
  <p class="hint small">{summary}</p>
{/if}

<style>
  svg {
    width: 100%;
    height: auto;
    /* The face of this diagram is the page, not a printed clock face - unlike
       the letter preview in the colour tab, which stays pale in either theme
       because a physical panel does. */
    background: var(--surface);
    border-radius: 6px;
    margin-top: 0.4rem;
  }

  .cell {
    stroke: rgb(0 0 0 / 0.25);
    stroke-width: 0.4;
    /* Translucent, so the wireframe under a near band still shows the ridge of
       a far one - which is the only depth cue an isometric view has. */
    fill-opacity: 0.62;
  }

  /* The colour has run out of slider: it cannot emit what the model asks for
     at any percentage in the range. Hatched rather than recoloured, because
     the fill is the hue and overwriting it would lose the axis. */
  .cell.limited { fill-opacity: 0.28; stroke-dasharray: 2 1.5; }

  /* A corner of the measurement that only ever said "at least this much".
     A different statement from the one above, so a different mark - and a cell
     can carry both. */
  .cell.bound { stroke: var(--text); stroke-width: 0.8; stroke-opacity: 0.45; }

  .wire { fill: none; stroke: var(--text); stroke-opacity: 0.32; stroke-width: 0.7; }
  .floor { stroke: var(--text); stroke-opacity: 0.25; stroke-width: 0.7; fill: none; }

  .tick { font-size: 7px; fill: var(--text); opacity: 0.65; }
  .axis { font-size: 8px; fill: var(--text); opacity: 0.5; }
  .swatch { stroke: var(--text); stroke-opacity: 0.3; stroke-width: 0.4; }

  .here.ring { fill: none; stroke: var(--text); stroke-width: 1.6; }
  .here.dot { fill: var(--text); }

  .legend {
    list-style: none;
    display: flex;
    flex-wrap: wrap;
    gap: 0.15rem 0.9rem;
    padding: 0;
    margin: 0.35rem 0 0;
    font-size: 0.78rem;
    opacity: 0.8;
  }
  .legend li { display: flex; align-items: center; gap: 0.3rem; }

  .chip {
    width: 0.7rem;
    height: 0.7rem;
    border-radius: 2px;
    display: inline-block;
    border: 1px solid currentColor;
  }
  .here-chip { border-radius: 50%; background: currentColor; }
  .limited-chip { opacity: 0.35; background: currentColor; border-style: dashed; }
  .bound-chip { background: none; border-width: 2px; }

  .empty { font-style: italic; }
  .small { font-size: 0.78rem; }
</style>
