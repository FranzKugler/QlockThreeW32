/**
 * Two invariants in the firmware that nothing else can check.
 *
 * Both are about *control flow around persistent storage*, which needs an NVS
 * partition to exercise and therefore needs a clock. A clock is exactly what
 * this project does not have in a test run, and the bug each of these guards
 * is one that reproduces only after a reboot - the slowest possible feedback
 * loop for the smallest possible edit.
 *
 * Reading the source is a poor substitute for running it and is used here
 * deliberately and narrowly: each test names one shape the code must have, and
 * the shape is the fix. Neither is a stand-in for behaviour that could be
 * tested properly; where that was possible the logic was moved into
 * `src/ResidualStore.cpp`, which `tests/host/` compiles and runs.
 */
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';

const SOURCE = fs.readFileSync(
  new URL('../../src/Luminance.cpp', import.meta.url), 'utf8');

const FACTORY = fs.readFileSync(
  new URL('../../src/FactoryLuminance.cpp', import.meta.url), 'utf8');

const SHIPPED = JSON.parse(fs.readFileSync(
  new URL('../../web/public/factory-luminance.json', import.meta.url), 'utf8'));

/**
 * The same source with every literal and comment blanked out, at the same
 * length, so an index into one is an index into the other.
 *
 * Both halves are load-bearing and both were found the hard way.
 * `FactoryLuminance::begin()` contains `raw.endsWith("}")`, so a brace counter
 * that cannot tell a literal from code stops three lines into a two-hundred
 * line body. And a comment in ordinary English contains an apostrophe, which
 * to a counter that cannot tell a comment from code opens a character literal
 * that runs until the next one - swallowing whatever braces lie between.
 *
 * Neither failure is loud. Both hand back a plausible looking excerpt and a
 * confident assertion about it, which is a worse fault than the ones this file
 * exists to catch, so they are removed rather than written around.
 */
function masked(source, alsoLiterals = true) {
  const out = source.split('');
  const blank = (from, to) => {
    for (let i = from; i < to && i < out.length; i++) {
      if (out[i] !== '\n') out[i] = ' ';
    }
  };
  for (let i = 0; i < source.length; i++) {
    const two = source.slice(i, i + 2);
    if (two === '//') {
      const end = source.indexOf('\n', i);
      const to = end === -1 ? source.length : end;
      blank(i, to);
      i = to;
    } else if (two === '/*') {
      const end = source.indexOf('*/', i + 2);
      const to = end === -1 ? source.length : end + 2;
      blank(i, to);
      i = to - 1;
    } else if (source[i] === '"' || source[i] === '\'') {
      const quote = source[i];
      let j = i + 1;
      while (j < source.length && source[j] !== quote) {
        j += source[j] === '\\' ? 2 : 1;
      }
      if (alsoLiterals) blank(i + 1, j);
      i = j;
    }
  }
  return out.join('');
}

/**
 * A body with its comments gone and its literals kept.
 *
 * For asserting that a call is *not* made: a comment saying which call was
 * wrong and why contains that call, spelled exactly, and matching against the
 * prose would fail the moment somebody wrote down what they had fixed. The
 * literals stay because the reads being checked are keyed by them - it is
 * `doc["profileId"]` that says which field is being read.
 */
const codeOf = (text) => masked(text, false);

/** The body of a top-level function, by its opening line. */
function bodyIn(source, signature, where) {
  const at = source.indexOf(signature);
  assert.notEqual(at, -1, `${signature} is not in ${where}`);
  const scan = masked(source);
  const open = source.indexOf('{', at);
  let depth = 0;
  for (let i = open; i < source.length; i++) {
    if (scan[i] === '{') depth++;
    if (scan[i] === '}' && --depth === 0) return source.slice(open + 1, i);
  }
  throw new Error(`${signature} has no end`);
}

/**
 * The body of a brace-delimited block within an already-extracted function
 * body, found by matching braces rather than by a fixed character window -
 * a comment explaining the fix can be arbitrarily long without pushing the
 * code being checked out of range.
 */
function blockIn(text, opening) {
  const at = text.indexOf(opening);
  assert.notEqual(at, -1, `${opening} is not in the given text`);
  const scan = masked(text);
  const open = text.indexOf('{', at);
  let depth = 0;
  for (let i = open; i < text.length; i++) {
    if (scan[i] === '{') depth++;
    if (scan[i] === '}' && --depth === 0) return text.slice(open + 1, i);
  }
  throw new Error(`${opening} has no end`);
}

const body = (signature) => bodyIn(SOURCE, signature, 'Luminance.cpp');

test('begin() reads both records, whatever the other one does', () => {
  // The bug this guards: begin() used to *be* loadCurve(), with three early
  // returns in it - no namespace, no stored key, unreadable JSON - and the
  // call that reads the colour corrections sat after them. A clock that has
  // only ever known the factory model has no `curve` key at all, so the second
  // return fired every boot, the corrections were never read back, and
  // everything the owner had taught the clock vanished overnight while the
  // read-out showed an empty list as though nothing had been said.
  //
  // The two stores are independent. A clock can have corrections and no white
  // curve, or a curve and no corrections, and any early exit here is a way for
  // one of them to be skipped because the *other* is missing.
  const begin = body('void Luminance::begin()');
  assert.match(begin, /\bloadCurve\(\)/, 'the white curve is read');
  assert.match(begin, /\bloadUser\(\)/, 'and so are the corrections');
  assert.doesNotMatch(begin, /(^|[^\w])return\b/,
                      'begin() must have no early exit between the two');
});

