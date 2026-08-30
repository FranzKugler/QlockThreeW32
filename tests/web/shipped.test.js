/**
 * The runtime profile reaches the filesystem image, and keeps reaching it.
 *
 * `build.emptyOutDir` clears `data/` on every build, so anything the firmware
 * needs there has to be *produced* by the build rather than left lying in it.
 * The mechanism is Vite's public directory - `web/public` is copied into the
 * output after the clearing - which is the same route `zones.json` and the
 * home screen icons already take: generated, committed, and never fetched or
 * built on demand.
 *
 * That is one line of configuration away from silently stopping. `publicDir:
 * false`, or an `outDir` that no longer matches what `pio run -t buildfs`
 * packs, and the clock boots with no colour model and falls back to the white
 * curve - which it does *quietly*, because falling back quietly is exactly
 * what it is supposed to do. So the arrangement is asserted here rather than
 * discovered on a clock.
 */
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { execFileSync } from 'node:child_process';

const ROOT = new URL('../../', import.meta.url);
const SHIPPED = new URL('web/public/factory-luminance.json', ROOT);
const CONFIG = fs.readFileSync(new URL('vite.config.js', ROOT), 'utf8');

test('the profile is committed where the build will pick it up', () => {
  assert.ok(fs.existsSync(SHIPPED), 'web/public/factory-luminance.json is missing');
});

test('and it is small enough to be worth shipping at all', () => {
  const bytes = fs.statSync(SHIPPED).size;
  // The reviewed profile it is derived from is 43 KB of provenance. If this
  // ever approaches that, the derivation has stopped deriving.
  assert.ok(bytes < 6144, `${bytes} bytes`);
  assert.ok(bytes > 512, `${bytes} bytes is not a grid`);
});

test('the build still copies the public directory into data/', () => {
  // Not a general assertion about Vite: these two lines are the whole route
  // from a committed file to the clock's flash.
  assert.match(CONFIG, /outDir:\s*'\.\.\/data'/);
  assert.doesNotMatch(CONFIG, /publicDir\s*:\s*false/,
                      'publicDir off means the profile never reaches data/');
});

test('it is a sealed document, not a bag of numbers', () => {
  const text = fs.readFileSync(SHIPPED, 'utf8');
  // The firmware checks the same three things in the same order before it
  // parses anything - see FactoryLuminance.h. The layout is what lets it hash
  // a substring rather than re-canonicalise a parsed document, which it cannot
  // do: ArduinoJson does not sort keys.
  assert.ok(text.startsWith('{"checksum":{"algorithm":"sha256","value":"'));
  assert.ok(text.endsWith('}'));
  assert.ok(!text.includes('\n'), 'canonical means no incidental whitespace');
  const document = JSON.parse(text);
  assert.equal(document.checksum.value.length, 64);
  assert.equal(document.payload.modelId,
               'white-cone-plus-first-harmonic-hue-nose-with-blue-line');
});

test('it is what the generator makes, byte for byte', () => {
  // Committed output, so a diff only ever shows a value that moved - and a
  // regeneration with nothing changed leaves the file untouched. The same rule
  // as zones.json and panels.scad.
  const scratch = new URL('web/public/.factory-luminance.check', ROOT);
  try {
    execFileSync('/usr/bin/python3',
                 ['scripts/build_cone_profile.py', new URL(scratch).pathname],
                 { cwd: new URL('.', ROOT), encoding: 'utf8' });
    assert.equal(fs.readFileSync(scratch, 'utf8'), fs.readFileSync(SHIPPED, 'utf8'));
  } finally {
    fs.rmSync(scratch, { force: true });
  }
});
