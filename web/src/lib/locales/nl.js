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
    'firmware.bin en littlefs.bin ontstaan met "pio run" respectievelijk ' +
    '"pio run -t buildfs" in de map .pio/build/seeed_xiao_esp32s3/. De klok ' +
    'controleert de checksum voordat hij naar het nieuwe image overschakelt — ' +
    'een afgebroken upload richt dus geen schade aan, hij start gewoon op met ' +
    'de vorige versie.',

  // --- api errors ---
  connectionLost: 'Verbinding met de klok verbroken',
  uploadAborted: 'Upload afgebroken'
};