test('a nudge at the top of the range is kept as a lower bound', () => {
  // The other half of the same kind of bug: invisible until a bright room, and
  // then wrong in the direction that matters. Somebody who drags the slider to
  // the ceiling has said "at least this much" - the slider had nothing above
  // it to offer - and storing that as an equality biases the model downward.
  //
  // The floor is deliberately *not* censored: it is a number the owner chose
  // as the dimmest they ever want, so a nudge sitting on it is a preference
  // being met rather than a wish being cut off. So this checks the comparison
  // is against the high end and nothing else.
  const poll = body('bool Luminance::poll(float lux)');
  assert.match(poll, /wanted\s*>=\s*rangeHigh/,
               'the ceiling is what makes a correction a bound');
  assert.doesNotMatch(poll, /wanted\s*<=\s*rangeLow/,
                      'the floor is a preference met, not a wish cut off');
  // The argument list contains calls of its own, so it cannot be matched by
  // "everything up to the first bracket".
  assert.match(poll, /ResidualStore::add\([\s\S]{0,240}?censored\s*\)/,
               'and the flag reaches the store');
});

/*
 * The runtime profile's contract, on the half no compiler here can run.
 *
 * `FactoryProfile::valid()` is pure and `tests/host/` compiles it, so every
 * rule that can live in the struct is checked there by running it. What cannot
 * is the *parser*: `FactoryLuminance::begin()` needs ArduinoJson, LittleFS and
 * NVS, none of which exist in this test run - and it is the parser that
 * decides whether a JSON `true` and a JSON `1` are the same thing.
 *
 * They are not, and the direction matters. `scripts/factory_luminance.py`
 * writes the file and `load_runtime()` refuses anything but a real boolean;
 * the generated file in `web/public/` is what both this and the clock read. A
 * parser here that insisted on 1 and 0 would refuse the very file the project
 * ships - not at build time, not in any test that runs, but on a clock, once,
 * as `factoryShape`. So the shape of that one read is checked, against the
 * shipped bytes rather than against a belief about them.
 */
test('the shipped profile really does write its flags as booleans', () => {
  // The premise of the test below. If this ever stops holding, the parser
  // requirement changes with it and the failure should land here first.
  const levels = SHIPPED.payload.levels;
  assert.ok(levels.length >= 2, 'the shipped profile has a grid');
  for (const level of levels) {
    assert.equal(typeof level.censored, 'boolean', 'censored is a boolean');
    for (const flag of level.bounds) {
      assert.equal(typeof flag, 'boolean', 'a bound is a boolean');
    }
  }
  assert.ok(levels.some((level) => level.bounds.some(Boolean)),
            'and at least one corner really is bounded, so the branch is live');
});

test('and the firmware reads a bound as a boolean, not as a number', () => {
  // `whole(one, 0, 1, value)` was the first version and is exactly wrong: the
  // helper refuses `is<bool>()` on purpose, so it accepts the 1 and 0 nothing
  // writes and refuses the `true` and `false` the generator does write.
  const begin = bodyIn(FACTORY, 'bool FactoryLuminance::begin()',
                       'FactoryLuminance.cpp');
  // Comments gone, and the window opened at the loop rather than at the array
  // it walks: the fix carries a note naming the call it replaced, so matching
  // against prose would fail the moment somebody wrote down what they fixed,
  // and a note long enough to explain it pushes the code out of a short slice.
  const loop = blockIn(begin, 'for (JsonVariantConst one : bounds)');
  const reading = codeOf(loop);
  assert.match(reading, /flag\(/,
               'each bound goes through flag(), which is the boolean reader');
  assert.doesNotMatch(reading, /whole\(\s*one\s*,\s*0\s*,\s*1\s*,/,
                      'and not through whole(), which refuses a real boolean');
});

test('a level still has to say whether it is censored', () => {
  const begin = bodyIn(FACTORY, 'bool FactoryLuminance::begin()',
                       'FactoryLuminance.cpp');
  assert.match(begin, /flag\(level\["censored"\]/,
               'the censored flag is read as a boolean too');
});

test('the identity is required to be a string with something in it', () => {
  // Both key something. `profileId` is what the read-out names when somebody
  // asks which measurement their clock is running; `sourceChecksum` is what
  // the stored colour corrections are filed under, so an empty one makes every
  // correction look as though it belonged to whatever profile is loaded now.
  // `text()` is the helper that requires a non-empty string; `load_runtime`
  // refuses the same two on the same grounds.
  const begin = bodyIn(FACTORY, 'bool FactoryLuminance::begin()',
                       'FactoryLuminance.cpp');
  assert.match(begin, /text\(doc\["profileId"\]/, 'the id is read as text');
  assert.match(begin, /text\(doc\["sourceChecksum"\]/,
               'and so is the checksum it is filed under');
  const helper = bodyIn(FACTORY, 'bool text(JsonVariantConst value, String &out)',
                        'FactoryLuminance.cpp');
  assert.match(helper, /out\.length\(\)\s*>\s*0/,
               'and text() is what refuses an empty one');
});

test('the saturation fade is read as two percentages', () => {
  const begin = bodyIn(FACTORY, 'bool FactoryLuminance::begin()',
                       'FactoryLuminance.cpp');
  for (const edge of ['zeroAtSat', 'fullAtSat']) {
    assert.match(begin, new RegExp(`whole\\(doc\\["satFade"\\]\\["${edge}"\\], 0, 100`),
                 `${edge} is a whole percentage`);
  }
  // That they are in the right order is FactoryProfile::valid()'s to say, and
  // tests/host/ runs it. Checking it twice here would be checking the source
  // instead of the behaviour, which is the trade this file exists to avoid.
  assert.match(begin, /FactoryProfile::valid\(built\)/,
               'and valid() is what compares them');
});
