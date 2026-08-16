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
    'Test'
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
  tzPickerTitle: 'Emplacement',
  tzRegion: 'Région',
  tzPlace: 'Lieu',
  tzChoose: 'à choisir',
  tzCustom: 'réglage manuel',
  tzPickerHint:
    'Le choix remplit les règles ci-dessous ; elles restent modifiables.',
  tzDataVersion: (version) => `Données de fuseaux ${version}.`,
  tzListUnavailable:
    'La liste des fuseaux n’a pas pu être chargée. Les règles ci-dessous ' +
    'restent réglables à la main.',
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
  clockName: 'Nom de l’horloge',
  name: 'Nom',
  saveAndRestart: 'Enregistrer et redémarrer',
  restarting: 'L’horloge redémarre …',
  hostnameSaved: (host) => `Redémarrée. L’horloge répond désormais à ${host}.local.`,
  hostnameHint: () =>
    'Lettres, chiffres et traits d’union uniquement. Le nom sert au mDNS, au ' +
    'routeur, à espota et au point d’accès de configuration — l’horloge ' +
    'l’adopte donc au redémarrage.',
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
  noFile: 'aucun fichier sélectionné',
  chooseFile: 'Choisir un fichier',
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
    "L'horloge vérifie la somme de contrôle avant de basculer sur la nouvelle " +
    'image ; un envoi interrompu est donc sans danger, elle redémarre ' +
    'simplement sur la version précédente.',

  // --- update channel ---
  updateSource: 'Mises à jour',
  channel: 'Canal',
  channelStable: 'Stable (versions testées)',
  channelEdge: 'Développement (chaque build)',
  autoUpdate: 'Installer automatiquement',
  autoUpdateHint:
    "Installe la nuit entre 2 h et 5 h, pour que l'horloge ne s'éteigne pas " +
    'le soir. Désactivé par défaut : une image défectueuse ne se récupère ' +
    "qu'avec un câble USB.",
  checkInterval: 'Vérifier toutes les',
  checkNever: 'jamais',
  hours: (count) => `${count} h`,
  checkNow: 'Vérifier maintenant',
  checking: 'Vérification …',
  available: 'Disponible',
  upToDate: "L'horloge est à jour.",
  neverChecked: 'pas encore vérifié',
  lastChecked: (text) => `Dernière vérification : ${text}`,
  justNow: "à l'instant",
  minutesAgo: (count) => `il y a ${count} min`,
  hoursAgo: (count) => `il y a ${count} h`,
  installNow: 'Mettre à jour',
  downloading: (percent) => `Téléchargement … ${percent} %`,
  runningFrom: 'Emplacement actif',

  // --- error codes reported by the clock (see lib/errors.js) ---
  err_otaBegin: 'Mise à jour refusée',
  err_otaWrite: "Erreur d'écriture pendant le flash",
  err_otaIncomplete: 'Image incomplète',
  err_otaAborted: 'Envoi interrompu',
  err_otaNoImage: 'Aucune image reçue',
  err_otaNoUpdate: 'Aucune mise à jour disponible',
  err_otaBusy: 'Une mise à jour est déjà en cours',
  err_otaServer: 'Serveur de mise à jour injoignable',
  err_otaManifestHttp: 'Manifeste non récupérable',
  err_otaManifestParse: 'Manifeste illisible',
  err_otaDownload: 'Échec du téléchargement',
  err_otaConnectionLost: 'Connexion perdue pendant le téléchargement',
  err_otaSize: 'Téléchargement incomplet',
  err_otaChecksum: 'Somme de contrôle incorrecte — image rejetée',
  err_otaBegin: 'Impossible de démarrer la mise à jour',
  err_otaWrite: 'Échec de l’écriture en flash',
  err_hostnameInvalid: 'Ce nom ne contient aucun caractère utilisable',
  err_wifiConnect: (ssid) => `Impossible de se connecter à « ${ssid} »`,
  err_wifiFallback: (ssid) => `Le retour à « ${ssid} » a également échoué`,

  // --- api errors ---
  connectionLost: "Connexion à l'horloge interrompue",
  uploadAborted: 'Envoi interrompu'
};
