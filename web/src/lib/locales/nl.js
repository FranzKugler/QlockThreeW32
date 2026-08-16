/**
 * nl
 * Dutch texts of the configuration UI. Keys follow locales/de.js.
 *
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.0
 * @created  15.8.2026
 * @updated  15.8.2026
 */
export default {
  // --- shell ---
  tabs: ['Weergave', 'Kleur', 'Tijdzone', 'WiFi', 'Update'],
  loading: 'Instellingen worden geladen …',
  loadingShort: 'wordt geladen …',
  clockUnreachable: 'Klok niet bereikbaar',
  retry: 'Opnieuw proberen',
  writeFailed: 'Verzenden mislukt',

  // --- display tab ---
  displayTitle: 'Weergave',
  modes: [
    'Tijd',
    'Tijd met WiFi-status',
    'Uit (donker)',
    'Seconden',
    'Test',
    'Status'
  ],
  appearance: 'Vormgeving',
  language: 'Taal',
  languages: [
    'Duits',
    'Zwabisch',
    'Beiers',
    'Saksisch',
    'Zwitserduits',
    'Engels',
    'Frans',
    'Italiaans',
    'Nederlands',
    'Spaans'
  ],
  corners: 'Hoeken',
  clockwise: 'met de klok mee',
  counterClockwise: 'tegen de klok in',
  minutes: 'Minuten',
  monochrome: 'monochroom',
  colored: 'in kleur',

  // --- colour tab ---
  colorTitle: 'Kleur',
  hue: 'Kleurtoon',
  saturation: 'Verzadiging',
  brightness: 'Helderheid',
  preview: ['HET IS', 'VIJF OVER', 'TWEE'],
  decreaseBy: (label, step) => `${label} met ${step} verlagen`,
  increaseBy: (label, step) => `${label} met ${step} verhogen`,
  automatic: 'Automatisch',
  autoBrightness: 'Automatische helderheid',
  ldrHint:
    'Zonder effect zolang de LDR-verwerking in de firmware is ' +
    'uitgecommentarieerd (zie src/LDR.cpp).',

  // --- timezone tab ---
  timeServer: 'Tijdserver',
  ntpServer: 'NTP-server',
  tzPickerTitle: 'Locatie',
  tzRegion: 'Regio',
  tzPlace: 'Plaats',
  tzChoose: 'kies een plaats',
  tzCustom: 'handmatig ingesteld',
  tzPickerHint:
    'De keuze vult de regels hieronder in; ze blijven aanpasbaar.',
  tzDataVersion: (version) => `Zonegegevens ${version}.`,
  tzListUnavailable:
    'De zonelijst kon niet worden geladen. De regels hieronder zijn nog ' +
    'steeds handmatig in te stellen.',
  timezoneTitle: 'Tijdzone',
  dst: 'Zomertijd',
  standardTime: 'Standaardtijd',
  noDstHint:
    'Zonder zomertijd geldt het hele jaar de offset van de standaardtijd; de ' +
    'omschakelmomenten worden niet gebruikt.',
  abbreviation: 'Afkorting',
  week: 'Week',
  weeks: ['Laatste', '1e', '2e', '3e', '4e'],
  day: 'Dag',
  days: ['zo', 'ma', 'di', 'wo', 'do', 'vr', 'za'],
  month: 'Maand',
  months: [
    'jan', 'feb', 'mrt', 'apr', 'mei', 'jun',
    'jul', 'aug', 'sep', 'okt', 'nov', 'dec'
  ],
  hour: 'Uur',
  offsetMin: 'Offset (min)',

  // --- wifi tab ---
  connection: 'Verbinding',
  network: 'Netwerk',
  address: 'Adres',
  signal: 'Signaal',
  hostname: 'Hostnaam',
  mac: 'MAC',
  quality: ['zwak', 'matig', 'goed', 'zeer goed'],
  statusUnavailable: 'Status niet op te vragen',
  clockName: 'Naam van de klok',
  name: 'Naam',
  saveAndRestart: 'Opslaan en herstarten',
  restarting: 'De klok start opnieuw op …',
  hostnameSaved: (host) => `Herstart. De klok is nu bereikbaar op ${host}.local.`,
  hostnameHint: () =>
    'Alleen letters, cijfers en koppeltekens. De naam geldt voor mDNS, de ' +
    'router, espota en het instel-accesspoint — de klok neemt hem daarom over ' +
    'met een herstart.',
  availableNetworks: 'Beschikbare netwerken',
  scanning: 'Bezig met zoeken …',
  noNetworks: 'Geen netwerken gevonden.',
  rescan: 'Opnieuw zoeken',
  encrypted: 'versleuteld',
  switchNetwork: 'Netwerk wisselen',
  password: 'Wachtwoord',
  passwordPlaceholder: 'leeg laten voor open netwerken',
  connect: 'Verbinden',
  connecting: 'Verbinden …',
  connectedTo: (ssid) => `Verbonden met ${ssid}.`,
  noResponse:
    'Geen reactie. Als de klok naar een ander netwerk is overgestapt, is hij ' +
    'op dit adres niet meer bereikbaar.',
  wifiHint: (host) =>
    'De klok verlaat het netwerk even. Lukt de verbinding niet, dan keert hij ' +
    'vanzelf terug naar het vorige netwerk. Na een wissel kan het adres ' +
    `veranderen — dan is hij bereikbaar op ${host}.local.`,

  // --- update tab ---
  installed: 'Geïnstalleerd',
  firmware: 'Firmware',
  webUi: 'Webinterface',
  used: 'Bezet',
  roomForUpdate: 'Ruimte voor update',
  unknown: 'onbekend',
  uploadImage: 'Image uploaden',
  file: 'Bestand',
  noFile: 'geen bestand gekozen',
  chooseFile: 'Bestand kiezen',
  detectedAs: 'Herkend als',
  firmwareImage: 'Firmware-image',
  filesystemImage: 'Bestandssysteem-image (webinterface)',
  filesystemHint:
    'Het bestandssysteem-image overschrijft de hele partitie. De instellingen ' +
    'blijven behouden — ze staan in NVS, een eigen partitie waar een update ' +
    'niet aan komt.',
  writing: (percent) => `Wordt geschreven … ${percent} %`,
  rebooting: 'De klok start opnieuw op — even geduld.',
  updateDone: (firmware, webUi) =>
    `Update geïnstalleerd. Firmware ${firmware}, webinterface ${webUi}.`,
  upload: 'Uploaden en herstarten',
  running: 'Bezig …',
  noResponseAfterReboot:
    'De klok heeft zich na de herstart niet gemeld. Bij een onjuist image ' +
    'start hij op met de vorige versie.',
  buildHint:
    'De klok controleert de checksum voordat hij naar het nieuwe image ' +
    'overschakelt — een afgebroken upload richt dus geen schade aan, hij start ' +
    'gewoon op met de vorige versie.',

  // --- update channel ---
  updateSource: 'Updates',
  channel: 'Kanaal',
  channelStable: 'Stabiel (geteste versies)',
  channelEdge: 'Ontwikkeling (elke build)',
  autoUpdate: 'Automatisch installeren',
  autoUpdateHint:
    "'s Nachts tussen 2 en 5 uur, zodat de klok 's avonds niet donker wordt. " +
    'Standaard uit: een onjuist image is alleen via USB te herstellen.',
  checkInterval: 'Controleren elke',
  checkNever: 'nooit',
  hours: (count) => `${count} u`,
  checkNow: 'Nu controleren',
  checking: 'Bezig met controleren …',
  available: 'Beschikbaar',
  upToDate: 'De klok is up-to-date.',
  neverChecked: 'nog niet gecontroleerd',
  lastChecked: (text) => `Laatst gecontroleerd: ${text}`,
  justNow: 'zojuist',
  minutesAgo: (count) => `${count} min geleden`,
  hoursAgo: (count) => `${count} u geleden`,
  installNow: 'Nu bijwerken',
  downloading: (percent) => `Wordt gedownload … ${percent} %`,
  runningFrom: 'Actieve slot',

  // --- error codes reported by the clock (see lib/errors.js) ---
  err_otaBegin: 'Update geweigerd',
  err_otaWrite: 'Schrijffout tijdens het flashen',
  err_otaIncomplete: 'Image onvolledig',
  err_otaAborted: 'Upload afgebroken',
  err_otaNoImage: 'Geen image ontvangen',
  err_otaNoUpdate: 'Geen update beschikbaar',
  err_otaBusy: 'Er loopt al een update',
  err_otaServer: 'Updateserver niet bereikbaar',
  err_otaManifestHttp: 'Manifest niet op te halen',
  err_otaManifestParse: 'Manifest onleesbaar',
  err_otaDownload: 'Download mislukt',
  err_otaConnectionLost: 'Verbinding verbroken tijdens de download',
  err_otaSize: 'Download onvolledig',
  err_otaChecksum: 'Controlesom klopt niet — image verworpen',
  err_otaBegin: 'De update kon niet worden gestart',
  err_otaWrite: 'Schrijven naar flash is mislukt',
  err_hostnameInvalid: 'Deze naam bevat geen bruikbare tekens',
  err_wifiConnect: (ssid) => `Verbinden met “${ssid}” is mislukt`,
  err_wifiFallback: (ssid) => `Terugvallen op “${ssid}” is ook mislukt`,

  // --- api errors ---
  connectionLost: 'Verbinding met de klok verbroken',
  uploadAborted: 'Upload afgebroken'
};
