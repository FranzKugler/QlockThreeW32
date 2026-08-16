/**
 * fr
 * French texts of the configuration UI. Keys follow locales/de.js.
 *
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.0
 * @created  15.8.2026
 * @updated  15.8.2026
 */
export default {
  // --- shell ---
  tabs: ['Affichage', 'Couleur', 'Fuseau horaire', 'WiFi', 'Mise à jour'],
  loading: 'Chargement des réglages …',
  loadingShort: 'chargement …',
  clockUnreachable: 'Horloge injoignable',
  retry: 'Réessayer',
  writeFailed: 'Échec de la transmission',

  // --- display tab ---
  displayTitle: 'Affichage',
  modes: [
    'Heure',
    'Heure avec état WiFi',
    'Éteint (sombre)',
    'Secondes',
    'Test',
    'État'
  ],
  appearance: 'Présentation',
  language: 'Langue',
  languages: [
    'Allemand',
    'Souabe',
    'Bavarois',
    'Saxon',
    'Suisse allemand',
    'Anglais',
    'Français',
    'Italien',
    'Néerlandais',
    'Espagnol'
  ],
  corners: 'Coins',
  clockwise: 'sens horaire',
  counterClockwise: 'sens antihoraire',
  minutes: 'Minutes',
  monochrome: 'monochrome',
  colored: 'en couleur',

  // --- colour tab ---
  colorTitle: 'Couleur',
  hue: 'Teinte',
  saturation: 'Saturation',
  brightness: 'Luminosité',
  preview: ['IL EST', 'DEUX HEURES', 'CINQ'],
  decreaseBy: (label, step) => `Diminuer ${label} de ${step}`,
  increaseBy: (label, step) => `Augmenter ${label} de ${step}`,
  automatic: 'Automatique',
  autoBrightness: 'Luminosité automatique',
  ldrHint:
    "Sans effet tant que l'évaluation du LDR est commentée dans le " +
    'micrologiciel (voir src/LDR.cpp).',

  // --- timezone tab ---
  timeServer: 'Serveur de temps',
  ntpServer: 'Serveur NTP',
  timezoneTitle: 'Fuseau horaire',
  dst: "Heure d'été",
  standardTime: 'Heure normale',
  noDstHint:
    "Sans heure d'été, le décalage de l'heure normale s'applique toute " +
    "l'année ; les moments de changement ne sont pas pris en compte.",
  abbreviation: 'Abréviation',
  week: 'Semaine',
  weeks: ['Dernier', '1er', '2e', '3e', '4e'],
  day: 'Jour',
  days: ['dim.', 'lun.', 'mar.', 'mer.', 'jeu.', 'ven.', 'sam.'],
  month: 'Mois',
  months: [
    'janv.', 'févr.', 'mars', 'avr.', 'mai', 'juin',
    'juil.', 'août', 'sept.', 'oct.', 'nov.', 'déc.'
  ],
  hour: 'Heure',
  offsetMin: 'Décalage (min)',

  // --- wifi tab ---
  connection: 'Connexion',
  network: 'Réseau',
  address: 'Adresse',
  signal: 'Signal',
  hostname: "Nom d'hôte",
  mac: 'MAC',
  quality: ['faible', 'moyen', 'bon', 'très bon'],
  statusUnavailable: 'État indisponible',
  availableNetworks: 'Réseaux disponibles',
  scanning: 'Recherche en cours …',
  noNetworks: 'Aucun réseau trouvé.',
  rescan: 'Rechercher à nouveau',
  encrypted: 'chiffré',
  switchNetwork: 'Changer de réseau',
  password: 'Mot de passe',
  passwordPlaceholder: 'laisser vide pour les réseaux ouverts',
  connect: 'Se connecter',
  connecting: 'Connexion …',
  connectedTo: (ssid) => `Connecté à ${ssid}.`,
  noResponse:
    "Pas de réponse. Si l'horloge est passée sur un autre réseau, elle n'est " +
    'plus joignable à cette adresse.',
  wifiHint: (host) =>
    "L'horloge quitte brièvement le réseau. Si la connexion échoue, elle " +
    "revient d'elle-même au réseau précédent. Après un changement, l'adresse " +
    `peut changer — elle est alors accessible à ${host}.local.`,

  // --- update tab ---
  installed: 'Installé',
  firmware: 'Micrologiciel',
  webUi: 'Interface web',
  used: 'Occupé',
  roomForUpdate: 'Place pour la mise à jour',
  unknown: 'inconnu',
  uploadImage: 'Envoyer une image',
  file: 'Fichier',
  detectedAs: 'Reconnu comme',
  firmwareImage: 'Image du micrologiciel',
  filesystemImage: 'Image du système de fichiers (interface web)',
  filesystemHint:
    "L'image du système de fichiers écrase toute la partition. Les réglages " +
    'sont conservés — ils se trouvent dans la NVS, une partition distincte ' +
    "qu'une mise à jour ne touche pas.",
  writing: (percent) => `Écriture … ${percent} %`,
  rebooting: "L'horloge redémarre — veuillez patienter.",
  updateDone: (firmware, webUi) =>
    `Mise à jour installée. Micrologiciel ${firmware}, interface web ${webUi}.`,
  upload: 'Envoyer et redémarrer',
  running: 'En cours …',
  noResponseAfterReboot:
    "L'horloge ne s'est pas manifestée après le redémarrage. Avec une image " +
    "défectueuse, elle repart sur la version précédente.",
  buildHint:
    'firmware.bin et littlefs.bin sont produits par « pio run » et ' +
    '« pio run -t buildfs » dans .pio/build/seeed_xiao_esp32s3/. ' +
    "L'horloge vérifie la somme de contrôle avant de basculer sur la nouvelle " +
    'image ; un envoi interrompu est donc sans danger, elle redémarre ' +
    'simplement sur la version précédente.',

  // --- update channel ---
  updateSource: 'Mises a jour',
  channel: 'Canal',
  channelStable: 'Stable (versions testees)',
  channelEdge: 'Developpement (chaque build)',
  autoUpdate: 'Installer automatiquement',
  autoUpdateHint:
    "Installe la nuit entre 2 h et 5 h, pour que l'horloge ne s'eteigne pas " +
    'le soir. Desactive par defaut : une image defectueuse ne se recupere ' +
    "qu'avec un cable USB.",
  checkInterval: 'Verifier toutes les',
  checkNever: 'jamais',
  hours: (count) => `${count} h`,
  checkNow: 'Verifier maintenant',
  checking: 'Verification ...',
  available: 'Disponible',
  upToDate: "L'horloge est a jour.",
  neverChecked: 'pas encore verifie',
  lastChecked: (text) => `Derniere verification : ${text}`,
  justNow: "a l'instant",
  minutesAgo: (count) => `il y a ${count} min`,
  hoursAgo: (count) => `il y a ${count} h`,
  installNow: 'Mettre a jour',
  downloading: (percent) => `Telechargement ... ${percent} %`,
  runningFrom: 'Emplacement actif',

  // --- api errors ---
  connectionLost: "Connexion à l'horloge interrompue",
  uploadAborted: 'Envoi interrompu'
};
