/**
 * posixtz
 * Turns a POSIX TZ string from zones.json into the fields the clock stores.
 *
 * "CET-1CEST,M3.5.0,M10.5.0/3" reads as: standard time is CET at one hour east
 * of UTC, summer time is CEST, it begins on the last Sunday of March and ends
 * on the last Sunday of October at 03:00. That is exactly the firmware's pair
 * of TimeChangeRules, so the conversion is mechanical - with two catches worth
 * knowing:
 *
 * - POSIX offsets count westwards. "CET-1" is UTC+1, so every offset is
 *   negated on the way in.
 * - The first rule after the offsets starts daylight saving and the second
 *   ends it, so they map onto tzDs* and tz* respectively, not in reading order.
 *
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.1
 * @created  16.8.2026
 * @updated  16.8.2026
 */

// A zone abbreviation is either three or more letters, or anything at all in
// angle brackets - which is how numeric ones like <+0330> are written.
const NAME = '(?:<[^>]+>|[A-Za-z]{3,})';
const OFFSET = '[+-]?\\d{1,3}(?::\\d{1,2}){0,2}';

const TZ = new RegExp(
  `^(${NAME})(${OFFSET})` + // standard abbreviation and offset
    `(?:(${NAME})(${OFFSET})?` + // summer abbreviation, offset optional
    `(?:,([^,]+),([^,]+))?)?$` // and when it starts and ends
);

// Mm.w.d[/h[:mm[:ss]]] - month, week, weekday, and the local time of the
// changeover. POSIX also allows Jn and n, which count days from the start of
// the year; no zone in the shipped list uses them, and the clock has nowhere
// to put them, so they are reported as unparseable rather than approximated.
const RULE = /^M(\d{1,2})\.(\d)\.(\d)(?:\/(-?\d{1,3})(?::(\d{1,2}))?(?::\d{1,2})?)?$/;

/** Strips the angle brackets from a numeric abbreviation such as <+0330>. */
const plainName = (name) => (name.startsWith('<') ? name.slice(1, -1) : name);

/** POSIX offset to minutes east of UTC, which is what the clock stores. */
function offsetMinutes(text) {
  const [hours, minutes = '0', seconds = '0'] = text.replace(/^\+/, '').split(':');
  const sign = hours.startsWith('-') ? -1 : 1;
  const total =
    Math.abs(parseInt(hours, 10)) * 60 + parseInt(minutes, 10) + Math.round(parseInt(seconds, 10) / 60);
  return -sign * total;
}

/**
 * One changeover rule as week, weekday, month and hour.
 *
 * The hour is clamped to the 0..23 the firmware field holds. Ten zones need
 * that - Cairo and Santiago switch at "24", Jerusalem at "26", Gaza at "50",
 * Nuuk at "-1", all of which mean midnight rolled into a neighbouring day.
 * Clamping moves the changeover by a few hours once a year in those places,
 * which is the price of the two-rule model and not worth widening it for.
 */
function parseRule(text) {
  const match = RULE.exec(text);
  if (!match) return null;

  const [, month, week, day, hour] = match;
  return {
    // POSIX counts weeks 1..5 with 5 meaning "the last one"; the firmware
    // follows the Timezone library, where Last is 0 and First is 1.
    week: Number(week) === 5 ? 0 : Number(week),
    // POSIX has Sunday as 0, the library has it as 1.
    dow: Number(day) + 1,
    month: Number(month),
    hour: Math.min(23, Math.max(0, hour === undefined ? 2 : Number(hour)))
  };
}

/**
 * Parses a POSIX TZ string, or returns null if it is not one.
 * Shape: { useDs, std: {name, offset, ...schedule}, dst: {...} }.
 */
export function parsePosixTz(tz) {
  const match = TZ.exec(String(tz).trim());
  if (!match) return null;

  const [, stdName, stdOffset, dstName, dstOffset, startsDst, endsDst] = match;
  const std = { name: plainName(stdName), offset: offsetMinutes(stdOffset) };

  // No summer abbreviation at all, or one without a schedule: the zone keeps
  // one offset all year. India and Iran are the obvious examples.
  if (!dstName || !startsDst || !endsDst) return { useDs: false, std, dst: null };

  const begin = parseRule(startsDst);
  const end = parseRule(endsDst);
  if (!begin || !end) return null;

  return {
    useDs: true,
    // The rule that ends daylight saving is the one that starts standard time.
    std: { ...std, ...end },
    dst: {
      name: plainName(dstName),
      // Left out, the summer offset is an hour ahead of standard time.
      offset: dstOffset === undefined ? std.offset + 60 : offsetMinutes(dstOffset),
      ...begin
    }
  };
}

/**
 * The same thing as a patch for the settings object, ready to be merged and
 * posted to /timezone.
 *
 * A zone without daylight saving only contributes the standard side. Its
 * changeover fields are left as they were rather than overwritten with a
 * schedule that does not exist - the UI disables them anyway, and keeping them
 * means switching to such a zone and back does not quietly discard anything.
 */
export function timezoneFields(zone, tz) {
  const parsed = parsePosixTz(tz);
  if (!parsed) return null;

  const fields = {
    tzZone: zone,
    useDs: parsed.useDs,
    tzName: parsed.std.name,
    tzOffset: parsed.std.offset
  };

  if (!parsed.useDs) return fields;

  return {
    ...fields,
    tzWeek: parsed.std.week,
    tzDoW: parsed.std.dow,
    tzMonth: parsed.std.month,
    tzHour: parsed.std.hour,
    tzDsName: parsed.dst.name,
    tzDsWeek: parsed.dst.week,
    tzDsDoW: parsed.dst.dow,
    tzDsMonth: parsed.dst.month,
    tzDsHour: parsed.dst.hour,
    tzDsOffset: parsed.dst.offset
  };
}

/** "Europe/Berlin" -> "Europe", "UTC" -> "UTC". */
export const zoneArea = (zone) => (zone.includes('/') ? zone.slice(0, zone.indexOf('/')) : zone);

/** "America/Argentina/Buenos_Aires" -> "Argentina/Buenos Aires". */
export const zonePlace = (zone) =>
  (zone.includes('/') ? zone.slice(zone.indexOf('/') + 1) : zone).replace(/_/g, ' ');
