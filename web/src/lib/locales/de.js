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
  tabs: ['Anzeige', 'Farbe', 'Zeitzone', 'WLAN', 'Update', 'Helligkeit', 'Debug', 'Speicher'],
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
  decreaseBy: (label, step) => `${label} um ${step} verringern`,
  increaseBy: (label, step) => `${label} um ${step} erhöhen`,
  automatic: 'Automatik',
  autoBrightness: 'Helligkeit automatisch',
  measured: 'Gemessen',
  calibration: 'Kalibrierung',
  calTaught: (n) => `${n} gelernte Punkte`,
  calReset: 'Zurücksetzen',
  calHint:
    'Im Automatikbetrieb bedeutet der Helligkeitsregler „bei diesem Licht ' +
    'hätte ich es gerne so hell“. Zehn Sekunden nach der letzten Änderung ' +
    'merkt sich die Uhr das Paar und legt eine neue Gerade durch alles ' +
    'Gelernte. Nichts zu drücken.',
  ldrHint:
    'Die Anzeige folgt dem Umgebungslicht, logarithmisch — so nimmt das ' +
    'Auge wahr. Geregelt wird zwischen 20 und 100 %. Der Wert in Klammern ' +
    'ist ungeglättet und hilft beim Platzieren des Sensors.',
  lumTitle: 'Helligkeitskurve',
  lumHint:
    'Was die Automatik gelernt hat und was sie daraus schließt. Nur zum ' +
    'Ansehen — zurückgesetzt wird im Farbreiter.',
  lumApplied: 'Angezeigt',
  lumLine: 'Gerade',
  lumSlope: 'Steigung',
  lumSlopeFitted: 'aus den Punkten berechnet',
  lumSlopeKept: 'beibehalten, Punkte liegen zu eng',
  lumAnchor: 'Die Steigung kommt aus allen Punkten (kleinste Quadrate), die Höhe allein aus dem neuesten — die Gerade geht deshalb genau durch ihn und an den älteren vorbei. So ist eine Korrektur beim nächsten Mal auch wirklich die, um die gebeten wurde.',
  lumNewest: 'neuester Punkt — auf ihn ist die Gerade festgelegt',
  lumAdjusting: (want, secs) => `${want} % eingestellt — wird in bis zu ${secs} s gelernt`,
  lumPoints: 'Gelernte Punkte',
  lumWanted: 'gewollt',
  lumCurve: 'Gerade',
  lumWhen: 'Laufzeit',
  lumEmpty: 'Noch nichts gelernt — es gilt die Standardgerade.',
  lumRange: 'Regelbereich',
  lumRangeHint: 'Wie dunkel und wie hell die Automatik gehen darf. Darunter und darüber bleibt die Kurve flach — das sind die beiden gepunkteten Linien im Diagramm. Aus schaltet nur der Anzeigemodus, nie der Sensor.',
  lumRangeMin: 'dunkelste',
  lumRangeMax: 'hellste',
  lumReadOnly: 'Nur Ansicht. Zum Bearbeiten die Uhr im Expertenmodus entsperren.',
  lumCoupling: 'Eigenlicht',
  lumCoupledCells: (n) => `${n} Zellen vermessen`,
  lumCoupledNone: 'Nicht vermessen — die Uhr rechnet ihr eigenes Licht nicht heraus.',
  lumDisplayShare: (lx, raw) => `${lx} lx von ${raw} lx roh sind die Anzeige selbst`,
  lumForget: 'Vergessen',
  lumForgetTitle: 'Diesen Punkt vergessen',
  lumResetPoints: 'Alle Punkte vergessen',
  lumResetCoupling: 'Eigenlicht-Messung löschen',
  lumResetCouplingHint: 'Danach regelt die Uhr wieder auf den rohen Messwert — mit der Rückkopplung durch die eigene Anzeige.',
  lumCalibrate: 'Eigenlicht selbst vermessen',
  lumCalibrateHint: (max) => `Die Uhr tastet rund 90 s lang jede Zelle einzeln ab. Dafür muss es dunkel sein — Uhr abdecken oder Raum verdunkeln; über ${max} lx bricht sie ab.`,
  lumCalibrateAbort: 'Abbrechen',
  lumCalibratePhases: ['bereit', 'Umgebungslicht prüfen', 'Empfindlichkeit wählen', 'Zellen abtasten', 'Farbkanäle messen', 'Treiberkennlinie', 'wird gespeichert', 'fertig', 'fehlgeschlagen'],
  lumCalibrateResult: (cells, rung) => `${cells} Zellen vermessen, Sprosse ${rung}`,
  lumCalibrateAmbient: (lx) => `Umgebung ${lx} lx`,

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

  // --- debug tab ---
  clockState: 'Zustand',
  uptime: 'Laufzeit',
  lastReset: 'Letzter Neustart',
  resetReasons: {
    'power-on': 'Einschalten',
    external: 'Reset-Leitung',
    software: 'Software (Neustart angefordert)',
    panic: 'Absturz (Panic)',
    'watchdog-int': 'Watchdog (Interrupt)',
    'watchdog-task': 'Watchdog (Task)',
    watchdog: 'Watchdog',
    brownout: 'Spannungseinbruch',
    sdio: 'SDIO',
    usb: 'Zurückgesetzt über USB',
    jtag: 'Zurückgesetzt über JTAG',
    efuse: 'eFuse-Fehler',
    'power-glitch': 'Spannungsspitze',
    'cpu-lockup': 'CPU-Blockade (Doppelfehler)',
    'deep-sleep': 'Tiefschlaf',
    unknown: 'unbekannt'
  },
  heapFree: 'Freier Speicher',
  heapMin: 'Minimum seit Start',
  heapBlock: 'Größter freier Block',
  heapHint:
    'Wird ein Update mit „Firmware konnte nicht aktiviert werden“ abgelehnt, ' +
    'ist das Minimum die erste Zahl, die anzusehen sich lohnt.',
  logTitle: 'Protokoll',
  logPause: 'Anhalten',
  logResume: 'Fortsetzen',
  logClear: 'Fenster leeren',
  logMissed: (count) =>
    `${count} Zeilen sind aus dem Speicher gelaufen, bevor sie hier ankamen.`,
  logEmpty: 'Noch nichts protokolliert.',
  logHint:
    'Die Uhr hält die letzten 200 Zeilen im Arbeitsspeicher, auch die vom ' +
    'Hochfahren. Was der Bootloader vor der Firmware ausgibt, steht nur am ' +
    'USB-Kabel.',

  // --- storage tab: the filesystem and NVS, one explorer for both ---
  storageTitle: 'Speicher',
  storageFs: 'LittleFS',
  storageNvs: 'NVS',
  storageFsHint:
    'Das Dateisystem der Uhr — dieselbe Partition, aus der diese Seite ' +
    'kommt. Ein Update der Weboberfläche überschreibt sie vollständig.',
  storageFsWarn:
    'Wer index.html löscht, erreicht die Uhr nur noch über die ' +
    'Schnittstelle und braucht das USB-Kabel.',
  storageNvsHint:
    'Kein Dateisystem, sondern Schlüssel und Werte: Namensräume als ' +
    'Ordner, Schlüssel als Dateien. Die Endung ist eine Deutung des ' +
    'Inhalts, kein gespeicherter Name. Ein Update lässt NVS unberührt — ' +
    'deshalb liegen Einstellungen, Kennwort und Helligkeitskurve hier.',
  storageNvsWarn:
    'Die Uhr hält ihre Einstellungen im Arbeitsspeicher und schreibt sie ' +
    'bei der nächsten Änderung zurück. Eine Bearbeitung hier überlebt nur ' +
    'ein sofortiger Neustart.',
  fsUsageEntries: (used, total) => `${used} von ${total} Einträgen belegt`,
  fsKeys: (n) => `${n} Schlüssel`,
  fsRestart: 'Jetzt neu starten',
  fsRestarting: 'Startet neu …',
  fsConfirmRestart: 'Die Uhr jetzt neu starten? Sie ist ein paar Sekunden lang nicht erreichbar.',
  fsRestarted: 'Neustart angefordert — die Seite in ein paar Sekunden neu laden.',
  fsGesture: 'Rechte Maustaste oder langes Antippen öffnet das Menü.',
  fsNewFolderHere: 'Ordner darin anlegen',
  fsProtected: 'Nicht lesbar (Kennwort-Hash)',
  fsCompact: 'kompakt speichern',
  fsNotJson: 'kein gültiges JSON',

  // --- the explorer itself ---
  fsUsage: (used, total) => `${used} von ${total} belegt`,
  fsRoot: 'Wurzel',
  fsEmpty: 'Dieser Ordner ist leer.',
  fsTruncated: 'Nur die ersten Einträge — der Ordner enthält mehr.',
  fsDownload: 'Herunterladen',
  fsEdit: 'Bearbeiten',
  fsDelete: 'Löschen',
  fsUpload: 'Hochladen',
  fsNewFolder: 'Neuer Ordner',
  fsFolderName: 'Name des Ordners',
  fsSave: 'Speichern',
  fsCancel: 'Abbrechen',
  fsSaved: 'Gespeichert.',
  fsConfirmDelete: (name) => `„${name}“ endgültig löschen?`,
  fsUploading: (percent) => `Wird übertragen … ${percent} %`,
  fsTooLarge: 'Zu groß für den Editor — herunterladen und bearbeiten.',
  fsBinary: 'Kein Text — nur zum Herunterladen.',

  // --- expert mode ---
  expertTitle: 'Expertenmodus',
  expertUnlocked:
    'Der Expertenmodus ist eingeschaltet. Update- und Debug-Reiter sind ' +
    'erreichbar — für jeden, der im selben Netz ist.',
  expertLock: 'Wieder sperren',
  expertEnterHint:
    'Kennwort eingeben, um Update- und Debug-Reiter freizuschalten. Die ' +
    'Freischaltung bleibt über Neustarts hinweg bestehen.',
  expertSetHint:
    'Auf dieser Uhr ist noch kein Kennwort gesetzt. Das erste hier ' +
    'vergebene gilt; bis dahin bleiben Update und Debug gesperrt.',
  expertPassword: 'Kennwort',
  expertUnlock: 'Freischalten',
  expertSet: 'Kennwort festlegen',
  expertMinLength: (count) => `Mindestens ${count} Zeichen.`,
  expertLockedOut:
    'Zu viele Fehlversuche. Die Uhr nimmt für einige Minuten kein Kennwort ' +
    'mehr an.',
  expertForgotten: 'Kennwort vergessen?',
  expertResetHint: (time) =>
    `Kurz nach dem Einschalten am Stecker lässt sich das Kennwort löschen. Noch ${time}. Danach hilft nur, den Strom zu nehmen — oder das USB-Kabel.`,
  expertReset: 'Kennwort löschen',

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
  err_lumNoSuchPoint: 'Diesen Punkt gibt es nicht mehr — vermutlich woanders schon gelöscht',
  err_lumRange: 'Regelbereich ungültig — 1 bis 100 %, und mindestens 5 % Abstand',
  err_calibTooBright: 'Zu hell für eine Messung — Uhr abdecken oder Raum verdunkeln',
  err_calibBusy: 'Es läuft schon eine Messung',
  err_calibLabActive: 'Das Labor hält gerade die LEDs',
  err_calibNoSensor: 'Diese Uhr hat keinen Lichtsensor',
  err_calibSaturated: 'Der Sensor wird auf jeder Empfindlichkeitsstufe übersteuert',
  err_calibNoCoupling: 'Keine Zelle erreicht den Sensor',
  err_calibStore: 'Die Messung ließ sich nicht speichern',
  err_calibCancelled: 'Abgebrochen',
  err_calibNoTask: 'Kein Speicher für die Messung',
  err_labCalibrating: 'Die Uhr vermisst gerade ihr Eigenlicht',
  err_couplingInvalid: 'Die Eigenlicht-Messung ist unlesbar',
  err_hostnameInvalid: 'Dieser Name enthält keine verwendbaren Zeichen',
  err_wifiConnect: (ssid) => `Verbindung zu „${ssid}“ fehlgeschlagen`,
  err_wifiFallback: (ssid) => `Auch der Rückfall auf „${ssid}“ ist fehlgeschlagen`,

  err_languageNotOnPanel: 'Diese Sprache passt nicht zum Buchstabenpanel der Uhr — im Expertenmodus änderbar',
  err_expertLocked: 'Der Expertenmodus ist gesperrt',
  err_expertLockedOut: 'Zu viele Fehlversuche — bitte später erneut',
  err_expertWrongPassword: 'Falsches Kennwort',
  err_expertPasswordShort: 'Das Kennwort ist zu kurz',
  err_expertNoGrace: 'Das Zeitfenster zum Zurücksetzen ist geschlossen',

  err_fsPath: 'Ungültiger Pfad',
  err_fsBody: 'Anfrage unlesbar',
  err_fsNotFound: 'Nicht gefunden',
  err_fsNotDir: 'Das ist kein Ordner',
  err_fsIsDir: 'Das ist ein Ordner',
  err_fsOpen: 'Datei lässt sich nicht anlegen',
  err_fsWrite: 'Schreibfehler — vermutlich ist das Dateisystem voll',
  err_fsRename: 'Datei konnte nicht an ihren Platz gebracht werden',
  err_fsAborted: 'Übertragung abgebrochen',
  err_fsTooBig: 'Zu groß für den Editor',
  err_fsNotEmpty: 'Der Ordner ist nicht leer',
  err_fsExists: 'Gibt es schon',
  err_fsDelete: 'Löschen fehlgeschlagen',
  err_fsMkdir: 'Ordner konnte nicht angelegt werden',

  err_nvsPath: 'Namensraum oder Schlüssel fehlt',
  err_nvsBody: 'Anfrage unlesbar',
  err_nvsNamespace: 'Namensraum nicht gefunden',
  err_nvsNotFound: 'Schlüssel nicht gefunden',
  err_nvsProtected: 'Dieser Wert wird nicht herausgegeben',
  err_nvsBinary: 'Binärwert — nur zum Herunterladen',
  err_nvsTooBig: 'Zu groß für den Editor',
  err_nvsNotANumber: 'Dieser Schlüssel hält eine Zahl',
  err_nvsRead: 'Wert nicht lesbar',
  err_nvsWrite: 'Wert konnte nicht geschrieben werden',
  err_nvsDelete: 'Löschen fehlgeschlagen',
  err_nvsMemory: 'Zu wenig Arbeitsspeicher',
  err_nvsNamespaceDelete: 'Ein Namensraum verschwindet mit seinem letzten Schlüssel',

  // --- api errors ---
  connectionLost: 'Verbindung zur Uhr unterbrochen',
  uploadAborted: 'Upload abgebrochen'
};
