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
    'Test'
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
  measured: 'Misurato',
  ldrHint:
    'La luce ambientale viene misurata ma non regola ancora il display — ' +
    'il valore serve a giudicare la posizione del sensore.',

  // --- timezone tab ---
  timeServer: 'Server orario',
  ntpServer: 'Server NTP',
  tzPickerTitle: 'Località',
  tzRegion: 'Regione',
  tzPlace: 'Luogo',
  tzChoose: 'da scegliere',
  tzCustom: 'impostazione manuale',
  tzPickerHint:
    'La scelta compila le regole qui sotto; restano modificabili.',
  tzDataVersion: (version) => `Dati dei fusi ${version}.`,
  tzListUnavailable:
    'Non è stato possibile caricare l’elenco dei fusi. Le regole qui sotto ' +
    'restano impostabili a mano.',
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
  clockName: 'Nome dell’orologio',
  name: 'Nome',
  saveAndRestart: 'Salva e riavvia',
  restarting: 'L’orologio si sta riavviando …',
  hostnameSaved: (host) => `Riavviato. L’orologio ora risponde a ${host}.local.`,
  hostnameHint: () =>
    'Solo lettere, cifre e trattini. Il nome vale per mDNS, il router, espota ' +
    'e il punto di accesso di configurazione — l’orologio lo adotta quindi ' +
    'con un riavvio.',
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
  noFile: 'nessun file selezionato',
  chooseFile: 'Scegli un file',
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
    "L'orologio verifica la somma di controllo prima di passare alla nuova " +
    'immagine: un caricamento interrotto non fa danni, riparte semplicemente ' +
    'con la versione precedente.',

  // --- update channel ---
  updateSource: 'Aggiornamenti',
  channel: 'Canale',
  channelStable: 'Stabile (versioni testate)',
  channelEdge: 'Sviluppo (ogni build)',
  autoUpdate: 'Installa automaticamente',
  autoUpdateHint:
    "Installa di notte tra le 2 e le 5, così l'orologio non resta spento di " +
    "sera. Disattivato per impostazione predefinita: un'immagine difettosa si " +
    'recupera solo via USB.',
  checkInterval: 'Controlla ogni',
  checkNever: 'mai',
  hours: (count) => `${count} h`,
  checkNow: 'Controlla ora',
  checking: 'Controllo in corso …',
  available: 'Disponibile',
  upToDate: "L'orologio è aggiornato.",
  neverChecked: 'non ancora controllato',
  lastChecked: (text) => `Ultimo controllo: ${text}`,
  justNow: 'proprio ora',
  minutesAgo: (count) => `${count} min fa`,
  hoursAgo: (count) => `${count} h fa`,
  installNow: 'Aggiorna ora',
  downloading: (percent) => `Download … ${percent} %`,
  runningFrom: 'Slot attivo',

  // --- error codes reported by the clock (see lib/errors.js) ---
  err_otaBegin: 'Aggiornamento rifiutato',
  err_otaWrite: 'Errore di scrittura durante il flash',
  err_otaIncomplete: 'Immagine incompleta',
  err_otaAborted: 'Caricamento interrotto',
  err_otaNoImage: 'Nessuna immagine ricevuta',
  err_otaNoUpdate: 'Nessun aggiornamento disponibile',
  err_otaBusy: 'Un aggiornamento è già in corso',
  err_otaServer: 'Server di aggiornamento non raggiungibile',
  err_otaManifestHttp: 'Manifest non recuperabile',
  err_otaManifestParse: 'Manifest illeggibile',
  err_otaDownload: 'Download non riuscito',
  err_otaConnectionLost: 'Connessione persa durante il download',
  err_otaSize: 'Download incompleto',
  err_otaChecksum: 'Somma di controllo errata — immagine scartata',
  err_hostnameInvalid: 'Questo nome non contiene caratteri utilizzabili',
  err_wifiConnect: (ssid) => `Impossibile connettersi a «${ssid}»`,
  err_wifiFallback: (ssid) => `Anche il ritorno a «${ssid}» non è riuscito`,

  // --- api errors ---
  connectionLost: "Collegamento con l'orologio interrotto",
  uploadAborted: 'Caricamento interrotto'
};
