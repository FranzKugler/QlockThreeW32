/**
 * de
 * German texts of the configuration UI, and the reference every other locale
 * follows: same keys, same order, same grouping.
 *
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.0
 * @created  15.8.2026
 * @updated  15.8.2026
 */
export default {
  // --- shell ---
  tabs: ['Anzeige', 'Farbe', 'Zeitzone', 'WLAN', 'Update'],
  loading: 'Einstellungen werden geladen …',
  loadingShort: 'wird geladen …',
  clockUnreachable: 'Uhr nicht erreichbar',
  retry: 'Erneut versuchen',
  writeFailed: 'Übertragung fehlgeschlagen',

  // --- display tab ---
  displayTitle: 'Anzeige',
  modes: [
    'Uhrzeit',
    'Uhrzeit mit WiFi-Status',
    'Aus (dunkel)',
    'Sekunden',
    'Test',
    'Status'
  ],
  appearance: 'Darstellung',
  language: 'Sprache',
  languages: [
    'Deutsch',
    'Schwäbisch',
    'Bayrisch',
    'Sächsisch',
    'Schweizerisch',
    'Englisch',
    'Französisch',
    'Italienisch',
    'Niederländisch',
    'Spanisch'
  ],
  corners: 'Ecken',
  clockwise: 'im Uhrzeigersinn',
  counterClockwise: 'gegen den Uhrzeigersinn',
  minutes: 'Minuten',
  monochrome: 'monochrom',
  colored: 'farbig',

  // --- colour tab ---
  colorTitle: 'Farbe',
  hue: 'Farbton',
  saturation: 'Sättigung',
  brightness: 'Helligkeit',
  // The lit clock face in the preview, reading "it is five past two".
  preview: ['ES IST', 'FÜNF NACH', 'ZWEI'],
  decreaseBy: (label, step) => `${label} um ${step} verringern`,
  increaseBy: (label, step) => `${label} um ${step} erhöhen`,
  automatic: 'Automatik',
  autoBrightness: 'Helligkeit automatisch',
  ldrHint:
    'Ohne Wirkung, solange die LDR-Auswertung in der Firmware auskommentiert ' +
    'ist (siehe src/LDR.cpp).',

  // --- timezone tab ---
  timeServer: 'Zeitserver',
  ntpServer: 'NTP-Server',
  timezoneTitle: 'Zeitzone',
  dst: 'Sommerzeit',
  standardTime: 'Normalzeit',
  noDstHint:
    'Ohne Sommerzeit gilt durchgehend der Offset der Normalzeit; die ' +
    'Umschaltzeitpunkte werden nicht ausgewertet.',
  abbreviation: 'Kürzel',
  week: 'Woche',
  weeks: ['Letzter', '1.', '2.', '3.', '4.'],
  day: 'Tag',
  days: ['So.', 'Mo.', 'Di.', 'Mi.', 'Do.', 'Fr.', 'Sa.'],
  month: 'Monat',
  months: [
    'Jan.', 'Feb.', 'Mär.', 'Apr.', 'Mai', 'Jun.',
    'Jul.', 'Aug.', 'Sep.', 'Okt.', 'Nov.', 'Dez.'
  ],
  hour: 'Stunde',
  offsetMin: 'Offset (min)',

  // --- wifi tab ---
  connection: 'Verbindung',
  network: 'Netz',
  address: 'Adresse',
  signal: 'Signal',
  hostname: 'Hostname',
  mac: 'MAC',
  quality: ['schwach', 'mittel', 'gut', 'sehr gut'],
  statusUnavailable: 'Status nicht abrufbar',
  availableNetworks: 'Verfügbare Netze',
  scanning: 'Suche läuft …',
  noNetworks: 'Keine Netze gefunden.',
  rescan: 'Erneut suchen',
  encrypted: 'verschlüsselt',
  switchNetwork: 'Netz wechseln',
  password: 'Passwort',
  passwordPlaceholder: 'leer lassen für offene Netze',
  connect: 'Verbinden',
  connecting: 'Verbinde …',
  connectedTo: (ssid) => `Verbunden mit ${ssid}.`,
  noResponse:
    'Keine Rückmeldung. Wenn die Uhr in ein anderes Netz gewechselt ist, ist ' +
    'sie unter dieser Adresse nicht mehr erreichbar.',
  wifiHint: (host) =>
    'Die Uhr trennt sich kurz vom Netz. Klappt die Verbindung nicht, kehrt ' +
    'sie automatisch ins bisherige Netz zurück. Nach einem Wechsel kann sich ' +
    `die Adresse ändern — dann ist sie unter ${host}.local erreichbar.`,

  // --- update tab ---
  installed: 'Installiert',
  firmware: 'Firmware',
  webUi: 'Weboberfläche',
  used: 'Belegt',
  roomForUpdate: 'Platz für Update',
  unknown: 'unbekannt',
  uploadImage: 'Image hochladen',
  file: 'Datei',
  detectedAs: 'Erkannt als',
  firmwareImage: 'Firmware-Image',
  filesystemImage: 'Dateisystem-Image (Weboberfläche)',
  filesystemHint:
    'Das Dateisystem-Image überschreibt die gesamte Partition. Die ' +
    'Einstellungen bleiben erhalten — sie liegen im NVS, einer eigenen ' +
    'Partition, die ein Update nicht anfasst.',
  writing: (percent) => `Wird geschrieben … ${percent} %`,
  rebooting: 'Die Uhr startet neu — bitte warten.',
  updateDone: (firmware, webUi) =>
    `Update eingespielt. Firmware ${firmware}, Weboberfläche ${webUi}.`,
  upload: 'Hochladen und neu starten',
  running: 'Läuft …',
  noResponseAfterReboot:
    'Die Uhr hat sich nach dem Neustart nicht zurückgemeldet. Sie startet bei ' +
    'einem fehlerhaften Image mit der bisherigen Version.',
  buildHint:
    'firmware.bin und littlefs.bin entstehen mit "pio run" bzw. ' +
    '"pio run -t buildfs" im Ordner .pio/build/seeed_xiao_esp32s3/. Die Uhr ' +
    'prüft die Prüfsumme, bevor sie auf das neue Image umschaltet — ein ' +
    'abgebrochener Upload macht also nichts kaputt, sie startet dann einfach ' +
    'mit der bisherigen Version.',

  // --- update channel ---
  updateSource: 'Aktualisierung',
  channel: 'Kanal',
  channelStable: 'Stabil (getestete Versionen)',
  channelEdge: 'Entwicklung (jeder Stand)',
  autoUpdate: 'Automatisch installieren',
  autoUpdateHint:
    'Installiert nachts zwischen 2 und 5 Uhr, damit die Uhr abends nicht ' +
    'dunkel wird. Standardmaessig aus: ein fehlerhaftes Image laesst sich nur ' +
    'per USB-Kabel zurueckholen.',
  checkInterval: 'Pruefen alle',
  checkNever: 'nie',
  hours: (count) => `${count} h`,
  checkNow: 'Jetzt pruefen',
  checking: 'Wird geprueft ...',
  available: 'Verfuegbar',
  upToDate: 'Die Uhr ist auf dem neuesten Stand.',
  neverChecked: 'noch nicht geprueft',
  lastChecked: (text) => `Zuletzt geprueft: ${text}`,
  justNow: 'gerade eben',
  minutesAgo: (count) => `vor ${count} min`,
  hoursAgo: (count) => `vor ${count} h`,
  installNow: 'Jetzt aktualisieren',
  downloading: (percent) => `Wird geladen ... ${percent} %`,
  runningFrom: 'Aktiver Slot',

  // --- api errors ---
  connectionLost: 'Verbindung zur Uhr unterbrochen',
  uploadAborted: 'Upload abgebrochen'
};
