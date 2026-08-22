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
  tabs: ['Display', 'Colour', 'Time zone', 'WiFi', 'Update', 'Debug'],
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
  lumLine: 'Line',
  lumSlope: 'Slope',
  lumSlopeFitted: 'fitted from the points',
  lumSlopeKept: 'kept, points sit too close together',
  lumAdjusting: (want, secs) => `${want} % set — learned within ${secs} s`,
  lumPoints: 'Points learned',
  lumWanted: 'wanted',
  lumCurve: 'line',
  lumWhen: 'uptime',
  lumEmpty: 'Nothing learned yet — the default line applies.',

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

  // --- file explorer, above the log in the debug tab ---
  fsTitle: 'Files',
  fsHint:
    'The clock\'s filesystem (LittleFS) — the same partition this page is ' +
    'served from. NVS is not shown here: it holds keys and values, not ' +
    'files. Delete index.html and the clock is reachable through the API ' +
    'only, and getting it back needs the USB cable.',
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
  err_hostnameInvalid: 'That name contains no usable characters',
  err_wifiConnect: (ssid) => `Could not connect to “${ssid}”`,
  err_wifiFallback: (ssid) => `Falling back to “${ssid}” failed as well`,

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

  // --- api errors ---
  connectionLost: 'Connection to the clock was lost',
  uploadAborted: 'Upload aborted'
};
