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
  tabs: ['Pantalla', 'Color', 'Zona horaria', 'WiFi', 'Actualización', 'Depuración'],
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
    'Prueba'
  ],
  appearance: 'Aspecto',
  language: 'Idioma',
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
  decreaseBy: (label, step) => `Reducir ${label} en ${step}`,
  increaseBy: (label, step) => `Aumentar ${label} en ${step}`,
  automatic: 'Automático',
  autoBrightness: 'Brillo automático',
  measured: 'Medido',
  calibration: 'Calibración',
  calDark: 'Oscuro',
  calBright: 'Claro',
  calCapture: 'Memorizar',
  calReset: 'Restablecer',
  calHint:
    'Ajusta el brillo que quieras con la luz actual y memorízalo: una vez a ' +
    'oscuras y otra con luz de día. De todo lo intermedio se encarga el ' +
    'reloj.',
  ldrHint:
    'La pantalla sigue la luz ambiental, de forma logarítmica entre los dos ' +
    'puntos: así es como percibe el ojo. El valor entre paréntesis no está ' +
    'suavizado y ayuda a colocar el sensor.',

  // --- timezone tab ---
  timeServer: 'Servidor de hora',
  ntpServer: 'Servidor NTP',
  tzPickerTitle: 'Ubicación',
  tzRegion: 'Región',
  tzPlace: 'Lugar',
  tzChoose: 'elija un lugar',
  tzCustom: 'ajuste manual',
  tzPickerHint:
    'La selección rellena las reglas de abajo; siguen siendo editables.',
  tzDataVersion: (version) => `Datos de zonas ${version}.`,
  tzListUnavailable:
    'No se pudo cargar la lista de zonas. Las reglas de abajo se pueden ' +
    'ajustar a mano.',
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
  clockName: 'Nombre del reloj',
  name: 'Nombre',
  saveAndRestart: 'Guardar y reiniciar',
  restarting: 'El reloj se está reiniciando …',
  hostnameSaved: (host) => `Reiniciado. El reloj responde ahora en ${host}.local.`,
  hostnameHint: () =>
    'Solo letras, números y guiones. El nombre se usa para mDNS, el router, ' +
    'espota y el punto de acceso de configuración, así que el reloj lo adopta ' +
    'al reiniciar.',
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

  // --- debug tab ---
  clockState: 'Estado',
  uptime: 'Tiempo en marcha',
  lastReset: 'Último reinicio',
  resetReasons: {
    'power-on': 'Encendido',
    external: 'Línea de reinicio',
    software: 'Software (reinicio solicitado)',
    panic: 'Fallo (panic)',
    'watchdog-int': 'Watchdog (interrupción)',
    'watchdog-task': 'Watchdog (tarea)',
    watchdog: 'Watchdog',
    brownout: 'Caída de tensión',
    sdio: 'SDIO',
    usb: 'Reinicio por USB',
    jtag: 'Reinicio por JTAG',
    efuse: 'Error de eFuse',
    'power-glitch': 'Pico de tensión',
    'cpu-lockup': 'Bloqueo de la CPU (doble excepción)',
    'deep-sleep': 'Sueño profundo',
    unknown: 'desconocido'
  },
  heapFree: 'Memoria libre',
  heapMin: 'Mínimo desde el arranque',
  heapBlock: 'Mayor bloque libre',
  heapHint:
    'Si una actualización se rechaza con «no se pudo activar el firmware», ' +
    'este mínimo es el primer número que conviene mirar.',
  logTitle: 'Registro',
  logPause: 'Pausar',
  logResume: 'Reanudar',
  logClear: 'Vaciar la ventana',
  logMissed: (count) =>
    `${count} líneas salieron de la memoria del reloj antes de llegar aquí.`,
  logEmpty: 'Todavía no hay nada registrado.',
  logHint:
    'El reloj guarda las últimas 200 líneas en memoria, incluidas las del ' +
    'arranque. Lo que el gestor de arranque imprime antes del firmware solo ' +
    'se ve por el cable USB.',

  // --- expert mode ---
  expertTitle: 'Modo experto',
  expertUnlocked:
    'El modo experto está activo. Las pestañas de actualización y ' +
    'depuración están accesibles — para cualquiera en la misma red.',
  expertLock: 'Bloquear de nuevo',
  expertEnterHint:
    'Introduce la contraseña para abrir las pestañas de actualización y ' +
    'depuración. El desbloqueo se mantiene tras reiniciar.',
  expertSetHint:
    'En este reloj todavía no hay contraseña. Vale la primera que se ' +
    'indique aquí; hasta entonces, actualización y depuración quedan ' +
    'bloqueadas.',
  expertPassword: 'Contraseña',
  expertUnlock: 'Desbloquear',
  expertSet: 'Establecer contraseña',
  expertMinLength: (count) => `Al menos ${count} caracteres.`,
  expertLockedOut:
    'Demasiados intentos fallidos. El reloj no aceptará contraseñas durante ' +
    'unos minutos.',
  expertForgotten: '¿Has olvidado la contraseña?',
  expertResetHint: (time) =>
    `Poco después de encender el reloj se puede borrar la contraseña. Quedan ${time}. Después solo ayuda cortar la corriente — o el cable USB.`,
  expertReset: 'Borrar la contraseña',

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
  err_calibrationTooClose: 'Los dos puntos están demasiado juntos',
  err_calibrationRange: 'Brillo fuera del intervalo válido',
  err_hostnameInvalid: 'Ese nombre no contiene caracteres utilizables',
  err_wifiConnect: (ssid) => `No se ha podido conectar a «${ssid}»`,
  err_wifiFallback: (ssid) => `Volver a «${ssid}» también ha fallado`,

  err_languageNotOnPanel: 'Ese idioma no encaja con el panel de letras del reloj — se cambia en modo experto',
  err_expertLocked: 'El modo experto está bloqueado',
  err_expertLockedOut: 'Demasiados intentos — inténtalo más tarde',
  err_expertWrongPassword: 'Contraseña incorrecta',
  err_expertPasswordShort: 'Esa contraseña es demasiado corta',
  err_expertNoGrace: 'La ventana para borrar la contraseña se ha cerrado',

  // --- api errors ---
  connectionLost: 'Se ha perdido la conexión con el reloj',
  uploadAborted: 'Subida interrumpida'
};
