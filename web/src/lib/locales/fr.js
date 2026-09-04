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
  tabs: ['Affichage', 'Couleur', 'Fuseau horaire', 'WiFi', 'Mise à jour', 'Luminosité', 'Débogage', 'Stockage'],
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
  decreaseBy: (label, step) => `Diminuer ${label} de ${step}`,
  increaseBy: (label, step) => `Augmenter ${label} de ${step}`,
  automatic: 'Automatique',
  autoBrightness: 'Luminosité automatique',
  measured: 'Mesuré',
  calibration: 'Étalonnage',
  calTaught: (n) => `${n} points appris`,
  calReset: 'Réinitialiser',
  calHint:
    'En mode automatique, le curseur de luminosité signifie « avec cet ' +
    'éclairage, je la veux comme ça ». Dix secondes après le dernier ' +
    'changement, l’horloge retient le couple et recalcule sa droite sur ' +
    'tout ce qu’elle a appris. Rien à valider.',
  ldrHint:
    'L’affichage suit la lumière ambiante, de façon logarithmique — comme ' +
    'l’œil. Régulé entre 20 et 100 %. La valeur entre parenthèses est ' +
    'brute et aide à placer le capteur.',
  lumTitle: 'Courbe de luminosité',
  lumHint:
    'Ce que l’automatisme a appris et ce qu’il en déduit. En lecture seule ' +
    '— la réinitialisation est dans l’onglet couleur.',
  lumApplied: 'Affiché',
  lumLine: 'Droite',
  lumSlope: 'Pente',
  lumSlopeFitted: 'calculée à partir des points',
  lumSlopeKept: 'conservée, points trop rapprochés',
  lumAnchor: 'La pente et le niveau viennent de tous les points (moindres carrés), tous du même poids — la droite ne passe donc en général par aucun d’eux. C’est voulu : plusieurs mesures doivent moyenner votre propre erreur d’appréciation.',
  lumNewest: 'point le plus récent — le plus ancien sort dès qu’un onzième arrive',
  lumCensored: 'à la butée haute — non compté dans l’ajustement',
  lumCensoredHint: 'Les points à la butée haute sont dessinés creux et exclus de l’ajustement : rien au-dessus du maximum ne pouvait être demandé, donc un tel point signifie « au moins autant » — pris pour une égalité, il aplatirait la droite.',
  lumTaughtIn: (hue, sat) => `appris en teinte ${hue}°, saturation ${sat} %`,
  lumAdjusting: (want, secs) => `${want} % réglé — appris d’ici ${secs} s`,
  lumPoints: 'Points appris',
  lumWanted: 'voulu',
  lumCurve: 'droite',
  lumWhen: 'durée',
  lumEmpty: 'Rien d’appris pour l’instant — la droite par défaut s’applique.',
  lumRange: 'Plage de régulation',
  lumRangeHint: 'Jusqu’où l’automatisme peut descendre et monter. Au-delà, la courbe reste plate — les deux lignes pointillées du graphique. Seul le mode d’affichage éteint la façade, jamais le capteur.',
  lumRangeMin: 'le plus sombre',
  lumRangeMax: 'le plus clair',
  lumReadOnly: 'Lecture seule. Déverrouillez l’horloge en mode expert pour modifier.',
  lumCoupling: 'Lumière propre',
  lumCoupledCells: (n) => `${n} cellules mesurées`,
  lumCoupledNone: 'Non mesurée — l’horloge ne soustrait pas sa propre lumière.',
  lumDisplayShare: (lx, raw) => `${lx} lx sur ${raw} lx bruts viennent de l’affichage`,
  lumForget: 'Oublier',
  lumForgetTitle: 'Oublier ce point',
  lumResetPoints: 'Oublier tous les points',
  lumResetCoupling: 'Supprimer la mesure de lumière propre',
  lumResetCouplingHint: 'L’horloge régulera de nouveau sur la mesure brute — avec la rétroaction de son propre affichage.',
  lumCalibrate: 'Mesurer la lumière propre',
  lumCalibrateHint: (max) => `L’horloge balaie chaque cellule une à une, environ 90 s. Il doit faire noir — couvrez l’horloge ou obscurcissez la pièce ; au-dessus de ${max} lx elle abandonne.`,
  lumCalibrateAbort: 'Annuler',
  lumCalibratePhases: ['prêt', 'contrôle de la lumière ambiante', 'choix de la sensibilité', 'balayage des cellules', 'mesure des canaux', 'courbe de commande', 'enregistrement', 'terminé', 'échec'],
  lumCalibrateResult: (cells, rung) => `${cells} cellules mesurées, échelon ${rung}`,
  lumCalibrateAmbient: (lx) => `ambiant ${lx} lx`,

  // --- the colour-aware factory model ---
  lumSurfaceTitle: 'Courbe d’usine selon la couleur',
  lumSurfaceHint:
    'Ce que l’horloge veut régler à chaque niveau de lumière et pour chaque ' +
    'couleur. Le pourcentage n’est pas de la lumière : le même réglage émet ' +
    'environ dix fois moins en bleu profond qu’au vert de cette horloge. ' +
    'La teinte fait le tour complet, car elle n’a ni début ni fin et un axe ' +
    'droit devrait la couper quelque part. L’anneau indique le point actuel.',
  lumSurfaceNone:
    'Aucun profil d’usine chargé — l’automatique suit la courbe blanche ' +
    'apprise, comme avant cette mesure.',
  lumSurfaceSummary: (lowLux, highLux, low, high) =>
    `Une surface de ${lowLux} à ${highLux} lx sur toutes les teintes ; la ` +
    `courbe demande entre ${low} et ${high} %.`,
  lumSurfaceHere: 'point actuel',
  lumSurfaceTaught: 'appris à la main',
  lumSurfaceLimited: 'couleur en butée — impossible d’aller plus haut',
  lumSurfaceBound: 'la mesure disait « au moins autant »',
  lumSurfaceRadius:
    'Angle = teinte, rayon = lumière ambiante (échelle logarithmique, plus ' +
    'clair vers l’extérieur), hauteur = la luminosité réglée, en pour cent.',
  lumSurfaceRotate:
    'Faites glisser pour tourner. Les flèches tournent et inclinent la vue, ' +
    'la touche Origine la remet en place.',
  lumSurfaceControls: 'Tourner la vue',
  lumSurfaceLeft: 'Tourner à gauche',
  lumSurfaceRight: 'Tourner à droite',
  lumSurfaceReset: 'Réinitialiser la vue',
  lumSurfaceView: (azimuth, tilt) => `Rotation ${azimuth}°, inclinaison ${tilt}°`,
  lumFactory: 'Profil d’usine',
  lumFactoryNone: 'Aucun chargé',
  lumFactoryStack: (stack) => `Optique : ${stack}`,
  lumFactorySource: {
    legacy: 'courbe blanche apprise',
    factory: 'profil d’usine',
    'factory+user': 'profil d’usine et vos corrections'
  },
  lumFactoryTarget: (percent, factory) =>
    `${percent} % voulus, le profil d’usine seul dit ${factory} %`,
  lumFactoryAccuracy: (max, hue) =>
    `Pire erreur en validation croisée : ${max} points de pourcentage, à la ` +
    `teinte ${hue}°. L’objectif de 10 n’est pas atteint — l’écart vient des ` +
    `mesures de cette teinte, pas de la forme du modèle.`,
  lumFactoryAccuracyMet: 'Validation croisée réussie.',
  lumFactoryObservations:
    'Les mesures sous-jacentes se contredisent par endroits ; la courbe ' +
    'ajustée monte tout de même partout avec la lumière.',
  lumFactoryMismatch:
    'Les corrections enregistrées ont été apprises sur un autre profil et ne ' +
    'sont pas appliquées. « Restaurer l’usine » les efface.',
  lumGeekTitle: 'Paramètres de l’ajustement (pour les curieux)',
  lumGeekCone: 'Cône (pente / ordonnée à l’origine)',
  lumGeekNose: 'Nez de couleur (a0 / a1 / b1)',
  lumGeekBlue: 'Droite propre au bleu (pente / ordonnée à l’origine)',
  lumGeekFactory: 'usine',
  lumGeekLearned: 'appris',
  lumGeekWhite: 'courbe blanche pure (sans couleur, pour comparer)',
  lumGeekAccuracy: 'Validation croisée',
  lumGeekHint:
    'Tout en décades de lumière. Le nez de couleur est une seule onde sur ' +
    'toutes les teintes sauf le bleu — a0 + a1·cos(teinte) + b1·sin(teinte) ' +
    '—, et le bleu a sa propre droite. « Appris » apparaît dès qu’au moins ' +
    'une correction de couleur y est entrée. La courbe blanche ne pilote ' +
    'plus l’horloge une fois un profil d’usine chargé — elle n’est ' +
    'affichée ici qu’à titre de comparaison.',
  lumResiduals: 'Vos corrections',
  lumResidualsHint:
    'Ce que l’automatique a appris de vos réglages — en écart au profil ' +
    'd’usine, en décades de lumière, séparément par couleur. Deux ' +
    'corrections à la même lumière dans des couleurs différentes ne se ' +
    'remplacent pas. Volontairement moins de places que pour les points ' +
    'blancs ci-dessus : le profil d’usine apporte déjà la forme de la ' +
    'courbe, il ne reste à apprendre qu’un niveau et un peu de goût pour ' +
    'la couleur — et une correction isolée ne déplace souvent la courbe ' +
    'que faiblement, sauf si elle tombe assez près d’une mesure d’usine ' +
    'pour la remplacer.',
  lumResidualsEmpty: 'Rien de corrigé pour l’instant.',
  lumResidualDecades: 'Écart',
  lumFactoryRestore: 'Restaurer l’usine',
  lumFactoryRestoreHint:
    'Efface vos corrections et la courbe blanche apprise. La mesure de la ' +
    'lumière propre reste : elle appartient à l’optique de cette horloge, ' +
    'pas à un goût.',

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

  // --- portail de configuration ---
  portalTitle: 'Configurer le réseau',
  portalIntro: (apName) =>
    `Cette horloge n'est pas encore sur un réseau et a ouvert son propre ` +
    `point d'accès, « ${apName} ». Choisissez votre WiFi ci-dessous et ` +
    `saisissez son mot de passe.`,
  portalConnected: (ssid, ip) => `Connectée à ${ssid} en tant que ${ip}.`,
  portalConnecting: (ssid) => `Connexion à ${ssid} …`,
  portalDone:
    "L'horloge redémarre et sera bientôt accessible sous son propre nom " +
    'sur le réseau.',

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

  // --- debug tab ---
  clockState: 'État',
  uptime: 'Durée de fonctionnement',
  lastReset: 'Dernier redémarrage',
  resetReasons: {
    'power-on': 'Mise sous tension',
    external: 'Ligne de reset',
    software: 'Logiciel (redémarrage demandé)',
    panic: 'Plantage (panic)',
    'watchdog-int': 'Chien de garde (interruption)',
    'watchdog-task': 'Chien de garde (tâche)',
    watchdog: 'Chien de garde',
    brownout: 'Chute de tension',
    sdio: 'SDIO',
    usb: 'Réinitialisé par USB',
    jtag: 'Réinitialisé par JTAG',
    efuse: 'Erreur eFuse',
    'power-glitch': 'Pic de tension',
    'cpu-lockup': 'Blocage du CPU (double exception)',
    'deep-sleep': 'Sommeil profond',
    unknown: 'inconnu'
  },
  heapFree: 'Mémoire libre',
  heapMin: 'Minimum depuis le démarrage',
  heapBlock: 'Plus grand bloc libre',
  heapHint:
    "Si une mise à jour est refusée avec « impossible d'activer le " +
    "micrologiciel », ce minimum est le premier chiffre à regarder.",
  logTitle: 'Journal',
  logPause: 'Pause',
  logResume: 'Reprendre',
  logClear: 'Vider la fenêtre',
  logMissed: (count) =>
    `${count} lignes ont quitté la mémoire de l'horloge avant d'arriver ici.`,
  logEmpty: 'Rien encore journalisé.',
  logHint:
    "L'horloge garde les 200 dernières lignes en mémoire, y compris celles " +
    "du démarrage. Ce que le bootloader affiche avant le micrologiciel n'est " +
    'visible que sur le câble USB.',

  // --- storage tab: the filesystem and NVS, one explorer for both ---
  storageTitle: 'Stockage',
  storageFs: 'LittleFS',
  storageNvs: 'NVS',
  storageFsHint:
    'Le système de fichiers de l\'horloge — la partition d\'où vient cette ' +
    'page. Une mise à jour de l\'interface la réécrit entièrement.',
  storageFsWarn:
    'Supprimer index.html rend l\'horloge accessible seulement par l\'API, ' +
    'et il faut le câble USB pour revenir.',
  storageNvsHint:
    'Pas un système de fichiers mais des clés et des valeurs : les ' +
    'espaces de noms en dossiers, les clés en fichiers. L\'extension est ' +
    'une lecture du contenu, pas un nom enregistré. Une mise à jour ne ' +
    'touche pas NVS — d\'où les réglages, le mot de passe et la courbe.',
  storageNvsWarn:
    'L\'horloge garde ses réglages en mémoire vive et les réécrit au ' +
    'changement suivant. Une modification faite ici ne survit qu\'à un ' +
    'redémarrage immédiat.',
  fsUsageEntries: (used, total) => `${used} entrées sur ${total} utilisées`,
  fsKeys: (n) => `${n} clés`,
  fsRestart: 'Redémarrer',
  fsRestarting: 'Redémarrage …',
  fsConfirmRestart: 'Redémarrer l’horloge maintenant ? Elle sera injoignable quelques secondes.',
  fsRestarted: 'Redémarrage demandé — rechargez la page dans quelques secondes.',
  fsGesture: 'Clic droit, ou appui long, pour ouvrir le menu.',
  fsNewFolderHere: 'Nouveau dossier dedans',
  fsProtected: 'Illisible (empreinte du mot de passe)',
  fsCompact: 'enregistrer compact',
  fsNotJson: 'JSON invalide',

  // --- the explorer itself ---
  fsUsage: (used, total) => `${used} sur ${total} utilisés`,
  fsRoot: 'Racine',
  fsEmpty: 'Ce dossier est vide.',
  fsTruncated: 'Seulement les premières entrées — le dossier en contient plus.',
  fsDownload: 'Télécharger',
  fsEdit: 'Modifier',
  fsDelete: 'Supprimer',
  fsUpload: 'Envoyer',
  fsNewFolder: 'Nouveau dossier',
  fsFolderName: 'Nom du dossier',
  fsSave: 'Enregistrer',
  fsCancel: 'Annuler',
  fsSaved: 'Enregistré.',
  fsConfirmDelete: (name) => `Supprimer « ${name} » définitivement ?`,
  fsUploading: (percent) => `Transfert … ${percent} %`,
  fsTooLarge: 'Trop volumineux pour l\'éditeur — téléchargez-le.',
  fsBinary: 'Pas du texte — téléchargement uniquement.',

  // --- expert mode ---
  expertTitle: 'Mode expert',
  expertUnlocked:
    'Le mode expert est actif. Les onglets mise à jour et débogage sont ' +
    'accessibles — à toute personne sur le même réseau.',
  expertLock: 'Verrouiller à nouveau',
  expertEnterHint:
    'Saisissez le mot de passe pour ouvrir les onglets mise à jour et ' +
    'débogage. Le déverrouillage survit aux redémarrages.',
  expertSetHint:
    "Aucun mot de passe n'est encore défini sur cette horloge. Le premier " +
    'saisi ici fera foi ; jusque-là, mise à jour et débogage restent ' +
    'verrouillés.',
  expertPassword: 'Mot de passe',
  expertUnlock: 'Déverrouiller',
  expertSet: 'Définir le mot de passe',
  expertMinLength: (count) => `Au moins ${count} caractères.`,
  expertLockedOut:
    "Trop d'essais infructueux. L'horloge n'accepte plus de mot de passe " +
    'pendant quelques minutes.',
  expertForgotten: 'Mot de passe oublié ?',
  expertResetHint: (time) =>
    `Peu après la mise sous tension, le mot de passe peut être effacé. Il reste ${time}. Ensuite, il faudra couper le courant — ou le câble USB.`,
  expertReset: 'Effacer le mot de passe',

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
  err_calibrationTooClose: 'Les deux points sont trop proches',
  err_calibrationRange: 'Luminosité hors de la plage valide',
  err_lumNoSuchPoint: 'Ce point n’existe plus — sans doute déjà supprimé ailleurs',
  err_lumRange: 'Plage invalide — de 1 à 100 %, avec au moins 5 % d’écart',
  err_calibTooBright: 'Trop clair pour mesurer — couvrez l’horloge ou obscurcissez la pièce',
  err_calibBusy: 'Une mesure est déjà en cours',
  err_calibLabActive: 'Le laboratoire occupe les LED',
  err_calibNoSensor: 'Cette horloge n’a pas de capteur de lumière',
  err_calibSaturated: 'Le capteur sature à tous les échelons de sensibilité',
  err_calibNoCoupling: 'Aucune cellule n’atteint le capteur',
  err_calibStore: 'La mesure n’a pas pu être enregistrée',
  err_calibCancelled: 'Annulé',
  err_calibNoTask: 'Mémoire insuffisante pour la mesure',
  err_labCalibrating: 'L’horloge mesure sa propre lumière',
  err_couplingInvalid: 'La mesure de lumière propre est illisible',
  err_hostnameInvalid: 'Ce nom ne contient aucun caractère utilisable',
  err_wifiConnect: (ssid) => `Impossible de se connecter à « ${ssid} »`,
  err_wifiFallback: (ssid) => `Le retour à « ${ssid} » a également échoué`,
  err_wifiNoSsid: 'Aucun nom de réseau indiqué',
  err_portalBusy: 'Une tentative de connexion est déjà en cours',

  err_languageNotOnPanel: 'Cette langue ne correspond pas au panneau de lettres de l’horloge — modifiable en mode expert',
  err_expertLocked: 'Le mode expert est verrouillé',
  err_expertLockedOut: 'Trop de tentatives — réessayez plus tard',
  err_expertWrongPassword: 'Mot de passe incorrect',
  err_expertPasswordShort: 'Ce mot de passe est trop court',
  err_expertNoGrace: 'La fenêtre pour effacer le mot de passe est fermée',

  err_fsPath: 'Chemin invalide',
  err_fsBody: 'Requête illisible',
  err_fsNotFound: 'Introuvable',
  err_fsNotDir: 'Ce n\'est pas un dossier',
  err_fsIsDir: 'C\'est un dossier',
  err_fsOpen: 'Impossible de créer le fichier',
  err_fsWrite: 'Échec d\'écriture — le système de fichiers est sans doute plein',
  err_fsRename: 'Impossible de mettre le fichier en place',
  err_fsAborted: 'Transfert interrompu',
  err_fsTooBig: 'Trop volumineux pour l\'éditeur',
  err_fsNotEmpty: 'Le dossier n\'est pas vide',
  err_fsExists: 'Existe déjà',
  err_fsDelete: 'Échec de la suppression',
  err_fsMkdir: 'Impossible de créer le dossier',

  err_nvsPath: 'Espace de noms ou clé manquant',
  err_nvsBody: 'Requête illisible',
  err_nvsNamespace: 'Espace de noms introuvable',
  err_nvsNotFound: 'Clé introuvable',
  err_nvsProtected: 'Cette valeur n\'est pas communiquée',
  err_nvsBinary: 'Valeur binaire — téléchargement uniquement',
  err_nvsTooBig: 'Trop volumineux pour l\'éditeur',
  err_nvsNotANumber: 'Cette clé contient un nombre',
  err_nvsRead: 'Impossible de lire la valeur',
  err_nvsWrite: 'Impossible d\'écrire la valeur',
  err_nvsDelete: 'Échec de la suppression',
  err_nvsMemory: 'Mémoire insuffisante',
  err_nvsNamespaceDelete: 'Un espace de noms disparaît avec sa dernière clé',
  err_factoryMissing: 'Aucun profil d’usine dans le système de fichiers',
  err_factoryLayout: 'Le profil d’usine n’a pas la structure attendue',
  err_factoryChecksum: 'Le profil d’usine ne correspond pas à sa somme de contrôle',
  err_factorySchema: 'Schéma de profil d’usine inconnu',
  err_factoryModel: 'Le profil d’usine décrit un autre modèle',
  err_factoryShape: 'Profil d’usine incomplet ou incorrect',
  err_factoryNotMonotone: 'La grille d’usine baisse quand la lumière monte',
  err_factoryTooBig: 'Le profil d’usine n’a pas la taille d’un profil',
  err_factoryUnreadable: 'Profil d’usine illisible',
  err_factoryUnavailable: 'Aucun profil d’usine valide à restaurer',

  // --- api errors ---
  connectionLost: "Connexion à l'horloge interrompue",
  uploadAborted: 'Envoi interrompu'
};
