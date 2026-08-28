/**
 * One gesture, one finger: which input owns a drag.
 *
 * A drag that remembers a position and not an identity works perfectly with
 * one finger and misbehaves silently with two. Every pointer on the glass
 * fires `pointerdown`, `pointermove` and `pointerup` at the same window
 * listeners, so a second finger landing overwrites the stored position and the
 * first finger's next move reads as the distance *between* the fingers - and a
 * second finger lifting switches the drag off underneath a finger that is
 * still down. This module is the identity half of that, kept away from the DOM
 * so it can be tested without one: see tests/web/gesture.test.js.
 *
 * Both input models are here because both are bound by the caller. A browser
 * with Pointer Events identifies an input by `pointerId`; the older phones
 * that never got them - which are exactly the phones a wall clock ends up
 * being configured from - identify a finger by `Touch.identifier` off
 * `touches` and `changedTouches`. A mouse has no identifier at all and is
 * given `MOUSE_ID`.
 *
 * **Zero is an identifier in both models.** Every check in here is against
 * `null`, never against falsiness: `if (!id)` drops exactly one gesture in
 * every browser that starts counting at zero, and does it only on the first
 * touch of a session, which is the hardest kind of report to act on.
 *
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.2
 * @created  28.8.2026
 * @updated  28.8.2026
 */

/**
 * The identity of the mouse, which has none of its own.
 *
 * A string rather than a number, because the two models share one slot: a
 * numeric sentinel could be mistaken for a touch identifier, and touch
 * identifier 0 is the one a first finger usually gets.
 */
export const MOUSE_ID = 'mouse';

/** Whether this is a touch event, i.e. one that carries lists of fingers. */
function isTouchEvent(event) {
  return !!event && (event.touches != null || event.changedTouches != null);
}

/**
 * The finger with this identifier, out of a list that is a `TouchList` on a
 * device and a plain array in the tests. Indexed rather than iterated: a
 * `TouchList` has a length and numeric keys and is not iterable everywhere the
 * fallback path has to run.
 */
function fingerIn(list, id) {
  if (!list) return null;
  for (let i = 0; i < list.length; i++) {
    const touch = list[i];
    if (touch && touch.identifier === id) return touch;
  }
  return null;
}

/** The first finger an event is about, which is the one that could start one. */
function firstFinger(event) {
  const changed = event.changedTouches;
  if (changed && changed.length) return changed[0];
  const touches = event.touches;
  if (touches && touches.length) return touches[0];
  return null;
}

/**
 * Which input an event came from, or `null` if it names none.
 *
 * `null` rather than `undefined`, so that a comparison against an untracked
 * gesture - which is also null - cannot come out true by accident.
 */
export function gestureId(event) {
  if (!event) return null;
  if (event.pointerId !== undefined && event.pointerId !== null) return event.pointerId;
  if (isTouchEvent(event)) {
    const touch = firstFinger(event);
    return touch ? touch.identifier : null;
  }
  return Number.isFinite(event.clientX) ? MOUSE_ID : null;
}

/** Where the tracked input is on this event, or `null` if it is not on it. */
function positionOf(event, id) {
  if (!event) return null;
  if (isTouchEvent(event)) {
    // `changedTouches` first: on a move it holds the fingers that actually
    // moved, and the tracked one may sit in `touches` at its old position -
    // which is a step of zero, not an absence.
    const touch = fingerIn(event.changedTouches, id) ?? fingerIn(event.touches, id);
    return touch ? { x: touch.clientX, y: touch.clientY } : null;
  }
  if (gestureId(event) !== id) return null;
  return Number.isFinite(event.clientX) && Number.isFinite(event.clientY)
    ? { x: event.clientX, y: event.clientY }
    : null;
}

/**
 * A tracker for one drag at a time, reusable for as long as the element lives.
 *
 * Every method answers `null` when the event is not the tracked gesture's, so
 * a caller can hand it everything the window delivers and act only on what
 * comes back.
 */
export function dragGesture() {
  let id = null;
  let x = 0;
  let y = 0;

  const release = () => {
    const was = id;
    id = null;
    return was === null ? null : { id: was };
  };

  return {
    /** Whether a drag is in progress. */
    get active() { return id !== null; },

    /** The input holding the drag, or `null`. */
    get id() { return id; },

    /**
     * Take the gesture, if it is free and this event can hold it.
     * Answers `{ id, x, y }` on a start and `null` on anything else - a second
     * input landing, a button that is not the left one, an event with no
     * position in it.
     */
    start(event) {
      // The left button only. A right-click is the context menu and a
      // middle-click is a paste on some desktops; neither is a drag.
      if (event && event.button !== undefined && event.button !== 0) return null;
      if (id !== null) return null;
      const which = gestureId(event);
      if (which === null) return null;
      const at = positionOf(event, which);
      if (!at) return null;
      id = which;
      x = at.x;
      y = at.y;
      return { id, x, y };
    },

    /**
     * The step this event makes, measured against where the tracked input was
     * last seen - `{ id, dx, dy }`, or `null` if this is somebody else's move.
     */
    move(event) {
      if (id === null) return null;
      const at = positionOf(event, id);
      if (!at) return null;
      const dx = at.x - x;
      const dy = at.y - y;
      x = at.x;
      y = at.y;
      return { id, dx, dy };
    },

    /**
     * End the drag, if this event is the tracked input letting go. Answers
     * `{ id }` so the caller can release the capture it took against that id.
     *
     * On a touch event only `changedTouches` counts: that is the list of
     * fingers this event is about, and on the event that really does end the
     * gesture the tracked finger has already left `touches`.
     */
    end(event) {
      if (id === null) return null;
      if (isTouchEvent(event)) {
        if (!fingerIn(event.changedTouches, id)) return null;
        return release();
      }
      if (gestureId(event) !== id) return null;
      return release();
    },

    /**
     * Let go of whatever is held, and say what it was. What a component's
     * `destroy` uses: the element is going away with a finger still on it.
     */
    stop() {
      return release();
    }
  };
}
