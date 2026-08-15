/**
 * i18n
 * Display language of the configuration UI. It is not chosen separately: the
 * UI follows the language set for the clock face, so picking "Français" in the
 * display tab switches this page to French as well. All German dialects and
 * Swiss German share the German texts.
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

// Index = the LANGUAGE_* value from src/Renderer.h, in that order:
// DE_DE, DE_SW, DE_BA, DE_SA, CH, EN, FR, IT, NL, ES.
const LOCALE_BY_LANGUAGE = ['de', 'de', 'de', 'de', 'de', 'en', 'fr', 'it', 'nl', 'es'];

const ui = $state({ locale: 'de' });

/** Switches the UI to the language belonging to a clock language number. */
export function setLanguage(language) {
  const next = LOCALE_BY_LANGUAGE[language] ?? 'de';
  if (next === ui.locale) return;
  ui.locale = next;
  // Keeps hyphenation and spell checking in step with what is on screen.
  document.documentElement.lang = next;
}

/** Texts of the current language; German for anything unexpected. */
export function dict() {
  return DICTS[ui.locale] ?? de;
}
