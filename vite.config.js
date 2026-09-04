/**
 * vite.config
 * Build and dev-server configuration of the web UI. Builds web/ into data/,
 * the LittleFS image flashed to the clock - two pages out of one build, see
 * the `build` option below.
 *
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.0
 * @created  15.8.2026
 * @updated  15.8.2026
 */
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { defineConfig } from 'vite';
import { svelte } from '@sveltejs/vite-plugin-svelte';

// The clock's REST endpoints. In dev these are proxied to the mock server
// (`npm run mock`); on the device the SPA is served from the same origin.
const API_ROUTES = [
  '/currentState',
  '/display',
  '/color',
  '/autoluminance',
  '/configuration',
  '/timezone',
  '/hostname',
  '/restart',
  '/light',
  '/luminance',
  '/log',
  '/panel',
  '/languages',
  '/expert',
  '/fs',
  '/nvs',
  // Built by the clock, not shipped in the image: it carries the clock's name.
  '/manifest.webmanifest',
  '/wifi',
  '/ota',
  '/portal'
];

const pkg = JSON.parse(readFileSync(new URL('./package.json', import.meta.url)));

/*
 * The release workflow passes the version the firmware was stamped with, so
 * both halves of a build report the same thing. package.json is the fallback
 * for a plain local build.
 */
const VERSION = process.env.QLOCK_VERSION || pkg.version;

/**
 * Puts version.json into the filesystem image. The firmware reads it at boot
 * and reports it through /ota/status, so the update tab can show which build
 * of the web UI is actually in flash - firmware and UI are separate images and
 * are not necessarily updated together.
 */
function versionFile() {
  return {
    name: 'qlock-version',
    generateBundle() {
      this.emitFile({
        type: 'asset',
        fileName: 'version.json',
        source: JSON.stringify({ version: VERSION, built: new Date().toISOString() })
      });
    }
  };
}

export default defineConfig({
  plugins: [svelte(), versionFile()],
  root: 'web',
  // Built output is the LittleFS image uploaded via `pio run -t uploadfs`.
  // Two pages come out of one build: index.html is the configuration SPA,
  // portal.html is the setup portal served from the clock's own access
  // point (see src/Portal.h) - they share app.css and everything under
  // web/src/lib/, which is what makes the portal look like the app rather
  // than merely similar to it.
  build: {
    outDir: '../data',
    emptyOutDir: true,
    rollupOptions: {
      input: {
        main: resolve(import.meta.dirname, 'web/index.html'),
        portal: resolve(import.meta.dirname, 'web/portal.html')
      }
    }
  },
  server: {
    proxy: Object.fromEntries(
      API_ROUTES.map((route) => [route, 'http://localhost:8080'])
    )
  }
});
