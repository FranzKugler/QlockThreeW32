/**
 * throttle
 * Leading-and-trailing throttle. Dragging the colour wheel emits a change event
 * per pointer move; without this every one of them would become a POST to the
 * ESP32's single-threaded web server (and push back its deferred flash write).
 *
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.0
 * @created  15.8.2026
 * @updated  15.8.2026
 */
export function throttle(fn, ms) {
  let timer = null;
  let pending = null;

  const fire = () => {
    if (pending === null) {
      timer = null;
      return;
    }
    const args = pending;
    pending = null;
    fn(...args);
    timer = setTimeout(fire, ms);
  };

  return (...args) => {
    pending = args;
    if (timer === null) fire();
  };
}
