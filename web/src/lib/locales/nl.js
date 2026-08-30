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
  tabs: ['Weergave', 'Kleur', 'Tijdzone', 'WiFi', 'Update', 'Helderheid', 'Debug', 'Opslag'],
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
    'Test'
  ],
  appearance: 'Vormgeving',
  language: 'Taal',
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
  decreaseBy: (label, step) => `${label} met ${step} verlagen`,
  increaseBy: (label, step) => `${label} met ${step} verhogen`,
  automatic: 'Automatisch',
  autoBrightness: 'Automatische helderheid',
  measured: 'Gemeten',
  calibration: 'Kalibratie',
  calTaught: (n) => `${n} geleerde punten`,
  calReset: 'Herstellen',
  calHint:
    'In de automatische stand betekent de helderheidsschuif „bij dit licht ' +
    'wil ik het zo helder”. Tien seconden na de laatste wijziging onthoudt ' +
    'de klok het paar en legt een nieuwe lijn door alles wat ze geleerd ' +
    'heeft. Niets te drukken.',
  ldrHint:
    'Het display volgt het omgevingslicht, logaritmisch — zoals het oog. ' +
    'Geregeld tussen 20 en 100 %. De waarde tussen haakjes is ongefilterd ' +
    'en helpt bij het plaatsen van de sensor.',
  lumTitle: 'Helderheidscurve',
  lumHint:
    'Wat de automaat geleerd heeft en wat ze eruit afleidt. Alleen lezen — ' +
    'herstellen doe je op het tabblad kleur.',
  lumApplied: 'Weergegeven',
  lumLine: 'Lijn',
  lumSlope: 'Helling',
  lumSlopeFitted: 'uit de punten berekend',
  lumSlopeKept: 'behouden, punten liggen te dicht bijeen',
  lumAnchor: 'Helling en hoogte komen allebei uit alle punten (kleinste kwadraten), even zwaar — de lijn gaat daarom meestal door geen enkel punt. Dat is de bedoeling: meerdere metingen moeten je eigen schattingsfout uitmiddelen.',
  lumNewest: 'nieuwste punt — het oudste valt af zodra er een elfde bij komt',
  lumCensored: 'aan de bovengrens — telt niet mee voor de lijn',
  lumCensoredHint: 'Punten aan de bovengrens zijn hol getekend en tellen niet mee: boven het maximum viel niets te vragen, dus zo’n punt betekent “minstens zoveel” — als gelijkheid gelezen zou het de lijn vlakker maken.',
  lumTaughtIn: (hue, sat) => `geleerd bij tint ${hue}°, verzadiging ${sat} %`,
  lumAdjusting: (want, secs) => `${want} % ingesteld — binnen ${secs} s geleerd`,
  lumPoints: 'Geleerde punten',
  lumWanted: 'gewenst',
  lumCurve: 'lijn',
  lumWhen: 'bedrijfstijd',
  lumEmpty: 'Nog niets geleerd — de standaardlijn geldt.',
  lumRange: 'Regelbereik',
  lumRangeHint: 'Hoe donker en hoe helder de automaat mag gaan. Daarbuiten blijft de curve vlak — de twee stippellijnen in de grafiek. Alleen de weergavemodus zet het vlak uit, nooit de sensor.',
  lumRangeMin: 'donkerst',
  lumRangeMax: 'helderst',
  lumReadOnly: 'Alleen lezen. Ontgrendel de klok in expertmodus om te bewerken.',
  lumCoupling: 'Eigen licht',
  lumCoupledCells: (n) => `${n} cellen gemeten`,
  lumCoupledNone: 'Niet gemeten — de klok trekt haar eigen licht er niet af.',
  lumDisplayShare: (lx, raw) => `${lx} lx van ${raw} lx ruw is het display zelf`,
  lumForget: 'Vergeten',
  lumForgetTitle: 'Dit punt vergeten',
  lumResetPoints: 'Alle punten vergeten',
  lumResetCoupling: 'Meting van eigen licht wissen',
  lumResetCouplingHint: 'De klok regelt dan weer op de ruwe meting — met de terugkoppeling van haar eigen display.',
  lumCalibrate: 'Eigen licht zelf meten',
  lumCalibrateHint: (max) => `De klok tast elke cel afzonderlijk af, ongeveer 90 s. Het moet donker zijn — dek de klok af of verduister de kamer; boven ${max} lx geeft ze het op.`,
  lumCalibrateAbort: 'Afbreken',
  lumCalibratePhases: ['gereed', 'omgevingslicht controleren', 'gevoeligheid kiezen', 'cellen aftasten', 'kanalen meten', 'aansturingscurve', 'opslaan', 'klaar', 'mislukt'],
  lumCalibrateResult: (cells, rung) => `${cells} cellen gemeten, trede ${rung}`,
  lumCalibrateAmbient: (lx) => `omgeving ${lx} lx`,

  // --- the colour-aware factory model ---
  lumSurfaceTitle: 'Fabriekscurve naar kleur',
  lumSurfaceHint:
    'Wat de klok wil instellen bij elk lichtniveau en elke kleur. Procenten ' +
    'zijn geen licht: dezelfde stand geeft in diepblauw ongeveer een tiende ' +
    'van het licht van het groen dat deze klok toont. De kleurtoon loopt ' +
    'helemaal rond, want hij heeft geen eerste en geen laatste waarde en een ' +
    'rechte as zou hem ergens moeten doorsnijden. De ring is het punt van nu.',
  lumSurfaceNone:
    'Geen fabrieksprofiel geladen — de automatiek regelt op de geleerde ' +
    'witte curve, zoals vóór deze meting.',
  lumSurfaceSummary: (lowLux, highLux, low, high) =>
    `Een vlak van ${lowLux} tot ${highLux} lx over alle kleurtonen; de curve ` +
    `vraagt tussen ${low} en ${high} %.`,
  lumSurfaceHere: 'punt van nu',
  lumSurfaceTaught: 'met de hand geleerd',
  lumSurfaceLimited: 'kleur op de aanslag — hoger kan niet',
  lumSurfaceBound: 'de meting zei “minstens zoveel”',
  lumSurfaceRadius:
    'Hoek = kleurtoon, straal = omgevingslicht (logaritmisch, naar buiten toe ' +
    'helderder), hoogte = de ingestelde helderheid in procent.',
  lumSurfaceRotate:
    'Sleep om te draaien. Met de pijltoetsen draai en kantel je de weergave, ' +
    'met Home zet je haar terug.',
  lumSurfaceControls: 'Weergave draaien',
  lumSurfaceLeft: 'Naar links draaien',
  lumSurfaceRight: 'Naar rechts draaien',
  lumSurfaceReset: 'Weergave herstellen',
  lumSurfaceView: (azimuth, tilt) => `Draaiing ${azimuth}°, kanteling ${tilt}°`,
  lumFactory: 'Fabrieksprofiel',
  lumFactoryNone: 'Geen geladen',
  lumFactoryStack: (stack) => `Optiek: ${stack}`,
  lumFactorySource: {
    legacy: 'geleerde witte curve',
    factory: 'fabrieksprofiel',
    'factory+user': 'fabrieksprofiel plus je eigen correcties'
  },
  lumFactoryTarget: (percent, factory) =>
    `${percent} % gewild, het fabrieksprofiel alleen zegt ${factory} %`,
  lumFactoryAccuracy: (max, hue) =>
    `Slechtste fout bij kruisvalidatie: ${max} procentpunten, bij kleurtoon ` +
    `${hue}°. Het doel van 10 is niet gehaald — het verschil zit in de ` +
    `metingen van die ene kleurtoon, niet in de vorm van het model.`,
  lumFactoryAccuracyMet: 'Kruisvalidatie gehaald.',
  lumFactoryObservations:
    'De metingen eronder spreken elkaar op plaatsen tegen; de gefitte curve ' +
    'stijgt toch overal met het licht.',
  lumFactoryMismatch:
    'De opgeslagen correcties zijn op een ander profiel geleerd en worden ' +
    'niet toegepast. “Fabriek herstellen” ruimt ze op.',
  lumGeekTitle: 'Fitparameters (voor techneuten)',
  lumGeekCone: 'Kegel (helling / afsnede)',
  lumGeekNose: 'Kleurneus (a0 / a1 / b1)',
  lumGeekBlue: 'Blauws eigen lijn (helling / afsnede)',
  lumGeekFactory: 'fabriek',
  lumGeekLearned: 'geleerd',
  lumGeekWhite: 'zuivere witte curve (zonder kleur, ter vergelijking)',
  lumGeekAccuracy: 'Kruisvalidatie',
  lumGeekHint:
    'Alles in decaden licht. De kleurneus is één golf over alle kleurtonen ' +
    'behalve blauw — a0 + a1·cos(kleurtoon) + b1·sin(kleurtoon) —, en ' +
    'blauw heeft zijn eigen lijn. “Geleerd” verschijnt zodra er minstens ' +
    'één kleurcorrectie in is verwerkt. De witte curve stuurt de klok niet ' +
    'meer aan zodra er een fabrieksprofiel geladen is — hier staat hij ' +
    'alleen ter vergelijking.',
  lumResiduals: 'Je eigen correcties',
  lumResidualsHint:
    'Wat de automatiek van je bijstellen heeft geleerd — als afwijking van ' +
    'het fabrieksprofiel, in decaden licht, per kleur apart gehouden. Twee ' +
    'correcties bij hetzelfde licht in verschillende kleuren vervangen ' +
    'elkaar niet. Bewust minder plekken dan bij de witte punten hierboven: ' +
    'het fabrieksprofiel brengt de vorm van de curve al mee, er blijft dus ' +
    'alleen een niveau en een beetje kleursmaak te leren over — en één ' +
    'correctie verschuift de curve vaak maar weinig, tenzij hij dicht ' +
    'genoeg bij een fabrieksmeting ligt om die te vervangen.',
  lumResidualsEmpty: 'Nog niets bijgesteld.',
  lumResidualDecades: 'Afwijking',
  lumFactoryRestore: 'Fabriek herstellen',
  lumFactoryRestoreHint:
    'Wist je correcties en de geleerde witte curve. De eigenlichtmeting ' +
    'blijft: die hoort bij de optiek van deze klok, niet bij een smaak.',

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

  // --- debug tab ---
  clockState: 'Status',
  uptime: 'Bedrijfstijd',
  lastReset: 'Laatste herstart',
  resetReasons: {
    'power-on': 'Inschakelen',
    external: 'Resetlijn',
    software: 'Software (herstart gevraagd)',
    panic: 'Crash (panic)',
    'watchdog-int': 'Watchdog (interrupt)',
    'watchdog-task': 'Watchdog (taak)',
    watchdog: 'Watchdog',
    brownout: 'Spanningsdip',
    sdio: 'SDIO',
    usb: 'Reset via USB',
    jtag: 'Reset via JTAG',
    efuse: 'eFuse-fout',
    'power-glitch': 'Spanningspiek',
    'cpu-lockup': 'CPU-blokkade (dubbele fout)',
    'deep-sleep': 'Diepe slaap',
    unknown: 'onbekend'
  },
  heapFree: 'Vrij geheugen',
  heapMin: 'Laagste sinds start',
  heapBlock: 'Grootste vrije blok',
  heapHint:
    'Wordt een update geweigerd met „firmware kon niet worden geactiveerd”, ' +
    'dan is dit minimum het eerste getal om naar te kijken.',
  logTitle: 'Logboek',
  logPause: 'Pauzeren',
  logResume: 'Hervatten',
  logClear: 'Venster leegmaken',
  logMissed: (count) =>
    `${count} regels zijn uit het geheugen van de klok gelopen voordat ze hier aankwamen.`,
  logEmpty: 'Nog niets gelogd.',
  logHint:
    'De klok bewaart de laatste 200 regels in het werkgeheugen, ook die van ' +
    'het opstarten. Wat de bootloader vóór de firmware toont, staat alleen ' +
    'op de USB-kabel.',

  // --- storage tab: the filesystem and NVS, one explorer for both ---
  storageTitle: 'Opslag',
  storageFs: 'LittleFS',
  storageNvs: 'NVS',
  storageFsHint:
    'Het bestandssysteem van de klok — dezelfde partitie waar deze pagina ' +
    'vandaan komt. Een update van de webinterface overschrijft haar geheel.',
  storageFsWarn:
    'Wie index.html wist, bereikt de klok nog alleen via de API en heeft ' +
    'de USB-kabel nodig.',
  storageNvsHint:
    'Geen bestandssysteem maar sleutels en waarden: naamruimten als ' +
    'mappen, sleutels als bestanden. De extensie is een lezing van de ' +
    'inhoud, geen opgeslagen naam. Een update laat NVS ongemoeid — daarom ' +
    'staan instellingen, wachtwoord en helderheidscurve hier.',
  storageNvsWarn:
    'De klok houdt haar instellingen in het werkgeheugen en schrijft ze ' +
    'bij de volgende wijziging terug. Een bewerking hier overleeft alleen ' +
    'een onmiddellijke herstart.',
  fsUsageEntries: (used, total) => `${used} van ${total} items in gebruik`,
  fsKeys: (n) => `${n} sleutels`,
  fsRestart: 'Nu herstarten',
  fsRestarting: 'Herstart …',
  fsConfirmRestart: 'De klok nu herstarten? Ze is een paar seconden niet bereikbaar.',
  fsRestarted: 'Herstart aangevraagd — laad de pagina over een paar seconden opnieuw.',
  fsGesture: 'Rechtermuisknop, of lang indrukken, opent het menu.',
  fsNewFolderHere: 'Nieuwe map hierin',
  fsProtected: 'Niet leesbaar (wachtwoord-hash)',
  fsCompact: 'compact opslaan',
  fsNotJson: 'geen geldige JSON',

  // --- the explorer itself ---
  fsUsage: (used, total) => `${used} van ${total} in gebruik`,
  fsRoot: 'Hoofdmap',
  fsEmpty: 'Deze map is leeg.',
  fsTruncated: 'Alleen de eerste items — de map bevat er meer.',
  fsDownload: 'Downloaden',
  fsEdit: 'Bewerken',
  fsDelete: 'Verwijderen',
  fsUpload: 'Uploaden',
  fsNewFolder: 'Nieuwe map',
  fsFolderName: 'Naam van de map',
  fsSave: 'Opslaan',
  fsCancel: 'Annuleren',
  fsSaved: 'Opgeslagen.',
  fsConfirmDelete: (name) => `„${name}” definitief verwijderen?`,
  fsUploading: (percent) => `Bezig met overdragen … ${percent} %`,
  fsTooLarge: 'Te groot voor de editor — download het bestand.',
  fsBinary: 'Geen tekst — alleen downloaden.',

  // --- expert mode ---
  expertTitle: 'Expertmodus',
  expertUnlocked:
    'De expertmodus staat aan. De tabbladen Update en Debug zijn bereikbaar ' +
    '— voor iedereen op hetzelfde netwerk.',
  expertLock: 'Weer vergrendelen',
  expertEnterHint:
    'Voer het wachtwoord in om de tabbladen Update en Debug te openen. De ' +
    'ontgrendeling blijft na een herstart bestaan.',
  expertSetHint:
    'Op deze klok is nog geen wachtwoord ingesteld. Het eerste dat hier ' +
    'wordt opgegeven geldt; tot dan blijven Update en Debug vergrendeld.',
  expertPassword: 'Wachtwoord',
  expertUnlock: 'Ontgrendelen',
  expertSet: 'Wachtwoord instellen',
  expertMinLength: (count) => `Minstens ${count} tekens.`,
  expertLockedOut:
    'Te veel mislukte pogingen. De klok neemt een paar minuten geen ' +
    'wachtwoord aan.',
  expertForgotten: 'Wachtwoord vergeten?',
  expertResetHint: (time) =>
    `Kort na het inschakelen aan de stekker kan het wachtwoord worden gewist. Nog ${time}. Daarna helpt alleen de stroom eraf — of de USB-kabel.`,
  expertReset: 'Wachtwoord wissen',

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
  err_calibrationTooClose: 'De twee punten liggen te dicht bij elkaar',
  err_calibrationRange: 'Helderheid buiten het geldige bereik',
  err_lumNoSuchPoint: 'Dat punt bestaat niet meer — waarschijnlijk elders al verwijderd',
  err_lumRange: 'Ongeldig bereik — 1 tot 100 %, en minstens 5 % ertussen',
  err_calibTooBright: 'Te licht om te meten — dek de klok af of verduister de kamer',
  err_calibBusy: 'Er loopt al een meting',
  err_calibLabActive: 'Het lab houdt de leds bezet',
  err_calibNoSensor: 'Deze klok heeft geen lichtsensor',
  err_calibSaturated: 'De sensor loopt op elke gevoeligheidstrede vol',
  err_calibNoCoupling: 'Geen enkele cel bereikt de sensor',
  err_calibStore: 'De meting kon niet worden opgeslagen',
  err_calibCancelled: 'Afgebroken',
  err_calibNoTask: 'Te weinig geheugen voor de meting',
  err_labCalibrating: 'De klok meet haar eigen licht',
  err_couplingInvalid: 'De eigenlichtmeting is onleesbaar',
  err_hostnameInvalid: 'Deze naam bevat geen bruikbare tekens',
  err_wifiConnect: (ssid) => `Verbinden met “${ssid}” is mislukt`,
  err_wifiFallback: (ssid) => `Terugvallen op “${ssid}” is ook mislukt`,

  err_languageNotOnPanel: 'Deze taal past niet bij het letterpaneel van de klok — aan te passen in de expertmodus',
  err_expertLocked: 'De expertmodus is vergrendeld',
  err_expertLockedOut: 'Te veel pogingen — probeer het later opnieuw',
  err_expertWrongPassword: 'Verkeerd wachtwoord',
  err_expertPasswordShort: 'Dat wachtwoord is te kort',
  err_expertNoGrace: 'Het venster om het wachtwoord te wissen is gesloten',

  err_fsPath: 'Ongeldig pad',
  err_fsBody: 'Verzoek onleesbaar',
  err_fsNotFound: 'Niet gevonden',
  err_fsNotDir: 'Dat is geen map',
  err_fsIsDir: 'Dat is een map',
  err_fsOpen: 'Het bestand kon niet worden aangemaakt',
  err_fsWrite: 'Schrijffout — het bestandssysteem is waarschijnlijk vol',
  err_fsRename: 'Het bestand kon niet op zijn plaats worden gezet',
  err_fsAborted: 'Overdracht afgebroken',
  err_fsTooBig: 'Te groot voor de editor',
  err_fsNotEmpty: 'De map is niet leeg',
  err_fsExists: 'Bestaat al',
  err_fsDelete: 'Verwijderen mislukt',
  err_fsMkdir: 'De map kon niet worden aangemaakt',

  err_nvsPath: 'Naamruimte of sleutel ontbreekt',
  err_nvsBody: 'Verzoek onleesbaar',
  err_nvsNamespace: 'Naamruimte niet gevonden',
  err_nvsNotFound: 'Sleutel niet gevonden',
  err_nvsProtected: 'Deze waarde wordt niet afgegeven',
  err_nvsBinary: 'Binaire waarde — alleen downloaden',
  err_nvsTooBig: 'Te groot voor de editor',
  err_nvsNotANumber: 'Deze sleutel bevat een getal',
  err_nvsRead: 'De waarde kon niet worden gelezen',
  err_nvsWrite: 'De waarde kon niet worden geschreven',
  err_nvsDelete: 'Verwijderen mislukt',
  err_nvsMemory: 'Te weinig geheugen',
  err_nvsNamespaceDelete: 'Een naamruimte verdwijnt met haar laatste sleutel',
  err_factoryMissing: 'Geen fabrieksprofiel in het bestandssysteem',
  err_factoryLayout: 'Het fabrieksprofiel heeft niet de verwachte opbouw',
  err_factoryChecksum: 'Het fabrieksprofiel klopt niet met zijn eigen controlesom',
  err_factorySchema: 'Onbekend schema van het fabrieksprofiel',
  err_factoryModel: 'Het fabrieksprofiel beschrijft een ander model',
  err_factoryShape: 'Fabrieksprofiel onvolledig of onjuist',
  err_factoryNotMonotone: 'Het fabrieksraster daalt terwijl het licht stijgt',
  err_factoryTooBig: 'Het fabrieksprofiel heeft niet de grootte van een profiel',
  err_factoryUnreadable: 'Fabrieksprofiel niet leesbaar',
  err_factoryUnavailable: 'Geen geldig fabrieksprofiel om naar terug te zetten',

  // --- api errors ---
  connectionLost: 'Verbinding met de klok verbroken',
  uploadAborted: 'Upload afgebroken'
};
