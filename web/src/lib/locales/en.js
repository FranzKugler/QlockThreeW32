/**
 * en
 * English texts of the configuration UI. Keys follow locales/de.js.
 *
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.0
 * @created  15.8.2026
 * @updated  15.8.2026
 */
export default {
  // --- shell ---
  tabs: ['Display', 'Colour', 'Time zone', 'WiFi', 'Update', 'Brightness', 'Debug', 'Storage'],
  loading: 'Loading the settings …',
  loadingShort: 'loading …',
  clockUnreachable: 'Clock not reachable',
  retry: 'Try again',
  writeFailed: 'Could not send the change',

  // --- display tab ---
  displayTitle: 'Display',
  modes: [
    'Time',
    'Time with WiFi status',
    'Off (dark)',
    'Seconds',
    'Test'
  ],
  appearance: 'Appearance',
  language: 'Language',
  corners: 'Corners',
  clockwise: 'clockwise',
  counterClockwise: 'counter-clockwise',
  minutes: 'Minutes',
  monochrome: 'monochrome',
  colored: 'coloured',

  // --- colour tab ---
  colorTitle: 'Colour',
  hue: 'Hue',
  saturation: 'Saturation',
  brightness: 'Brightness',
  decreaseBy: (label, step) => `Decrease ${label} by ${step}`,
  increaseBy: (label, step) => `Increase ${label} by ${step}`,
  automatic: 'Automatic',
  autoBrightness: 'Automatic brightness',
  measured: 'Measured',
  calibration: 'Calibration',
  calTaught: (n) => `${n} points learned`,
  calReset: 'Reset',
  calHint:
    'With the automatic on, the brightness slider means “at this light I ' +
    'would like it this bright”. Ten seconds after the last change the ' +
    'clock keeps the pair and fits a new line through everything it has ' +
    'learned. Nothing to press.',
  ldrHint:
    'The display follows the ambient light, logarithmically — the way the ' +
    'eye does. Regulated between 20 and 100 %. The value in brackets is ' +
    'unsmoothed and helps when placing the sensor.',
  lumTitle: 'Brightness curve',
  lumHint:
    'What the automatic has been taught and what it makes of it. Read-only ' +
    '— resetting is in the colour tab.',
  lumApplied: 'On the face',
  lumLine: 'Line',
  lumSlope: 'Slope',
  lumSlopeFitted: 'fitted from the points',
  lumSlopeKept: 'kept, points sit too close together',
  lumAnchor: 'Slope and level both come from every point (least squares), all weighted the same — so the line generally passes through none of them. That is the point: several measurements are meant to average out your own guessing error.',
  lumNewest: 'newest point — the oldest drops out as soon as an eleventh arrives',
  lumCensored: 'at the ceiling — not counted in the fit',
  lumCensoredHint: 'Points at the ceiling are drawn hollow and left out of the fit: nothing above the maximum could be asked for, so such a point means “at least this much” — read as an equality it would flatten the line.',
  lumTaughtIn: (hue, sat) => `taught at hue ${hue}°, saturation ${sat} %`,
  lumAdjusting: (want, secs) => `${want} % set — learned within ${secs} s`,
  lumPoints: 'Points learned',
  lumWanted: 'wanted',
  lumCurve: 'line',
  lumWhen: 'uptime',
  lumEmpty: 'Nothing learned yet — the default line applies.',
  lumRange: 'Regulated range',
  lumRangeHint: 'How dark and how bright the automatic may go. Beyond either end the curve stays flat — the two dotted lines in the chart. Only the display mode switches the face off, never the sensor.',
  lumRangeMin: 'darkest',
  lumRangeMax: 'brightest',
  lumReadOnly: 'View only. Unlock the clock in expert mode to edit.',
  lumCoupling: 'Own light',
  lumCoupledCells: (n) => `${n} cells measured`,
  lumCoupledNone: 'Not measured — the clock does not subtract its own light.',
  lumDisplayShare: (lx, raw) => `${lx} lx of ${raw} lx raw is the display itself`,
  lumForget: 'Forget',
  lumForgetTitle: 'Forget this point',
  lumResetPoints: 'Forget every point',
  lumResetCoupling: 'Delete the own-light measurement',
  lumResetCouplingHint: 'The clock then regulates on the raw reading again — with the feedback from its own display.',
  lumCalibrate: 'Measure own light',
  lumCalibrateHint: (max) => `The clock scans every cell one at a time, about 90 s. It has to be dark — cover the clock or darken the room; above ${max} lx it gives up.`,
  lumCalibrateAbort: 'Cancel',
  lumCalibratePhases: ['ready', 'checking ambient light', 'choosing sensitivity', 'scanning cells', 'measuring channels', 'drive response', 'storing', 'done', 'failed'],
  lumCalibrateResult: (cells, rung) => `${cells} cells measured, rung ${rung}`,
  lumCalibrateAmbient: (lx) => `ambient ${lx} lx`,

  // --- the colour-aware factory model ---
  lumSurfaceTitle: 'Colour-aware factory curve',
  lumSurfaceHint:
    'What the clock wants to set at each ambient level and each colour. ' +
    'Per cent is not light: the same setting emits about a tenth as much in ' +
    'deep blue as in the green this clock runs. Hue runs the whole way round, ' +
    'because it has no first value and no last one and a straight axis would ' +
    'have to cut it somewhere. The ring is where it is now.',
  lumSurfaceNone:
    'No factory profile loaded — the automatic regulates on the learned ' +
    'white curve, as it did before this measurement existed.',
  lumSurfaceSummary: (lowLux, highLux, low, high) =>
    `A surface over ${lowLux} to ${highLux} lx and every hue; the curve asks ` +
    `for between ${low} and ${high} %.`,
  lumSurfaceHere: 'where it is now',
  lumSurfaceTaught: 'taught by hand',
  lumSurfaceLimited: 'colour out of slider — it cannot go brighter',
  lumSurfaceBound: 'the observation only said “at least this much”',
  lumSurfaceRadius:
    'Angle = hue, radius = ambient light (logarithmic, brighter further out), ' +
    'height = the brightness it sets, in per cent.',
  lumSurfaceRotate:
    'Drag to turn it. The arrow keys turn and tilt it, and Home puts the view ' +
    'back.',
  lumSurfaceControls: 'Turn the view',
  lumSurfaceLeft: 'Turn left',
  lumSurfaceRight: 'Turn right',
  lumSurfaceReset: 'Reset the view',
  lumSurfaceView: (azimuth, tilt) => `Turned ${azimuth}°, tilted ${tilt}°`,
  lumFactory: 'Factory profile',
  lumFactoryNone: 'None loaded',
  lumFactoryStack: (stack) => `Optics: ${stack}`,
  lumFactorySource: {
    legacy: 'learned white curve',
    factory: 'factory profile',
    'factory+user': 'factory profile plus your corrections'
  },
  lumFactoryTarget: (percent, factory) =>
    `${percent} % wanted, the factory profile alone says ${factory} %`,
  lumFactoryAccuracy: (max, hue) =>
    `Worst held-out error: ${max} percentage points, at hue ${hue}°. The ` +
    `goal of 10 was not met — the shortfall is that one hue's observations ` +
    `contradicting each other, not the shape of the model.`,
  lumFactoryAccuracyMet: 'Cross-validation passed.',
  lumFactoryObservations:
    'The observations behind it contradict themselves in places; the fitted ' +
    'curve still rises everywhere with the light.',
  lumFactoryMismatch:
    'The stored corrections were learned on a different profile and are not ' +
    'being applied. “Restore factory” clears them.',
  lumGeekTitle: 'Fit parameters (for the curious)',
  lumGeekCone: 'Cone (slope / offset)',
  lumGeekNose: 'Colour nose (a0 / a1 / b1)',
  lumGeekBlue: 'Blue’s own line (slope / offset)',
  lumGeekFactory: 'factory',
  lumGeekLearned: 'learned',
  lumGeekWhite: 'plain white curve (no colour, for comparison)',
  lumGeekAccuracy: 'Cross-validation',
  lumGeekHint:
    'All in decades of light. The colour nose is a single wave over every ' +
    'hue but blue — a0 + a1·cos(hue) + b1·sin(hue) — and blue has its own ' +
    'line. “Learned” appears once at least one colour correction has fed ' +
    'into it. The white curve no longer drives the clock once a factory ' +
    'profile is loaded — it is shown here only for comparison.',
  lumResiduals: 'Your corrections',
  lumResidualsHint:
    'What the automatic learned from your nudges — as a difference from the ' +
    'factory profile, in decades of light, kept apart by colour. Two ' +
    'corrections at the same light in different colours do not replace one ' +
    'another. Deliberately fewer slots than the white points above: the ' +
    'factory profile already carries the shape of the curve, so what is ' +
    'left to learn is a level and a little colour preference — and one ' +
    'correction on its own often moves the curve only a little, unless it ' +
    'sits close enough to a factory measurement to replace it.',
  lumResidualsEmpty: 'Nothing corrected yet.',
  lumResidualDecades: 'Difference',
  lumFactoryRestore: 'Restore factory',
  lumFactoryRestoreHint:
    'Clears your corrections and the learned white curve. The self-light ' +
    'measurement stays: that belongs to this clock’s optics, not to anyone’s ' +
    'taste.',

  // --- timezone tab ---
  timeServer: 'Time server',
  ntpServer: 'NTP server',
  tzPickerTitle: 'Location',
  tzRegion: 'Region',
  tzPlace: 'Place',
  tzChoose: 'please choose',
  tzCustom: 'set by hand',
  tzPickerHint: 'Choosing a place fills in the rules below; they stay editable.',
  tzDataVersion: (version) => `Zone data ${version}.`,
  tzListUnavailable:
    'The zone list could not be loaded. The rules below can still be set ' +
    'by hand.',
  timezoneTitle: 'Time zone',
  dst: 'Daylight saving',
  standardTime: 'Standard time',
  noDstHint:
    'Without daylight saving the standard offset applies all year; the ' +
    'changeover moments are ignored.',
  abbreviation: 'Abbreviation',
  week: 'Week',
  weeks: ['Last', '1st', '2nd', '3rd', '4th'],
  day: 'Day',
  days: ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'],
  month: 'Month',
  months: [
    'Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun',
    'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'
  ],
  hour: 'Hour',
  offsetMin: 'Offset (min)',

  // --- wifi tab ---
  connection: 'Connection',
  network: 'Network',
  address: 'Address',
  signal: 'Signal',
  hostname: 'Host name',
  mac: 'MAC',
  quality: ['weak', 'fair', 'good', 'very good'],
  statusUnavailable: 'Status not available',
  clockName: 'Clock name',
  name: 'Name',
  saveAndRestart: 'Save and restart',
  restarting: 'The clock is restarting …',
  hostnameSaved: (host) => `Restarted. The clock now answers at ${host}.local.`,
  hostnameHint: () =>
    'Letters, digits and hyphens only. The name is used for mDNS, the router, ' +
    'espota and the setup access point — so the clock takes it on with a ' +
    'restart.',
  availableNetworks: 'Available networks',
  scanning: 'Scanning …',
  noNetworks: 'No networks found.',
  rescan: 'Scan again',
  encrypted: 'encrypted',
  switchNetwork: 'Switch network',
  password: 'Password',
  passwordPlaceholder: 'leave empty for open networks',
  connect: 'Connect',
  connecting: 'Connecting …',
  connectedTo: (ssid) => `Connected to ${ssid}.`,
  noResponse:
    'No response. If the clock has moved to a different network, it can no ' +
    'longer be reached at this address.',
  wifiHint: (host) =>
    'The clock briefly leaves the network. If the connection fails it returns ' +
    'to the previous network by itself. After a switch the address may change ' +
    `— it can then be reached at ${host}.local.`,

  // --- setup portal ---
  portalTitle: 'Set up the network',
  portalIntro: (apName) =>
    `This clock is not on a network yet and has opened its own access ` +
    `point, “${apName}”. Pick your WiFi below and enter its password.`,
  portalConnected: (ssid, ip) => `Connected to ${ssid} as ${ip}.`,
  portalConnecting: (ssid) => `Connecting to ${ssid} …`,
  portalDone:
    'The clock is restarting and will shortly be reachable under its own ' +
    'name on the network.',

  // --- update tab ---
  installed: 'Installed',
  firmware: 'Firmware',
  webUi: 'Web interface',
  used: 'Used',
  roomForUpdate: 'Room for update',
  unknown: 'unknown',
  uploadImage: 'Upload image',
  file: 'File',
  noFile: 'no file selected',
  chooseFile: 'Choose a file',
  detectedAs: 'Detected as',
  firmwareImage: 'Firmware image',
  filesystemImage: 'Filesystem image (web interface)',
  filesystemHint:
    'The filesystem image overwrites the whole partition. The settings are ' +
    'kept — they live in NVS, a partition of its own that an update does not ' +
    'touch.',
  writing: (percent) => `Writing … ${percent} %`,
  rebooting: 'The clock is restarting — please wait.',
  updateDone: (firmware, webUi) =>
    `Update installed. Firmware ${firmware}, web interface ${webUi}.`,
  upload: 'Upload and restart',
  running: 'Working …',
  noResponseAfterReboot:
    'The clock did not report back after the restart. With a faulty image it ' +
    'starts up with the previous version.',
  buildHint:
    'The clock verifies the checksum before switching to the new image, so an ' +
    'aborted upload does no harm — it simply keeps running the previous ' +
    'version.',

  // --- update channel ---
  updateSource: 'Updates',
  channel: 'Channel',
  channelStable: 'Stable (tested releases)',
  channelEdge: 'Development (every build)',
  autoUpdate: 'Install automatically',
  autoUpdateHint:
    'Installs at night between 2 and 5, so the clock never goes dark in the ' +
    'evening. Off by default: a faulty image can only be recovered over USB.',
  checkInterval: 'Check every',
  checkNever: 'never',
  hours: (count) => `${count} h`,
  checkNow: 'Check now',
  checking: 'Checking …',
  available: 'Available',
  upToDate: 'The clock is up to date.',
  neverChecked: 'not checked yet',
  lastChecked: (text) => `Last checked: ${text}`,
  justNow: 'just now',
  minutesAgo: (count) => `${count} min ago`,
  hoursAgo: (count) => `${count} h ago`,
  installNow: 'Update now',
  downloading: (percent) => `Downloading … ${percent} %`,
  runningFrom: 'Active slot',

  // --- debug tab ---
  clockState: 'State',
  uptime: 'Uptime',
  lastReset: 'Last restart',
  resetReasons: {
    'power-on': 'Power-on',
    external: 'Reset line',
    software: 'Software (restart requested)',
    panic: 'Crash (panic)',
    'watchdog-int': 'Watchdog (interrupt)',
    'watchdog-task': 'Watchdog (task)',
    watchdog: 'Watchdog',
    brownout: 'Brown-out',
    sdio: 'SDIO',
    usb: 'Reset over USB',
    jtag: 'Reset over JTAG',
    efuse: 'eFuse error',
    'power-glitch': 'Power glitch',
    'cpu-lockup': 'CPU lock-up (double exception)',
    'deep-sleep': 'Deep sleep',
    unknown: 'unknown'
  },
  heapFree: 'Free memory',
  heapMin: 'Lowest since boot',
  heapBlock: 'Largest free block',
  heapHint:
    'If an update is refused with "could not activate the firmware", this ' +
    'low-water mark is the first number worth looking at.',
  logTitle: 'Log',
  logPause: 'Pause',
  logResume: 'Resume',
  logClear: 'Clear view',
  logMissed: (count) =>
    `${count} lines scrolled out of the clock's buffer before they got here.`,
  logEmpty: 'Nothing logged yet.',
  logHint:
    'The clock keeps the last 200 lines in RAM, including the ones from ' +
    'booting. What the bootloader prints before the firmware starts is only ' +
    'on the USB cable.',

  // --- storage tab: the filesystem and NVS, one explorer for both ---
  storageTitle: 'Storage',
  storageFs: 'LittleFS',
  storageNvs: 'NVS',
  storageFsHint:
    'The clock\'s filesystem — the same partition this page is served ' +
    'from. A web UI update overwrites it whole.',
  storageFsWarn:
    'Delete index.html and the clock is reachable through the API only, ' +
    'and getting it back needs the USB cable.',
  storageNvsHint:
    'Not a filesystem but keys and values: namespaces as folders, keys ' +
    'as files. The extension is a reading of the content, not a stored ' +
    'name. An update leaves NVS untouched — which is why the settings, ' +
    'the password and the brightness curve live here.',
  storageNvsWarn:
    'The clock keeps its settings in RAM and writes them back on the ' +
    'next change. An edit made here survives only an immediate restart.',
  fsUsageEntries: (used, total) => `${used} of ${total} entries used`,
  fsKeys: (n) => `${n} keys`,
  fsRestart: 'Restart now',
  fsRestarting: 'Restarting …',
  fsConfirmRestart: 'Restart the clock now? It will be unreachable for a few seconds.',
  fsRestarted: 'Restart asked for — reload the page in a few seconds.',
  fsGesture: 'Right-click, or press and hold, to open the menu.',
  fsNewFolderHere: 'New folder inside',
  fsProtected: 'Not readable (password hash)',
  fsCompact: 'save compact',
  fsNotJson: 'not valid JSON',

  // --- the explorer itself ---
  fsUsage: (used, total) => `${used} of ${total} used`,
  fsRoot: 'Root',
  fsEmpty: 'This folder is empty.',
  fsTruncated: 'Only the first entries — the folder holds more.',
  fsDownload: 'Download',
  fsEdit: 'Edit',
  fsDelete: 'Delete',
  fsUpload: 'Upload',
  fsNewFolder: 'New folder',
  fsFolderName: 'Folder name',
  fsSave: 'Save',
  fsCancel: 'Cancel',
  fsSaved: 'Saved.',
  fsConfirmDelete: (name) => `Delete “${name}” for good?`,
  fsUploading: (percent) => `Transferring … ${percent} %`,
  fsTooLarge: 'Too large for the editor — download it instead.',
  fsBinary: 'Not text — download only.',

  // --- expert mode ---
  expertTitle: 'Expert mode',
  expertUnlocked:
    'Expert mode is on. The update and debug tabs are reachable — by anyone ' +
    'on the same network.',
  expertLock: 'Lock again',
  expertEnterHint:
    'Enter the password to open the update and debug tabs. The clock stays ' +
    'unlocked across restarts.',
  expertSetHint:
    'No password has been set on this clock yet. The first one given here ' +
    'is the one that counts; until then update and debug stay locked.',
  expertPassword: 'Password',
  expertUnlock: 'Unlock',
  expertSet: 'Set password',
  expertMinLength: (count) => `At least ${count} characters.`,
  expertLockedOut:
    'Too many wrong answers. The clock will not take a password for a few ' +
    'minutes.',
  expertForgotten: 'Forgotten the password?',
  expertResetHint: (time) =>
    `For a short while after the clock is switched on at the plug, the password can be cleared. ${time} left. After that it takes a power cut — or the USB cable.`,
  expertReset: 'Clear password',

  // --- error codes reported by the clock (see lib/errors.js) ---
  err_otaBegin: 'Update refused',
  err_otaWrite: 'Write error while flashing',
  err_otaIncomplete: 'Image incomplete',
  err_otaAborted: 'Upload aborted',
  err_otaNoImage: 'No image received',
  err_otaNoUpdate: 'No update available',
  err_otaBusy: 'An update is already running',
  err_otaServer: 'Update server not reachable',
  err_otaManifestHttp: 'Manifest could not be fetched',
  err_otaManifestParse: 'Manifest unreadable',
  err_otaDownload: 'Download failed',
  err_otaConnectionLost: 'Connection lost during the download',
  err_otaSize: 'Download incomplete',
  err_otaChecksum: 'Checksum does not match — image discarded',
  err_calibrationTooClose: 'The two points are too close together',
  err_calibrationRange: 'Brightness outside the valid range',
  err_lumNoSuchPoint: 'That point no longer exists — probably already deleted elsewhere',
  err_lumRange: 'Invalid range — 1 to 100 %, and at least 5 % apart',
  err_calibTooBright: 'Too bright to measure — cover the clock or darken the room',
  err_calibBusy: 'A measurement is already running',
  err_calibLabActive: 'The lab is holding the LEDs',
  err_calibNoSensor: 'This clock has no light sensor',
  err_calibSaturated: 'The sensor is saturated on every sensitivity step',
  err_calibNoCoupling: 'No cell reaches the sensor',
  err_calibStore: 'The measurement could not be stored',
  err_calibCancelled: 'Cancelled',
  err_calibNoTask: 'Not enough memory for the measurement',
  err_labCalibrating: 'The clock is measuring its own light',
  err_couplingInvalid: 'The own-light measurement is unreadable',
  err_hostnameInvalid: 'That name contains no usable characters',
  err_wifiConnect: (ssid) => `Could not connect to “${ssid}”`,
  err_wifiFallback: (ssid) => `Falling back to “${ssid}” failed as well`,
  err_wifiNoSsid: 'No network name given',
  err_portalBusy: 'A connection attempt is already in progress',

  err_languageNotOnPanel: 'That language does not fit this clock’s letter panel — change it in expert mode',
  err_expertLocked: 'Expert mode is locked',
  err_expertLockedOut: 'Too many attempts — try again later',
  err_expertWrongPassword: 'Wrong password',
  err_expertPasswordShort: 'That password is too short',
  err_expertNoGrace: 'The window for clearing the password has closed',

  err_fsPath: 'Invalid path',
  err_fsBody: 'Request could not be read',
  err_fsNotFound: 'Not found',
  err_fsNotDir: 'That is not a folder',
  err_fsIsDir: 'That is a folder',
  err_fsOpen: 'The file could not be created',
  err_fsWrite: 'Write failed — the filesystem is probably full',
  err_fsRename: 'The file could not be moved into place',
  err_fsAborted: 'Transfer aborted',
  err_fsTooBig: 'Too large for the editor',
  err_fsNotEmpty: 'The folder is not empty',
  err_fsExists: 'Already exists',
  err_fsDelete: 'Delete failed',
  err_fsMkdir: 'The folder could not be created',

  err_nvsPath: 'Namespace or key missing',
  err_nvsBody: 'Request could not be read',
  err_nvsNamespace: 'Namespace not found',
  err_nvsNotFound: 'Key not found',
  err_nvsProtected: 'This value is not handed out',
  err_nvsBinary: 'Binary value — download only',
  err_nvsTooBig: 'Too large for the editor',
  err_nvsNotANumber: 'This key holds a number',
  err_nvsRead: 'The value could not be read',
  err_nvsWrite: 'The value could not be written',
  err_nvsDelete: 'Delete failed',
  err_nvsMemory: 'Not enough memory',
  err_nvsNamespaceDelete: 'A namespace goes with its last key',
  err_factoryMissing: 'No factory profile in the filesystem',
  err_factoryLayout: 'The factory profile is not laid out as expected',
  err_factoryChecksum: 'The factory profile does not match its own checksum',
  err_factorySchema: 'The factory profile is a schema this firmware cannot read',
  err_factoryModel: 'The factory profile describes a different model',
  err_factoryShape: 'The factory profile is incomplete or malformed',
  err_factoryNotMonotone: 'The factory grid falls as the light rises',
  err_factoryTooBig: 'The factory profile is not the size a profile is',
  err_factoryUnreadable: 'The factory profile cannot be read',
  err_factoryUnavailable: 'No valid factory profile to restore to',

  // --- api errors ---
  connectionLost: 'Connection to the clock was lost',
  uploadAborted: 'Upload aborted'
};
