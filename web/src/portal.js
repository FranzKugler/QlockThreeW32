/**
 * portal
 * Entry point of the setup portal: mounts it into portal.html.
 *
 * A second entry rather than a route inside the SPA, because the two are
 * served in different modes of the clock and only one of them exists at a
 * time - see src/Portal.h. Vite builds both from web/, so they share
 * app.css and everything under lib/ without either of them importing the
 * other.
 *
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  1.0
 * @created  4.9.2026
 * @updated  4.9.2026
 */
import './app.css';
import { mount } from 'svelte';
import { preferBrowserLanguage } from './lib/i18n.svelte.js';
import Portal from './Portal.svelte';

preferBrowserLanguage();

export default mount(Portal, { target: document.getElementById('app') });
