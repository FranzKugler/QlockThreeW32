/**
 * vite.config
 * Build and dev-server configuration of the configuration SPA. Builds
 * web/ into data/, the LittleFS image flashed to the clock.
 *
 * @autor    Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.0
 * @created  15.8.2026
 * @updated  15.8.2026
 */
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
  '/wifi'
];

export default defineConfig({
  plugins: [svelte()],
  root: 'web',
  // Built output is the LittleFS image uploaded via `pio run -t uploadfs`.
  build: {
    outDir: '../data',
    emptyOutDir: true
  },
  server: {
    proxy: Object.fromEntries(
      API_ROUTES.map((route) => [route, 'http://localhost:8080'])
    )
  }
});
