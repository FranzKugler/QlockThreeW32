<script>
  /**
   * One labelled slider with a - / + button on either side.
   *
   * `stepSize` applies to the buttons only; dragging the slider stays at 1, so
   * the buttons give the fine, repeatable steps.
   */
  let {
    id,
    label,
    unit = '',
    min = 0,
    max = 100,
    stepSize = 1,
    value = $bindable(),
    /** CSS background for the slider track. */
    track,
    /** Hue is circular, so its buttons wrap instead of stopping at the ends. */
    wrap = false,
    onchange
  } = $props();

  function stepBy(delta) {
    const next = value + delta;
    if (wrap) {
      const span = max - min + 1;
      value = (((next - min) % span) + span) % span + min;
    } else {
      value = Math.min(max, Math.max(min, next));
    }
    onchange();
  }
</script>

<div class="ctl">
  <div class="ctl-head">
    <label for={id}>{label}</label>
    <output for={id}>{value}{unit}</output>
  </div>
  <div class="ctl-row">
    <button
      type="button"
      class="step"
      onclick={() => stepBy(-stepSize)}
      disabled={!wrap && value <= min}
      aria-label="{label} um {stepSize}{unit} verringern"
    >
      −
    </button>
    <input {id} type="range" {min} {max} bind:value oninput={onchange} style="--track: {track}" />
    <button
      type="button"
      class="step"
      onclick={() => stepBy(stepSize)}
      disabled={!wrap && value >= max}
      aria-label="{label} um {stepSize}{unit} erhöhen"
    >
      +
    </button>
  </div>
</div>
