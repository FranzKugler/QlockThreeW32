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
  '/timezone'
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
