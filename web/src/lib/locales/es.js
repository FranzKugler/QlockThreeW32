/**
 * es
 * Spanish texts of the configuration UI. Keys follow locales/de.js.
 *
 * @author   Franz Kugler / franz _AT_ franz _MINUS_ kugler _DOT_ de
 * @version  2.0
 * @created  15.8.2026
 * @updated  15.8.2026
 */
export default {
  // --- shell ---
  tabs: ['Pantalla', 'Color', 'Zona horaria', 'WiFi', 'Actualización'],
  loading: 'Cargando los ajustes …',
  loadingShort: 'cargando …',
  clockUnreachable: 'Reloj no accesible',
  retry: 'Reintentar',
  writeFailed: 'Error al enviar',

  // --- display tab ---
  displayTitle: 'Pantalla',
  modes: [
    'Hora',
    'Hora con estado WiFi',
    'Apagado (oscuro)',
    'Segundos',
    'Prueba',
    'Estado'
  ],
  appearance: 'Aspecto',
  language: 'Idioma',
  languages: [
    'Alemán',
    'Suabo',
    'Bávaro',
    'Sajón',
    'Suizo alemán',
    'Inglés',
    'Francés',
    'Italiano',
    'Neerlandés',
    'Español'
  ],
  corners: 'Esquinas',
  clockwise: 'en sentido horario',
  counterClockwise: 'en sentido antihorario',
  minutes: 'Minutos',
  monochrome: 'monocromo',
  colored: 'en color',

  // --- colour tab ---
  colorTitle: 'Color',
  hue: 'Tono',
  saturation: 'Saturación',
  brightness: 'Brillo',
  preview: ['SON LAS', 'DOS', 'Y CINCO'],
  decreaseBy: (label, step) => `Reducir ${label} en ${step}`,
  increaseBy: (label, step) => `Aumentar ${label} en ${step}`,
  automatic: 'Automático',
  autoBrightness: 'Brillo automático',
  ldrHint:
    'Sin efecto mientras la evaluación del LDR esté comentada en el firmware ' +
    '(véase src/LDR.cpp).',

  // --- timezone tab ---
  timeServer: 'Servidor de hora',
  ntpServer: 'Servidor NTP',
  timezoneTitle: 'Zona horaria',
  dst: 'Horario de verano',
  standardTime: 'Horario estándar',
  noDstHint:
    'Sin horario de verano se aplica todo el año el desfase del horario ' +
    'estándar; los momentos de cambio no se tienen en cuenta.',
  abbreviation: 'Abreviatura',
  week: 'Semana',
  weeks: ['Última', '1ª', '2ª', '3ª', '4ª'],
  day: 'Día',
  days: ['dom', 'lun', 'mar', 'mié', 'jue', 'vie', 'sáb'],
  month: 'Mes',
  months: [
    'ene', 'feb', 'mar', 'abr', 'may', 'jun',
    'jul', 'ago', 'sep', 'oct', 'nov', 'dic'
  ],
  hour: 'Hora',
  offsetMin: 'Desfase (min)',

  // --- wifi tab ---
  connection: 'Conexión',
  network: 'Red',
  address: 'Dirección',
  signal: 'Señal',
  hostname: 'Nombre de host',
  mac: 'MAC',
  quality: ['débil', 'media', 'buena', 'muy buena'],
  statusUnavailable: 'Estado no disponible',
  availableNetworks: 'Redes disponibles',
  scanning: 'Buscando …',
  noNetworks: 'No se han encontrado redes.',
  rescan: 'Buscar de nuevo',
  encrypted: 'cifrada',
  switchNetwork: 'Cambiar de red',
  password: 'Contraseña',
  passwordPlaceholder: 'dejar vacío para redes abiertas',
  connect: 'Conectar',
  connecting: 'Conectando …',
  connectedTo: (ssid) => `Conectado a ${ssid}.`,
  noResponse:
    'Sin respuesta. Si el reloj ha cambiado a otra red, ya no es accesible en ' +
    'esta dirección.',
  wifiHint: (host) =>
    'El reloj abandona la red brevemente. Si la conexión falla, vuelve por sí ' +
    'mismo a la red anterior. Tras un cambio la dirección puede variar — ' +
    `entonces es accesible en ${host}.local.`,

  // --- update tab ---
  installed: 'Instalado',
  firmware: 'Firmware',
  webUi: 'Interfaz web',
  used: 'Ocupado',
  roomForUpdate: 'Espacio para actualizar',
  unknown: 'desconocido',
  uploadImage: 'Subir imagen',
  file: 'Archivo',
  noFile: 'ningún archivo seleccionado',
  chooseFile: 'Elegir un archivo',
  detectedAs: 'Reconocido como',
  firmwareImage: 'Imagen de firmware',
  filesystemImage: 'Imagen del sistema de archivos (interfaz web)',
  filesystemHint:
    'La imagen del sistema de archivos sobrescribe toda la partición. Los ' +
    'ajustes se conservan — están en la NVS, una partición propia que una ' +
    'actualización no toca.',
  writing: (percent) => `Escribiendo … ${percent} %`,
  rebooting: 'El reloj se está reiniciando — espere.',
  updateDone: (firmware, webUi) =>
    `Actualización instalada. Firmware ${firmware}, interfaz web ${webUi}.`,
  upload: 'Subir y reiniciar',
  running: 'En curso …',
  noResponseAfterReboot:
    'El reloj no ha respondido tras el reinicio. Con una imagen defectuosa ' +
    'arranca con la versión anterior.',
  buildHint:
    'El reloj comprueba la suma de verificación antes de pasar a la nueva ' +
    'imagen, así que una subida interrumpida no causa daños: simplemente ' +
    'arranca con la versión anterior.',

  // --- update channel ---
  updateSource: 'Actualizaciones',
  channel: 'Canal',
  channelStable: 'Estable (versiones probadas)',
  channelEdge: 'Desarrollo (cada compilación)',
  autoUpdate: 'Instalar automáticamente',
  autoUpdateHint:
    'Instala de noche entre las 2 y las 5, para que el reloj no se apague por ' +
    'la tarde. Desactivado por defecto: una imagen defectuosa solo se ' +
    'recupera por USB.',
  checkInterval: 'Comprobar cada',
  checkNever: 'nunca',
  hours: (count) => `${count} h`,
  checkNow: 'Comprobar ahora',
  checking: 'Comprobando …',
  available: 'Disponible',
  upToDate: 'El reloj está actualizado.',
  neverChecked: 'aún sin comprobar',
  lastChecked: (text) => `Última comprobación: ${text}`,
  justNow: 'ahora mismo',
  minutesAgo: (count) => `hace ${count} min`,
  hoursAgo: (count) => `hace ${count} h`,
  installNow: 'Actualizar ahora',
  downloading: (percent) => `Descargando … ${percent} %`,
  runningFrom: 'Ranura activa',

  // --- error codes reported by the clock (see lib/errors.js) ---
  err_otaBegin: 'Actualización rechazada',
  err_otaWrite: 'Error de escritura al grabar',
  err_otaIncomplete: 'Imagen incompleta',
  err_otaAborted: 'Subida interrumpida',
  err_otaNoImage: 'No se ha recibido ninguna imagen',
  err_otaNoUpdate: 'No hay actualizaciones disponibles',
  err_otaBusy: 'Ya hay una actualización en curso',
  err_otaServer: 'Servidor de actualizaciones no accesible',
  err_otaManifestHttp: 'No se ha podido obtener el manifiesto',
  err_otaManifestParse: 'Manifiesto ilegible',
  err_otaDownload: 'Error en la descarga',
  err_otaConnectionLost: 'Conexión perdida durante la descarga',
  err_otaSize: 'Descarga incompleta',
  err_otaChecksum: 'La suma de verificación no coincide — imagen descartada',
  err_wifiConnect: (ssid) => `No se ha podido conectar a «${ssid}»`,
  err_wifiFallback: (ssid) => `Volver a «${ssid}» también ha fallado`,

  // --- api errors ---
  connectionLost: 'Se ha perdido la conexión con el reloj',
  uploadAborted: 'Subida interrumpida'
};
