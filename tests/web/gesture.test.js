/**
 * One gesture, one finger: the identity half of dragging the cylinder.
 *
 * The surface below the brightness curve turns under a finger, and the thing
 * that goes wrong there is not the arithmetic - `dragView` is tested next door
 * and a degree is a degree. It is *whose* finger. A second touch on a phone,
 * or a stylus set down beside a thumb, fires the same `pointerdown`,
 * `pointermove` and `pointerup` as the first one, on the same window
 * listeners; a drag that keeps one position and no identity reads all of them
 * as the same hand. The two failures that produces are both silent:
 *
 *   - the second finger lands somewhere else on the screen, the stored
 *     position jumps to it, and the next real move of the first finger turns
 *     the cylinder by the distance *between the fingers*;
 *   - the second finger lifts, the drag is switched off underneath the first
 *     one, and the surface stops following a finger that is still on it.
 *
 * Neither shows up in a screenshot and neither is reproducible with a mouse,
 * which is why the identity lives in a module with no DOM in it and is tested
 * here rather than looked at on a phone.
 *
 * Both input models are covered because both are bound: a browser with Pointer
 * Events uses `pointerId`, and the older phones that never got them - which are
 * exactly the phones a wall clock ends up being configured from - use
 * `Touch.identifier` off `touches` and `changedTouches`. The identifiers are
 * numbers and **zero is one of them** in both models, so every check here is
 * against `null`, never against falsiness.
 */
import test from 'node:test';
import assert from 'node:assert/strict';

import { dragGesture, gestureId, MOUSE_ID } from '../../web/src/lib/gesture.js';

/** A pointer event as the tracker reads one. */
const pointer = (id, x, y, extra = {}) =>
  ({ pointerId: id, clientX: x, clientY: y, button: 0, ...extra });

/** One finger in a touch list. */
const finger = (id, x, y) => ({ identifier: id, clientX: x, clientY: y });

/**
 * A touch event. `touches` is every finger on the glass, `changedTouches` is
 * the ones this event is about - which on a `touchend` is the ones that left,
 * and is the only list that says whose gesture ended.
 */
const touching = (touches, changed = touches) =>
  ({ touches, changedTouches: changed });

const mouse = (x, y, extra = {}) => ({ clientX: x, clientY: y, button: 0, ...extra });

// --- who an event belongs to ----------------------------------------------

test('an event says which input it came from, and zero is an input', () => {
  assert.equal(gestureId(pointer(3, 0, 0)), 3);
  // The falsiness trap, twice: a pointer id and a touch identifier may both be
  // 0, and a tracker written around `if (!id)` drops exactly one gesture in
  // every browser that starts counting there.
  assert.equal(gestureId(pointer(0, 0, 0)), 0);
  assert.equal(gestureId(touching([finger(0, 5, 5)])), 0);
  assert.equal(gestureId(touching([finger(7, 5, 5)])), 7);
  // A mouse has no identifier of its own, so it gets one - it is still a
  // single input, and it must not be mistaken for touch identifier 0.
  assert.equal(gestureId(mouse(4, 4)), MOUSE_ID);
  assert.notEqual(MOUSE_ID, 0);
  // And nothing identifiable is null rather than undefined, which would match
  // an untracked gesture.
  assert.equal(gestureId({}), null);
  assert.equal(gestureId(touching([], [])), null);
  assert.equal(gestureId(null), null);
});

// --- pointer events -------------------------------------------------------

test('a pointer drag is measured against its own last position', () => {
  const drag = dragGesture();
  assert.deepEqual(drag.start(pointer(1, 10, 10)), { id: 1, x: 10, y: 10 });
  assert.deepEqual(drag.move(pointer(1, 30, 25)), { id: 1, dx: 20, dy: 15 });
  assert.deepEqual(drag.move(pointer(1, 30, 20)), { id: 1, dx: 0, dy: -5 });
});

