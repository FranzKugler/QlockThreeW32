/**
 * i18n
 * Display language of the configuration UI. It is not chosen separately: the
 * UI follows the language set for the clock face, so picking "Français" in the
 * display tab switches this page to French as well. All German dialects and
 * Swiss German share the German texts.
 *
 * Which languages exist, what they are called and which locale each one wants
 * are the clock's to say - see GET /languages. This file keeps a frozen copy
 * for clocks too old to answer, and nothing else here needs touching when a
 * language is added.
 *
 * Components read the texts through `dict()` inside a `$derived`, which makes
 * them re-render when the language changes:
 *
 *     const t = $derived(dict());
 *
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.0
 * @created  15.8.2026
 * @updated  15.8.2026
 */
import de from './locales/de.js';
import en from './locales/en.js';
import fr from './locales/fr.js';
import it from './locales/it.js';
import nl from './locales/nl.js';
import es from './locales/es.js';

const DICTS = { de, en, fr, it, nl, es };

/**
 * The languages this page knows without asking, and the fallback when a clock
 * cannot answer GET /languages.
 *
 * Deliberately frozen. A clock without that endpoint runs firmware from before
 * the list moved into the clock, so this *is* its list and will never need
 * another entry; anything newer answers and this is not read. Do not add a
 * language here - add it to src/languages/ and it arrives on its own.
 *
 * Names are endonyms, as the firmware sends them: someone looking for their
 * own language in a page they cannot read finds "Nederlands", not "Dutch".
 */
const BUILT_IN = [
  { value: 0, code: 'de-DE', name: 'Deutsch', uiLocale: 'de' },
  { value: 1, code: 'de-SW', name: 'Schwäbisch', uiLocale: 'de' },
  { value: 2, code: 'de-BA', name: 'Bayrisch', uiLocale: 'de' },
  { value: 3, code: 'de-SA', name: 'Sächsisch', uiLocale: 'de' },
  { value: 4, code: 'de-CH', name: 'Schwiizerdütsch', uiLocale: 'de' },
  { value: 5, code: 'en', name: 'English', uiLocale: 'en' },
  { value: 6, code: 'fr', name: 'Français', uiLocale: 'fr' },
  { value: 7, code: 'it', name: 'Italiano', uiLocale: 'it' },
  { value: 8, code: 'nl', name: 'Nederlands', uiLocale: 'nl' },
  { value: 9, code: 'es', name: 'Español', uiLocale: 'es' }
];

const ui = $state({ locale: 'de', languages: BUILT_IN });

/**
 * Takes the list the clock answered with. Anything that does not look like a
 * list of languages is ignored in favour of the built-in one - an empty picker
 * is worse than a slightly stale one.
 */
export function setLanguageList(list) {
  if (!Array.isArray(list) || list.length === 0) return;
  const clean = list.filter(
    (l) => Number.isInteger(l?.value) && typeof l.name === 'string' && l.name !== ''
  );
  if (clean.length === 0) return;
  ui.languages = clean;
}

/** The languages on offer, for the picker in the display tab. */
export function languages() {
  return ui.languages;
}

/**
 * Switches the UI to the language belonging to a clock language number.
 *
 * Reads the list, so an $effect that calls this re-runs when the clock's
 * answer arrives - the settings and the language list are two requests and
 * either can win.
 */
export function setLanguage(language) {
  const next = ui.languages.find((l) => l.value === language)?.uiLocale;
  const locale = next && next in DICTS ? next : 'de';
  if (locale === ui.locale) return;
  ui.locale = locale;
  // Keeps hyphenation and spell checking in step with what is on screen.
  document.documentElement.lang = locale;
}

/** Texts of the current language; German for anything unexpected. */
export function dict() {
  return DICTS[ui.locale] ?? de;
}

/**
 * Picks a UI language from the browser instead of from a clock - for the
 * setup portal, which is served before any clock exists to follow. Called
 * once, from portal.js; main.js never calls this, so the SPA's own default
 * (German, until the `$effect` on clock.language overrides it) is untouched.
 */
export function preferBrowserLanguage() {
  const wanted = (navigator.languages ?? [navigator.language ?? 'de']).map((tag) =>
    String(tag).toLowerCase().split('-')[0]
  );
  const locale = wanted.find((code) => code in DICTS) ?? 'de';
  if (locale !== ui.locale) {
    ui.locale = locale;
    document.documentElement.lang = locale;
  }
}
