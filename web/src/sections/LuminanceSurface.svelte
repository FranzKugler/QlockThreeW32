<script>
  /**
   * LuminanceSurface
   * The colour-aware model, drawn over both of its axes at once - as a cylinder.
   *
   * The curve above this answers one question - how bright at how much light -
   * and the factory model answers two, because what a percentage is worth
   * depends on the colour the face is showing: the same setting emits about a
   * tenth as much light in full blue as in the green this clock runs. Six flat
   * charts, one per hue, would be six charts nobody compares. One surface is
   * the shape of the thing being described.
   *
   * **Hue is an angle, so the diagram is round.** A straight hue axis has to
   * cut the wheel somewhere, and wherever the cut goes the drawing grows an
   * edge the model does not have, with red on one side and violet on the other
   * and nothing saying they are neighbours. Wrapped into a cylinder the seam is
   * a joint: azimuth is hue, radius is ambient light, height is the percentage
   * the model asks for. The radius is **log light** - the knots run 0.02 lx to
   * 10, a factor of five hundred, and on a linear radius every evening this
   * clock is read in would land inside the innermost two per cent of the disc.
   *
   * **No 3D library.** A WebGL or Plotly bundle is hundreds of kilobytes into
   * a 3.5 MB partition that also holds the rest of this SPA, and nothing here
   * may come from a CDN - the clock has to work on a network with no internet
   * at all. What is actually needed is a rotation, an orthographic projection
   * and a painter's algorithm, which is web/src/lib/surface3d.js, tested under
   * `node --test` because the parts that can be wrong without *looking* wrong -
   * which cell is where, whether the seam closes, whether the drawing stays in
   * its box at every angle - are exactly the parts a screenshot cannot check.
   *
   * **It turns, and there is always a way back.** A closed cylinder hides half
   * of itself behind the other half, and no single angle answers "what does my
   * hue do" for every hue, so the view has to be movable. What that costs is
   * comparability with the picture somebody saw yesterday, which is why the
   * reset is a visible button and the Home key, and why both return to exactly
   * `DEFAULT_VIEW` rather than to something near it.
   *
   * **Nothing here animates.** The view follows the finger and the key press
   * and stops when they do, so there is no motion to sit out and nothing for
   * `prefers-reduced-motion` to switch off. That is the reason there is no
   * spin-to-rest and no eased reset, rather than an omission.
   *
   * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
   * @version  2.2
   * @created  27.8.2026
   * @updated  28.8.2026
   */
  import { dict } from '../lib/i18n.svelte.js';
  import { dragGesture } from '../lib/gesture.js';
  import {
    projectSurface, bandColour,
    DEFAULT_VIEW, normaliseView, isDefaultView, dragView, keyView
  } from '../lib/surface3d.js';

  const t = $derived(dict());

  let { surface = null, target = null, lux = null, residuals = [] } = $props();

  const W = 340;
  const H = 250;

  let view = $state({ ...DEFAULT_VIEW });

  // The operating point rides on the surface rather than beside it: the
  // percentage is already in the read-out above, and what the diagram adds is
  // *where on the model* the clock currently is.
  const point = $derived(
    surface?.valid && target && Number.isFinite(lux)
      ? { lux, hue: target.hue ?? 0, percent: target.percent ?? 0 }
      : null
  );

  /*
   * Table 2 - what the owner has taught the clock - on the same surface.
   *
   * `percent` arrives already computed by the firmware (see WebRoutes.cpp's
   * describeFactory()): converting a taught decades-residual back to a
   * percentage is the same inversion the surface itself is built from, and a
   * second implementation here would be a second place for the two to
   * quietly disagree. A point missing it - an older firmware, or the factory
   * profile not loaded when it was taught - is left off rather than guessed
   * at, and white is left off on principle: it has no hue, and a diagram
   * whose one axis *is* hue has nowhere honest to put it.
   */
  const taught = $derived(
    surface?.valid
      ? (residuals ?? []).filter((one) => one.sat > 0 && Number.isFinite(one.percent))
          .map((one) => ({ lux: one.lux, hue: one.hue, percent: one.percent, bound: one.bound }))
      : []
  );

  const drawn = $derived(
    projectSurface(surface, { width: W, height: H, point, points: taught, view })
  );

  const azimuth = $derived(Math.round(drawn.view.azimuth));
  const tilt = $derived(Math.round(drawn.view.tilt));
  const isDefault = $derived(isDefaultView(drawn.view));

  /*
   * The whole picture in one sentence, for a reader who is not looking at it.
   *
   * A rotated 3D surface is the least screen-reader-friendly thing on this
   * page, and `role="img"` with a label that said "diagram" would be worse
   * than nothing. What the diagram actually says is a range, a direction and
   * which quantity is which axis - and all of it is numbers already in hand.
   * The mapping sentence is part of the label rather than only in the prose,
   * because a listener has no way to see that the radius is logarithmic.
   */
  const summary = $derived(
    drawn.empty
      ? t.lumSurfaceNone
      : `${t.lumSurfaceSummary(
          surface.lux[0], surface.lux[surface.lux.length - 1],
          Math.min(...surface.percent.flat()), Math.max(...surface.percent.flat()))} `
        + `${t.lumSurfaceRadius}`
  );

  function turn(key) {
    view = keyView(view, key).view;
  }

  function reset() {
    view = { ...DEFAULT_VIEW };
  }

  function onKey(event) {
    if (event.altKey || event.ctrlKey || event.metaKey) return;
    const answer = keyView(view, event.key);
    if (!answer.handled) return;
    // Only once it is ours: an unhandled key has to stay the page's, or Tab
    // stops moving focus out of a diagram somebody cannot leave.
    event.preventDefault();
    view = answer.view;
  }

  /**
   * Dragging, on whatever the browser has.
   *
   * An action rather than markup handlers, because the two input models must
   * not both be attached: a browser with Pointer Events fires `pointerdown`
   * *and* `mousedown` for the same gesture, so binding both would turn the
   * cylinder twice per pixel. The feature is tested for rather than the
   * browser, and the mouse/touch pair is the fallback for the older phones
   * that never got Pointer Events - which are exactly the phones a wall clock
   * ends up being configured from.
   *
   * The move and release listeners go on the window, not the node: a finger
   * that leaves the diagram mid-turn is still turning it, and a release
   * outside would otherwise never arrive and leave the drag stuck on.
   *
   * **Exactly one input owns the turn**, and which one is `dragGesture()`'s to
   * say rather than this action's - a second finger or a stylus set down
   * beside a thumb fires the same events at the same window listeners, and a
   * drag that keeps a position and no identity reads them as the same hand.
   * See web/src/lib/gesture.js and the tests beside it; nothing here decides
   * whose event it is, it only asks.
   */
  function rotatable(node) {
    const pointerAware = typeof window !== 'undefined' && 'PointerEvent' in window;
    const drag = dragGesture();

    // Only a real pointer id was ever captured, so only one can be released:
    // the mouse's stand-in identity is not a number and belongs to nothing the
    // element ever took hold of.
    const letGo = (id) => {
      if (typeof id !== 'number' || !node.releasePointerCapture) return;
      try {
        if (node.hasPointerCapture && !node.hasPointerCapture(id)) return;
        node.releasePointerCapture(id);
      } catch { /* not fatal */ }
    };

    const start = (event) => {
      const held = drag.start(event);
      // A refusal is a second input, a button that is not the left one, or an
      // event with no position in it - and in all three the drag already in
      // progress stays exactly as it was.
      if (!held) return;
      node.focus?.();
      // Capture where it exists, so a drag that runs off the element keeps
      // getting events even without the window listeners below. Guarded
      // rather than assumed: it is missing on the same old browsers the
      // fallback path is there for.
      if (typeof held.id === 'number' && node.setPointerCapture) {
        try { node.setPointerCapture(held.id); } catch { /* not fatal */ }
      }
    };

    const move = (event) => {
      const step = drag.move(event);
      if (!step) return;
      const box = node.getBoundingClientRect();
      view = dragView(view, step.dx, step.dy,
                      { width: box.width, height: box.height });
      // `touch-action: none` below is what actually stops the page scrolling
      // under the finger; this is the same thing for browsers too old to
      // honour it, and it is why the touch listeners are not passive.
      if (event.cancelable) event.preventDefault();
    };

    const end = (event) => {
      const done = drag.end(event);
      if (!done) return;
      letGo(done.id);
    };

    const on = [];
    const bind = (target, type, handler, options) => {
      target.addEventListener(type, handler, options);
      on.push(() => target.removeEventListener(type, handler, options));
    };

    if (pointerAware) {
      bind(node, 'pointerdown', start);
      bind(window, 'pointermove', move, { passive: false });
      bind(window, 'pointerup', end);
      bind(window, 'pointercancel', end);
    } else {
      bind(node, 'mousedown', start);
      bind(window, 'mousemove', move);
      bind(window, 'mouseup', end);
      bind(node, 'touchstart', start, { passive: false });
      bind(window, 'touchmove', move, { passive: false });
      bind(window, 'touchend', end);
      bind(window, 'touchcancel', end);
    }

    return {
      destroy() {
        // The element can go away with a finger still on it - a tab switched
        // while turning - so the capture is released against the id that was
        // actually captured rather than left on a node nobody holds.
        letGo(drag.stop()?.id);
        for (const off of on) off();
      }
    };
  }
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
  <!-- `application` rather than `img` with a tabindex: this really is a widget
       whose arrow keys do something, and that role is what makes a screen
       reader hand the arrows over instead of using them to move its own
       cursor. The buttons underneath do the same job for anyone that role
       serves badly, so nothing here is reachable only through the graphic.

       The two suppressions below are the linter's list, not the markup:
       Svelte's a11y rules take their interactive roles from the widget set,
       which has `slider` and `tab` and `button` in it and does not have
       `application` - so a div carrying it reads to them as a plain div that
       has grown a tabindex and a keydown. Naming the two rules rather than
       silencing the file keeps every other a11y warning in this component
       live. -->
  <!-- svelte-ignore a11y_no_noninteractive_element_interactions -->
  <!-- svelte-ignore a11y_no_noninteractive_tabindex -->
  <div class="stage" role="application" tabindex="0"
       aria-label={summary} aria-describedby="lum-surface-help"
       aria-keyshortcuts="ArrowLeft ArrowRight ArrowUp ArrowDown Home"
       onkeydown={onKey} use:rotatable>
    <svg viewBox="0 0 {W} {H}" role="img" aria-hidden="true">
      <!-- The floor first, so it is behind everything: one ring per ambient
           level is what makes the log radius legible. Without it the rings
           are only implied by where the colour bands bend, and "which circle
           is one lux" has no answer on the screen. -->
      {#each drawn.floor.spokes as spoke (spoke.value)}
        <path class="spoke" d={spoke.d} />
      {/each}
      {#each drawn.floor.rings as ring (ring.lux)}
        <path class="ring" d={ring.d} />
      {/each}

      {#each drawn.quads as quad, i (i)}
        <!-- Painter's order: surface3d sorts from the back, so the far side of
             the cylinder is drawn first and the near side over it. Each cell
             carries its own outline, which is the wireframe - drawn this way
             round it is occluded along with the cell it belongs to, where a
             separate wireframe layer would show the far ribs through the near
             surface. -->
        <path class="cell" class:limited={quad.limited} class:bound={quad.bound}
              d={quad.d} fill={quad.colour} />
      {/each}

      {#each drawn.axes.lux as tick (tick.value)}
        <text class="tick" x={tick.x} y={tick.y + 10} text-anchor="middle">{tick.label}</text>
      {/each}
      <!-- Far labels are faded, not dropped: a rim that lost half its numbers
           when turned would read as a drawing error rather than as depth. -->
      {#each drawn.axes.hue as tick (tick.value)}
        <g class="huetick" class:far={tick.depth > 0}>
          <circle class="swatch" cx={tick.x} cy={tick.y} r="3" fill={tick.colour} />
          <text class="tick" x={tick.x} y={tick.y + 12} text-anchor="middle">{tick.label}</text>
        </g>
      {/each}
      {#each drawn.axes.percent as tick (tick.value)}
        <text class="tick" x={tick.x - 4} y={tick.y + 3} text-anchor="end">{tick.label}</text>
      {/each}

      <!-- Table 2, before the "here" marker so the current point is never
           hidden under a taught one sitting at the same spot. A square
           rather than a circle - the one shape not already used on this
           diagram - and faded on the far side of the cylinder the same way
           the hue labels are, or a diagram that only ever shows the near
           half of what was taught would look like half a calibration. -->
      {#each drawn.points as mark, i (i)}
        <rect class="taught" class:bound={mark.bound} class:far={mark.depth > 0}
              x={mark.x - 3} y={mark.y - 3} width="6" height="6" />
      {/each}

      {#if drawn.point}
        <!-- Two circles rather than one: the ring survives being drawn over a
             band of its own colour, which a filled dot does not. -->
        <circle class="here ring-mark" cx={drawn.point.x} cy={drawn.point.y} r="6" />
        <circle class="here dot" cx={drawn.point.x} cy={drawn.point.y} r="2.5" />
      {/if}
    </svg>
  </div>

  <div class="viewrow">
    <div class="turns" role="group" aria-label={t.lumSurfaceControls}>
      <button class="turn" type="button" aria-label={t.lumSurfaceLeft}
              onclick={() => turn('ArrowLeft')}>◀</button>
      <button class="turn" type="button" aria-label={t.lumSurfaceRight}
              onclick={() => turn('ArrowRight')}>▶</button>
      <button type="button" disabled={isDefault} onclick={reset}>{t.lumSurfaceReset}</button>
    </div>
    <!-- Not a live region: it changes on every pixel of a drag, and a screen
         reader reading three hundred of those is worse than one that says
         nothing. It is here to be read after the turn, and to make "put it
         back" a thing with a number attached. -->
    <span class="readout">{t.lumSurfaceView(azimuth, tilt)}</span>
  </div>

  <p class="hint small" id="lum-surface-help">{t.lumSurfaceRotate}</p>

  <ul class="legend">
    <li><span class="chip here-chip"></span>{t.lumSurfaceHere}</li>
    {#if drawn.points.length > 0}
      <li><span class="chip taught-chip"></span>{t.lumSurfaceTaught}</li>
    {/if}
    <li><span class="chip limited-chip"></span>{t.lumSurfaceLimited}</li>
    <li><span class="chip bound-chip"></span>{t.lumSurfaceBound}</li>
  </ul>
  <p class="hint small">{t.lumSurfaceRadius}</p>
{/if}

<style>
  /* `.stage`, `.turns`, `.turn`, `.viewrow` and `.readout` are named here and
     nowhere in app.css - a component class that a global rule also matches
     inherits whatever that rule sets and does it silently, which is how the
     storage tab once got a 24 px row height it never asked for. */
  .stage {
    display: block;
    margin-top: 0.4rem;
    border-radius: 6px;
    background: var(--surface);
    /* The browser must not claim the gesture: without this the first
       vertical movement of a turn scrolls the page instead. */
    touch-action: none;
    cursor: grab;
  }
  .stage:active { cursor: grabbing; }
  .stage:focus-visible { outline: 2px solid currentColor; outline-offset: 2px; }

  svg {
    width: 100%;
    height: auto;
    display: block;
    /* The face of this diagram is the page, not a printed clock face - unlike
       the letter preview in the colour tab, which stays pale in either theme
       because a physical panel does. */
    background: var(--surface);
    border-radius: 6px;
    /* Or a drag that starts on a tick label selects the label. */
    user-select: none;
    -webkit-user-select: none;
  }

  .cell {
    stroke: rgb(0 0 0 / 0.3);
    stroke-width: 0.4;
    /* Translucent, so the far side of the cylinder still shows through the
       near one - which on a closed surface is the only way the reader can
       tell it is closed. */
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

  .ring { fill: none; stroke: var(--text); stroke-opacity: 0.22; stroke-width: 0.7; }
  .spoke { fill: none; stroke: var(--text); stroke-opacity: 0.14; stroke-width: 0.6; }

  .tick { font-size: 7px; fill: var(--text); opacity: 0.65; }
  .swatch { stroke: var(--text); stroke-opacity: 0.3; stroke-width: 0.4; }
  .huetick.far { opacity: 0.4; }

  .here.ring-mark { fill: none; stroke: var(--text); stroke-width: 1.6; }
  .here.dot { fill: var(--text); }

  /* Table 2. Stroked rather than filled, so a marker sitting on a dark band
     still reads as a shape and not as a blob the same colour as the surface
     under it - the same reasoning as the ring around the "here" dot. */
  .taught { fill: var(--surface); stroke: var(--text); stroke-width: 1.3; }
  .taught.far { opacity: 0.4; }
  /* "At least this much": the same dashed language the limited cells use,
     because it is the same kind of statement - a bound, not an equality. */
  .taught.bound { stroke-dasharray: 1.5 1; }

  .viewrow {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    gap: 0.4rem 0.7rem;
    margin-top: 0.4rem;
  }
  .turns { display: flex; gap: 0.35rem; }
  .turn { min-width: 2.2rem; }
  .readout { font-size: 0.78rem; opacity: 0.7; font-variant-numeric: tabular-nums; }

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
  .taught-chip { background: var(--surface); }
  .limited-chip { opacity: 0.35; background: currentColor; border-style: dashed; }
  .bound-chip { background: none; border-width: 2px; }

  .empty { font-style: italic; }
  .small { font-size: 0.78rem; }
</style>