test('a second pointer does not take the gesture over', () => {
  // The jump. Without an identity the second `pointerdown` overwrites the
  // stored position, and the first finger's next move turns the cylinder by
  // the distance between the two fingers - here 190 px, most of a half turn.
  const drag = dragGesture();
  drag.start(pointer(1, 10, 10));
  assert.equal(drag.start(pointer(2, 200, 200)), null, 'the second pointer grabbed it');
  assert.equal(drag.move(pointer(2, 260, 240)), null, 'the second pointer moved it');
  assert.deepEqual(drag.move(pointer(1, 20, 10)), { id: 1, dx: 10, dy: 0 });
});

test('a second pointer lifting does not end the first', () => {
  const drag = dragGesture();
  drag.start(pointer(1, 10, 10));
  assert.equal(drag.end(pointer(2, 200, 200)), null, 'a stray release ended the drag');
  assert.equal(drag.end(pointer(2, 200, 200, { type: 'pointercancel' })), null);
  // Still turning, and still from where the tracked pointer actually was.
  assert.deepEqual(drag.move(pointer(1, 40, 10)), { id: 1, dx: 30, dy: 0 });
});

test('the pointer that started it is the one that ends it', () => {
  const drag = dragGesture();
  drag.start(pointer(1, 10, 10));
  assert.deepEqual(drag.end(pointer(1, 40, 10)), { id: 1 });
  // And afterwards nothing is being tracked: a move that arrives late - the
  // window listeners outlive any single gesture - must not turn anything.
  assert.equal(drag.move(pointer(1, 90, 10)), null);
  assert.equal(drag.end(pointer(1, 90, 10)), null);
});

test('pointer zero can hold the gesture like any other', () => {
  const drag = dragGesture();
  assert.deepEqual(drag.start(pointer(0, 10, 10)), { id: 0, x: 10, y: 10 });
  assert.equal(drag.start(pointer(1, 90, 90)), null);
  assert.deepEqual(drag.move(pointer(0, 15, 10)), { id: 0, dx: 5, dy: 0 });
  assert.deepEqual(drag.end(pointer(0, 15, 10)), { id: 0 });
});

// --- the touch fallback ---------------------------------------------------

test('a touch drag follows its own finger through a crowd', () => {
  // The fallback's own version of the same fault: reading `touches[0]` is
  // reading whichever finger the browser happens to list first, and the list
  // is not ordered by when they landed. Here the second finger is listed
  // first, so `touches[0]` would report a 190 px jump that nobody made.
  const drag = dragGesture();
  assert.deepEqual(drag.start(touching([finger(7, 10, 10)])), { id: 7, x: 10, y: 10 });
  const crowded = touching([finger(9, 200, 200), finger(7, 30, 25)]);
  assert.deepEqual(drag.move(crowded), { id: 7, dx: 20, dy: 15 });
});

test('a finger that is still down but did not move is not a move', () => {
  // On a `touchmove` the tracked finger may be in `touches` and not in
  // `changedTouches` - it is still on the glass, another finger moved. Its
  // position is where it was, so the step is zero rather than nothing.
  const drag = dragGesture();
  drag.start(touching([finger(7, 10, 10)]));
  const other = touching([finger(7, 10, 10), finger(9, 44, 44)], [finger(9, 44, 44)]);
  assert.deepEqual(drag.move(other), { id: 7, dx: 0, dy: 0 });
});

test('a second finger landing does not restart the gesture', () => {
  const drag = dragGesture();
  drag.start(touching([finger(7, 10, 10)]));
  const second = touching([finger(7, 10, 10), finger(9, 200, 200)], [finger(9, 200, 200)]);
  assert.equal(drag.start(second), null, 'the second finger grabbed it');
  assert.deepEqual(drag.move(touching([finger(7, 25, 10), finger(9, 200, 200)])),
                   { id: 7, dx: 15, dy: 0 });
});

test('a second finger lifting does not end the gesture', () => {
  // `changedTouches` is the list that says whose gesture ended, and it is the
  // only one: `touches` on this event still holds the tracked finger, and on
  // the event that really does end it, it does not.
  const drag = dragGesture();
  drag.start(touching([finger(7, 10, 10)]));
  const away = touching([finger(7, 10, 10)], [finger(9, 200, 200)]);
  assert.equal(drag.end(away), null, 'another finger leaving ended the drag');
  assert.deepEqual(drag.move(touching([finger(7, 40, 10)])), { id: 7, dx: 30, dy: 0 });
});

