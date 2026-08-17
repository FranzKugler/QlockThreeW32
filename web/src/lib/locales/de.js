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
    'Test'
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
  measured: 'Gemessen',
  calibration: 'Kalibrierung',
  calDark: 'Dunkel',
  calBright: 'Hell',
  calCapture: 'Jetzt merken',
  calReset: 'Zurücksetzen',
  calHint:
    'Helligkeit so einstellen, wie sie beim aktuellen Umgebungslicht sein ' +
    'soll, und merken — einmal im Dunkeln, einmal bei Tageslicht. Alles ' +
    'dazwischen regelt die Uhr selbst.',
  ldrHint:
    'Die Anzeige folgt dem Umgebungslicht, zwischen den beiden Punkten ' +
    'logarithmisch — so nimmt das Auge wahr. Der Wert in Klammern ist ' +
    'ungeglättet und hilft beim Platzieren des Sensors.',

  // --- timezone tab ---
  timeServer: 'Zeitserver',
  ntpServer: 'NTP-Server',
  tzPickerTitle: 'Standort',
  tzRegion: 'Region',
  tzPlace: 'Ort',
  tzChoose: 'bitte wählen',
  tzCustom: 'eigene Einstellung',
  tzPickerHint: 'Die Auswahl füllt die Regeln unten aus; änderbar bleiben sie.',
  tzDataVersion: (version) => `Zonendaten ${version}.`,
  tzListUnavailable:
    'Die Zonenliste ließ sich nicht laden. Die Regeln unten lassen sich ' +
    'unverändert von Hand einstellen.',
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
  clockName: 'Name der Uhr',
  name: 'Name',
  saveAndRestart: 'Speichern und neu starten',
  restarting: 'Die Uhr startet neu …',
  hostnameSaved: (host) => `Neu gestartet. Die Uhr ist jetzt als ${host}.local erreichbar.`,
  hostnameHint: () =>
    'Nur Buchstaben, Ziffern und Bindestriche. Der Name gilt für mDNS, den ' +
    'Router, espota und den Einrichtungs-Accesspoint — die Uhr übernimmt ihn ' +
    'deshalb mit einem Neustart.',
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
  noFile: 'keine Datei ausgewählt',
  chooseFile: 'Datei auswählen',
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
    'Die Uhr prüft die Prüfsumme, bevor sie auf das neue Image umschaltet — ' +
    'ein abgebrochener Upload macht also nichts kaputt, sie startet dann ' +
    'einfach mit der bisherigen Version.',

  // --- update channel ---
  updateSource: 'Aktualisierung',
  channel: 'Kanal',
  channelStable: 'Stabil (getestete Versionen)',
  channelEdge: 'Entwicklung (jeder Stand)',
  autoUpdate: 'Automatisch installieren',
  autoUpdateHint:
    'Installiert nachts zwischen 2 und 5 Uhr, damit die Uhr abends nicht ' +
    'dunkel wird. Standardmäßig aus: ein fehlerhaftes Image lässt sich nur ' +
    'per USB-Kabel zurückholen.',
  checkInterval: 'Prüfen alle',
  checkNever: 'nie',
  hours: (count) => `${count} h`,
  checkNow: 'Jetzt prüfen',
  checking: 'Wird geprüft …',
  available: 'Verfügbar',
  upToDate: 'Die Uhr ist auf dem neuesten Stand.',
  neverChecked: 'noch nicht geprüft',
  lastChecked: (text) => `Zuletzt geprüft: ${text}`,
  justNow: 'gerade eben',
  minutesAgo: (count) => `vor ${count} min`,
  hoursAgo: (count) => `vor ${count} h`,
  installNow: 'Jetzt aktualisieren',
  downloading: (percent) => `Wird geladen … ${percent} %`,
  runningFrom: 'Aktiver Slot',

  // --- error codes reported by the clock (see lib/errors.js) ---
  err_otaBegin: 'Update abgelehnt',
  err_otaWrite: 'Schreibfehler beim Flashen',
  err_otaIncomplete: 'Image unvollständig',
  err_otaAborted: 'Upload abgebrochen',
  err_otaNoImage: 'Kein Image empfangen',
  err_otaNoUpdate: 'Kein Update verfügbar',
  err_otaBusy: 'Es läuft bereits ein Update',
  err_otaServer: 'Update-Server nicht erreichbar',
  err_otaManifestHttp: 'Manifest nicht abrufbar',
  err_otaManifestParse: 'Manifest unlesbar',
  err_otaDownload: 'Download fehlgeschlagen',
  err_otaConnectionLost: 'Verbindung während des Downloads abgerissen',
  err_otaSize: 'Download unvollständig',
  err_otaChecksum: 'Prüfsumme stimmt nicht — Image verworfen',
  err_calibrationTooClose: 'Die beiden Punkte liegen zu dicht beieinander',
  err_calibrationRange: 'Helligkeit außerhalb des gültigen Bereichs',
  err_hostnameInvalid: 'Dieser Name enthält keine verwendbaren Zeichen',
  err_wifiConnect: (ssid) => `Verbindung zu „${ssid}“ fehlgeschlagen`,
  err_wifiFallback: (ssid) => `Auch der Rückfall auf „${ssid}“ ist fehlgeschlagen`,

  // --- api errors ---
  connectionLost: 'Verbindung zur Uhr unterbrochen',
  uploadAborted: 'Upload abgebrochen'
};
