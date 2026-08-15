/**
 * it
 * Italian texts of the configuration UI. Keys follow locales/de.js.
 *
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.0
 * @created  15.8.2026
 * @updated  15.8.2026
 */
export default {
  // --- shell ---
  tabs: ['Visualizzazione', 'Colore', 'Fuso orario', 'WiFi', 'Aggiornamento'],
  loading: 'Caricamento delle impostazioni …',
  loadingShort: 'caricamento …',
  clockUnreachable: 'Orologio non raggiungibile',
  retry: 'Riprova',
  writeFailed: 'Trasmissione non riuscita',

  // --- display tab ---
  displayTitle: 'Visualizzazione',
  modes: [
    'Ora',
    'Ora con stato WiFi',
    'Spento (buio)',
    'Secondi',
    'Test',
    'Stato'
  ],
  appearance: 'Aspetto',
  language: 'Lingua',
  languages: [
    'Tedesco',
    'Svevo',
    'Bavarese',
    'Sassone',
    'Svizzero tedesco',
    'Inglese',
    'Francese',
    'Italiano',
    'Olandese',
    'Spagnolo'
  ],
  corners: 'Angoli',
  clockwise: 'in senso orario',
  counterClockwise: 'in senso antiorario',
  minutes: 'Minuti',
  monochrome: 'monocromatico',
  colored: 'a colori',

  // --- colour tab ---
  colorTitle: 'Colore',
  hue: 'Tonalità',
  saturation: 'Saturazione',
  brightness: 'Luminosità',
  preview: ['SONO LE', 'DUE', 'E CINQUE'],
  decreaseBy: (label, step) => `Riduci ${label} di ${step}`,
  increaseBy: (label, step) => `Aumenta ${label} di ${step}`,
  automatic: 'Automatico',
  autoBrightness: 'Luminosità automatica',
  ldrHint:
    "Senza effetto finché la valutazione dell'LDR è commentata nel firmware " +
    '(vedi src/LDR.cpp).',

  // --- timezone tab ---
  timeServer: 'Server orario',
  ntpServer: 'Server NTP',
  timezoneTitle: 'Fuso orario',
  dst: 'Ora legale',
  standardTime: 'Ora solare',
  noDstHint:
    "Senza ora legale vale tutto l'anno lo scarto dell'ora solare; i momenti " +
    'di cambio non vengono considerati.',
  abbreviation: 'Sigla',
  week: 'Settimana',
  weeks: ['Ultima', '1ª', '2ª', '3ª', '4ª'],
  day: 'Giorno',
  days: ['dom', 'lun', 'mar', 'mer', 'gio', 'ven', 'sab'],
  month: 'Mese',
  months: [
    'gen', 'feb', 'mar', 'apr', 'mag', 'giu',
    'lug', 'ago', 'set', 'ott', 'nov', 'dic'
  ],
  hour: 'Ora',
  offsetMin: 'Scarto (min)',

  // --- wifi tab ---
  connection: 'Connessione',
  network: 'Rete',
  address: 'Indirizzo',
  signal: 'Segnale',
  hostname: 'Nome host',
  mac: 'MAC',
  quality: ['debole', 'medio', 'buono', 'ottimo'],
  statusUnavailable: 'Stato non disponibile',
  availableNetworks: 'Reti disponibili',
  scanning: 'Ricerca in corso …',
  noNetworks: 'Nessuna rete trovata.',
  rescan: 'Cerca di nuovo',
  encrypted: 'protetta',
  switchNetwork: 'Cambia rete',
  password: 'Password',
  passwordPlaceholder: 'lasciare vuoto per reti aperte',
  connect: 'Connetti',
  connecting: 'Connessione …',
  connectedTo: (ssid) => `Connesso a ${ssid}.`,
  noResponse:
    "Nessuna risposta. Se l'orologio è passato a un'altra rete, non è più " +
    'raggiungibile a questo indirizzo.',
  wifiHint: (host) =>
    "L'orologio lascia brevemente la rete. Se la connessione non riesce, " +
    "torna da solo alla rete precedente. Dopo un cambio l'indirizzo può " +
    `cambiare — allora è raggiungibile su ${host}.local.`,

  // --- update tab ---
  installed: 'Installato',
  firmware: 'Firmware',
  webUi: 'Interfaccia web',
  used: 'Occupato',
  roomForUpdate: 'Spazio per aggiornamento',
  unknown: 'sconosciuto',
  uploadImage: 'Carica immagine',
  file: 'File',
  detectedAs: 'Riconosciuto come',
  firmwareImage: 'Immagine del firmware',
  filesystemImage: 'Immagine del file system (interfaccia web)',
  filesystemHint:
    "L'immagine del file system sovrascrive l'intera partizione. Le " +
    'impostazioni restano — si trovano nella NVS, una partizione a sé che un ' +
    'aggiornamento non tocca.',
  writing: (percent) => `Scrittura … ${percent} %`,
  rebooting: "L'orologio si riavvia — attendere.",
  updateDone: (firmware, webUi) =>
    `Aggiornamento installato. Firmware ${firmware}, interfaccia web ${webUi}.`,
  upload: 'Carica e riavvia',
  running: 'In corso …',
  noResponseAfterReboot:
    "L'orologio non si è più fatto sentire dopo il riavvio. Con un'immagine " +
    'difettosa riparte con la versione precedente.',
  buildHint:
    'firmware.bin e littlefs.bin si ottengono con "pio run" e ' +
    '"pio run -t buildfs" nella cartella .pio/build/seeed_xiao_esp32s3/. ' +
    "L'orologio verifica la somma di controllo prima di passare alla nuova " +
    'immagine: un caricamento interrotto non fa danni, riparte semplicemente ' +
    'con la versione precedente.',

  // --- api errors ---
  connectionLost: "Collegamento con l'orologio interrotto",
  uploadAborted: 'Caricamento interrotto'
};