test('the tracked finger lifting ends it, however crowded the glass is', () => {
  const drag = dragGesture();
  drag.start(touching([finger(7, 10, 10)]));
  const lifted = touching([finger(9, 200, 200)], [finger(7, 40, 10)]);
  assert.deepEqual(drag.end(lifted), { id: 7 });
  assert.equal(drag.move(touching([finger(9, 260, 200)])), null);
});

test('a cancelled touch ends it too', () => {
  // A touch the browser takes away - a call arriving, a gesture the system
  // claimed - has to leave the drag off, or the surface turns on the next
  // unrelated move.
  const drag = dragGesture();
  drag.start(touching([finger(7, 10, 10)]));
  assert.deepEqual(drag.end(touching([], [finger(7, 10, 10)])), { id: 7 });
  assert.equal(drag.move(touching([finger(7, 90, 10)])), null);
});

test('finger zero can hold the gesture like any other', () => {
  const drag = dragGesture();
  assert.deepEqual(drag.start(touching([finger(0, 10, 10)])), { id: 0, x: 10, y: 10 });
  assert.equal(drag.end(touching([finger(0, 10, 10)], [finger(1, 90, 90)])), null);
  assert.deepEqual(drag.end(touching([], [finger(0, 10, 10)])), { id: 0 });
});

test('a mouse and a finger are not the same input', () => {
  // Both pairs are bound on the fallback path, so a touch event can arrive
  // mid-mouse-drag and the other way round. Neither may steer or stop the
  // other.
  const drag = dragGesture();
  drag.start(mouse(10, 10));
  assert.equal(drag.start(touching([finger(0, 200, 200)])), null);
  assert.equal(drag.move(touching([finger(0, 260, 240)])), null);
  assert.equal(drag.end(touching([], [finger(0, 260, 240)])), null);
  assert.deepEqual(drag.move(mouse(30, 10)), { id: MOUSE_ID, dx: 20, dy: 0 });
  assert.deepEqual(drag.end(mouse(30, 10)), { id: MOUSE_ID });
});

// --- what never starts a drag ---------------------------------------------

test('the left button only', () => {
  // A right-click is the context menu and a middle-click is a paste on some
  // desktops; neither is a turn of the cylinder.
  const drag = dragGesture();
  assert.equal(drag.start(pointer(1, 10, 10, { button: 2 })), null);
  assert.equal(drag.start(mouse(10, 10, { button: 1 })), null);
  // And nothing is left half-held behind the refusal.
  assert.equal(drag.move(pointer(1, 40, 10)), null);
  assert.deepEqual(drag.start(pointer(1, 10, 10)), { id: 1, x: 10, y: 10 });
});

test('an event with nowhere in it starts nothing', () => {
  const drag = dragGesture();
  for (const nothing of [{}, null, touching([], []), { pointerId: 1 }]) {
    assert.equal(drag.start(nothing), null, `${JSON.stringify(nothing)} started a drag`);
  }
});

test('a move or an end with no drag in progress is nothing', () => {
  const drag = dragGesture();
  assert.equal(drag.move(pointer(1, 10, 10)), null);
  assert.equal(drag.end(pointer(1, 10, 10)), null);
  assert.equal(drag.stop(), null);
});

test('stop lets go of whatever it was holding, and says what that was', () => {
  // What the component's `destroy` uses: the element is going away with a
  // finger still on it, and the pointer capture taken at the start has to be
  // released against the id that was actually captured.
  const drag = dragGesture();
  drag.start(pointer(4, 10, 10));
  assert.deepEqual(drag.stop(), { id: 4 });
  assert.equal(drag.stop(), null, 'stop let go of it twice');
  assert.equal(drag.move(pointer(4, 40, 10)), null);
  // And it is reusable afterwards: one tracker lives as long as the element.
  assert.deepEqual(drag.start(pointer(5, 1, 1)), { id: 5, x: 1, y: 1 });
});
