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
  tabs: ['Display', 'Colour', 'Time zone', 'WiFi', 'Update'],
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
  languages: [
    'German',
    'Swabian',
    'Bavarian',
    'Saxon',
    'Swiss German',
    'English',
    'French',
    'Italian',
    'Dutch',
    'Spanish'
  ],
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
  preview: ['IT IS', 'FIVE PAST', 'TWO'],
  decreaseBy: (label, step) => `Decrease ${label} by ${step}`,
  increaseBy: (label, step) => `Increase ${label} by ${step}`,
  automatic: 'Automatic',
  autoBrightness: 'Automatic brightness',
  measured: 'Measured',
  resulting: 'Resulting',
  calibration: 'Calibration',
  calDark: 'Dark',
  calBright: 'Bright',
  calCapture: 'Capture now',
  calReset: 'Reset',
  calHint:
    'Set the brightness you want at the light there is right now, then ' +
    'capture it — once in the dark, once in daylight. Everything in ' +
    'between the clock works out for itself.',
  calHintAutoOn:
    'Switch the automatic off to calibrate: both points are taken from the ' +
    'brightness slider, and it is not driving the display at the moment.',
  ldrHint:
    'The display follows the ambient light, logarithmically between the two ' +
    'points — that is how the eye perceives it. The figure in brackets is ' +
    'unsmoothed and helps when placing the sensor.',

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

  // --- api errors ---
  connectionLost: 'Connection to the clock was lost',
  uploadAborted: 'Upload aborted'
};
