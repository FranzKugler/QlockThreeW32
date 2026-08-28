/**
 * The six locale files, against each other.
 *
 * `de.js` is the reference: same keys, same order, same shapes in every
 * locale. Nothing falls back per key, so a missing one renders as `undefined`
 * on the page - and the build says nothing, because an object literal with a
 * key missing is a perfectly good object literal.
 *
 * **Duplicates matter as much as counts**, and that is not a hypothetical: an
 * `err_*` code added twice silently shadowed the original in all six files at
 * once, and a count comparison across locales saw six consistent numbers and
 * reported nothing, because the mistake had been made six times identically.
 * In an object literal the last entry simply wins, with no warning from Vite
 * or Svelte. So this reads the source rather than the imported object, which
 * is the only place a duplicate is still visible.
 */
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';

const LOCALES = ['de', 'en', 'fr', 'it', 'es', 'nl'];
const DIR = new URL('../../web/src/lib/locales/', import.meta.url);

const modules = Object.fromEntries(await Promise.all(
  LOCALES.map(async (code) => [code, (await import(new URL(`${code}.js`, DIR))).default])
));

/** Top-level keys in the order they are written, duplicates included. */
function writtenKeys(code) {
  const source = fs.readFileSync(new URL(`${code}.js`, DIR), 'utf8');
  return [...source.matchAll(/^ {2}(?:'([^']+)'|([A-Za-z_$][\w$]*)):/gm)]
    .map((match) => match[1] ?? match[2]);
}

test('every locale carries the same keys as the reference', () => {
  const reference = Object.keys(modules.de);
  for (const code of LOCALES.slice(1)) {
    assert.deepEqual(Object.keys(modules[code]), reference,
                     `${code}.js does not match de.js key for key, in order`);
  }
});

test('no locale says the same thing twice', () => {
  for (const code of LOCALES) {
    const keys = writtenKeys(code);
    const seen = new Set();
    const twice = keys.filter((key) => seen.size === seen.add(key).size);
    assert.deepEqual(twice, [], `${code}.js repeats ${twice.join(', ')}`);
  }
});

test('a key that takes arguments takes them everywhere', () => {
  for (const [key, value] of Object.entries(modules.de)) {
    for (const code of LOCALES.slice(1)) {
      const other = modules[code][key];
      assert.equal(typeof other, typeof value, `${code}.js ${key} is a different shape`);
      if (typeof value === 'function') {
        assert.equal(other.length, value.length,
                     `${code}.js ${key} takes a different number of arguments`);
      }
      if (Array.isArray(value)) {
        assert.equal(other.length, value.length,
                     `${code}.js ${key} is a different length`);
      }
    }
  }
});

test('the colour model speaks all six languages', () => {
  // The keys this work added, named rather than counted: a count would go
  // green again the moment somebody removed one and added another.
  const added = [
    'lumSurfaceTitle', 'lumSurfaceHint', 'lumSurfaceNone', 'lumSurfaceSummary',
    'lumSurfaceHere', 'lumSurfaceLimited', 'lumSurfaceBound',
    // The cylinder: what the three axes are, and how to turn it. The mapping
    // sentence is not decoration - a radius that is logarithmic and does not
    // say so is a chart that reads as linear and is wrong by two decades.
    'lumSurfaceRadius', 'lumSurfaceRotate', 'lumSurfaceControls',
    'lumSurfaceLeft', 'lumSurfaceRight', 'lumSurfaceReset', 'lumSurfaceView',
    'lumFactory', 'lumFactoryNone', 'lumFactoryStack', 'lumFactorySource',
    'lumFactoryTarget', 'lumFactoryAccuracy', 'lumFactoryAccuracyMet',
    'lumFactoryObservations', 'lumFactoryMismatch',
    'lumResiduals', 'lumResidualsHint', 'lumResidualsEmpty',
    'lumResidualDecades', 'lumFactoryRestore', 'lumFactoryRestoreHint'
  ];
  for (const code of LOCALES) {
    for (const key of added) {
      assert.ok(key in modules[code], `${code}.js is missing ${key}`);
    }
  }
});

test('the three sources the API can report all have a name', () => {
  // `target.source` is one of exactly these three, and an unnamed one would
  // render as `undefined` next to a number - which reads as a fault in the
  // clock rather than a gap in a translation.
  for (const code of LOCALES) {
    for (const source of ['legacy', 'factory', 'factory+user']) {
      assert.equal(typeof modules[code].lumFactorySource[source], 'string',
                   `${code}.js has no name for ${source}`);
    }
  }
});
