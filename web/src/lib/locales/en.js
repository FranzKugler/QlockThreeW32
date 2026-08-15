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
    'Test',
    'Status'
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
  ldrHint:
    'Has no effect while the LDR evaluation is commented out in the firmware ' +
    '(see src/LDR.cpp).',

  // --- timezone tab ---
  timeServer: 'Time server',
  ntpServer: 'NTP server',
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
    'firmware.bin and littlefs.bin are produced by "pio run" and ' +
    '"pio run -t buildfs" in .pio/build/seeed_xiao_esp32s3/. The clock ' +
    'verifies the checksum before switching to the new image, so an aborted ' +
    'upload does no harm — it simply keeps running the previous version.',

  // --- api errors ---
  connectionLost: 'Connection to the clock was lost',
  uploadAborted: 'Upload aborted'
};
