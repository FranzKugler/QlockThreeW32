/**
 * appname
 * The name of the clock this page belongs to, in the three places it shows.
 *
 * With more than one clock on a network the name is the only thing telling
 * them apart, so it drives the heading, the browser tab and the label iOS puts
 * under the home screen icon rather than each of them carrying its own copy.
 * The value comes from `hostname` in /currentState and is updated in place
 * when the WLAN tab renames the clock.
 *
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.1
 * @created  16.8.2026
 * @updated  16.8.2026
 */

let current = $state('QlockThreeW32');

/** Read it in markup: `{appName()}`. */
export const appName = () => current;

export function setAppName(next) {
  if (!next || next === current) return;
  current = next;

  document.title = next;

  // Safari reads this from the live DOM at the moment someone taps "Add to
  // Home Screen", not from the served HTML, so setting it here is enough for
  // the icon to be labelled with the clock's own name.
  let meta = document.querySelector('meta[name="apple-mobile-web-app-title"]');
  if (!meta) {
    meta = document.createElement('meta');
    meta.name = 'apple-mobile-web-app-title';
    document.head.appendChild(meta);
  }
  meta.content = next;
}
