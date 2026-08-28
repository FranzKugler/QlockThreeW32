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
  tabs: ['Pantalla', 'Color', 'Zona horaria', 'WiFi', 'Actualización', 'Brillo', 'Depuración', 'Almacenamiento'],
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
  calTaught: (n) => `${n} puntos aprendidos`,
  calReset: 'Restablecer',
  calHint:
    'Con el automático activado, el control de brillo significa «con esta ' +
    'luz lo quiero así». Diez segundos después del último cambio el reloj ' +
    'guarda el par y traza una recta nueva por todo lo aprendido. No hay ' +
    'nada que pulsar.',
  ldrHint:
    'La pantalla sigue la luz ambiente de forma logarítmica, como el ojo. ' +
    'Se regula entre 20 y 100 %. El valor entre paréntesis es sin suavizar ' +
    'y ayuda a colocar el sensor.',
  lumTitle: 'Curva de brillo',
  lumHint:
    'Lo que el automático ha aprendido y lo que deduce de ello. Solo ' +
    'lectura: el restablecimiento está en la pestaña de color.',
  lumApplied: 'En la esfera',
  lumLine: 'Recta',
  lumSlope: 'Pendiente',
  lumSlopeFitted: 'calculada a partir de los puntos',
  lumSlopeKept: 'conservada, los puntos están demasiado juntos',
  lumAnchor: 'La pendiente y el nivel salen de todos los puntos (mínimos cuadrados), con el mismo peso: por eso la recta no suele pasar por ninguno. Es intencionado: varias medidas promedian el error de apreciación propio.',
  lumNewest: 'punto más reciente: el más antiguo sale en cuanto llega un undécimo',
  lumCensored: 'en el tope superior: no cuenta para la recta',
  lumCensoredHint: 'Los puntos en el tope superior se dibujan huecos y quedan fuera del ajuste: por encima del máximo no se podía pedir nada, así que un punto así significa «al menos tanto»; leído como igualdad aplanaría la recta.',
  lumTaughtIn: (hue, sat) => `aprendido con tono ${hue}°, saturación ${sat} %`,
  lumAdjusting: (want, secs) => `${want} % ajustado: se aprende en ${secs} s`,
  lumPoints: 'Puntos aprendidos',
  lumWanted: 'deseado',
  lumCurve: 'recta',
  lumWhen: 'actividad',
  lumEmpty: 'Todavía no ha aprendido nada: rige la recta por defecto.',
  lumRange: 'Intervalo de regulación',
  lumRangeHint: 'Hasta dónde puede bajar y subir el automático. Más allá de cada extremo la curva se mantiene plana: son las dos líneas punteadas del gráfico. Solo el modo de pantalla apaga la esfera, nunca el sensor.',
  lumRangeMin: 'más oscuro',
  lumRangeMax: 'más claro',
  lumReadOnly: 'Solo lectura. Desbloquee el reloj en modo experto para editar.',
  lumCoupling: 'Luz propia',
  lumCoupledCells: (n) => `${n} celdas medidas`,
  lumCoupledNone: 'Sin medir: el reloj no resta su propia luz.',
  lumDisplayShare: (lx, raw) => `${lx} lx de ${raw} lx en bruto son la propia pantalla`,
  lumForget: 'Olvidar',
  lumForgetTitle: 'Olvidar este punto',
  lumResetPoints: 'Olvidar todos los puntos',
  lumResetCoupling: 'Borrar la medición de luz propia',
  lumResetCouplingHint: 'El reloj volverá a regular con la lectura en bruto, con la realimentación de su propia pantalla.',
  lumCalibrate: 'Medir la luz propia',
  lumCalibrateHint: (max) => `El reloj recorre cada celda una a una, unos 90 s. Tiene que estar oscuro: cubra el reloj u oscurezca la sala; por encima de ${max} lx se rinde.`,
  lumCalibrateAbort: 'Cancelar',
  lumCalibratePhases: ['listo', 'comprobando la luz ambiental', 'eligiendo la sensibilidad', 'recorriendo celdas', 'midiendo canales', 'curva de excitación', 'guardando', 'hecho', 'ha fallado'],
  lumCalibrateResult: (cells, rung) => `${cells} celdas medidas, escalón ${rung}`,
  lumCalibrateAmbient: (lx) => `ambiente ${lx} lx`,

  // --- the colour-aware factory model ---
  lumSurfaceTitle: 'Curva de fábrica según el color',
  lumSurfaceHint:
    'Lo que el reloj quiere ajustar en cada nivel de luz y cada color. El ' +
    'porcentaje no es luz: el mismo ajuste emite una décima parte en azul ' +
    'profundo que en el verde de este reloj. El tono da la vuelta entera, ' +
    'porque no tiene un primer ni un último valor y un eje recto tendría que ' +
    'cortarlo en algún sitio. El anillo marca el punto actual.',
  lumSurfaceNone:
    'No hay perfil de fábrica cargado — el automático sigue la curva blanca ' +
    'aprendida, como antes de esta medición.',
  lumSurfaceSummary: (lowLux, highLux, low, high) =>
    `Una superficie de ${lowLux} a ${highLux} lx sobre todos los tonos; la ` +
    `curva pide entre ${low} y ${high} %.`,
  lumSurfaceHere: 'punto actual',
  lumSurfaceLimited: 'color al tope — no puede subir más',
  lumSurfaceBound: 'la medición decía «al menos esto»',
  lumSurfaceRadius:
    'Ángulo = tono, radio = luz ambiente (escala logarítmica, más claro hacia ' +
    'fuera), altura = el brillo que ajusta, en por ciento.',
  lumSurfaceRotate:
    'Arrastra para girarlo. Las flechas giran e inclinan la vista, e Inicio ' +
    'la devuelve a su sitio.',
  lumSurfaceControls: 'Girar la vista',
  lumSurfaceLeft: 'Girar a la izquierda',
  lumSurfaceRight: 'Girar a la derecha',
  lumSurfaceReset: 'Restablecer la vista',
  lumSurfaceView: (azimuth, tilt) => `Giro ${azimuth}°, inclinación ${tilt}°`,
  lumFactory: 'Perfil de fábrica',
  lumFactoryNone: 'Ninguno cargado',
  lumFactoryStack: (stack) => `Óptica: ${stack}`,
  lumFactorySource: {
    legacy: 'curva blanca aprendida',
    factory: 'perfil de fábrica',
    'factory+user': 'perfil de fábrica más tus correcciones'
  },
  lumFactoryTarget: (percent, factory) =>
    `${percent} % deseados, el perfil de fábrica solo dice ${factory} %`,
  lumFactoryAccuracy: (max, hue) =>
    `Peor error en validación cruzada: ${max} puntos porcentuales, en el ` +
    `tono ${hue}°. No se alcanzó el objetivo de 10 — la diferencia está en ` +
    `las mediciones de ese tono, no en la forma del modelo.`,
  lumFactoryAccuracyMet: 'Validación cruzada superada.',
  lumFactoryObservations:
    'Las mediciones de base se contradicen en algunos puntos; la rejilla ' +
    'entregada sube igualmente en todas partes con la luz.',
  lumFactoryMismatch:
    'Las correcciones guardadas se aprendieron con otro perfil y no se ' +
    'aplican. «Restaurar fábrica» las elimina.',
  lumResiduals: 'Tus correcciones',
  lumResidualsHint:
    'Lo que el automático aprendió de tus ajustes — como diferencia con el ' +
    'perfil de fábrica, en décadas de luz, separadas por color. Dos ' +
    'correcciones con la misma luz en colores distintos no se sustituyen.',
  lumResidualsEmpty: 'Todavía nada corregido.',
  lumResidualDecades: 'Diferencia',
  lumFactoryRestore: 'Restaurar fábrica',
  lumFactoryRestoreHint:
    'Borra tus correcciones y la curva blanca aprendida. La medición de la ' +
    'luz propia permanece: pertenece a la óptica de este reloj, no a un ' +
    'gusto.',

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

  // --- storage tab: the filesystem and NVS, one explorer for both ---
  storageTitle: 'Almacenamiento',
  storageFs: 'LittleFS',
  storageNvs: 'NVS',
  storageFsHint:
    'El sistema de archivos del reloj: la misma partición desde la que se ' +
    'sirve esta página. Una actualización de la interfaz la reescribe entera.',
  storageFsWarn:
    'Si se borra index.html, el reloj solo queda accesible por la API y ' +
    'hace falta el cable USB para recuperarlo.',
  storageNvsHint:
    'No es un sistema de archivos sino claves y valores: los espacios de ' +
    'nombres como carpetas y las claves como archivos. La extensión es una ' +
    'lectura del contenido, no un nombre guardado. Una actualización no ' +
    'toca NVS: por eso aquí viven los ajustes, la contraseña y la curva.',
  storageNvsWarn:
    'El reloj mantiene sus ajustes en memoria y los reescribe en el ' +
    'siguiente cambio. Una edición hecha aquí solo sobrevive a un ' +
    'reinicio inmediato.',
  fsUsageEntries: (used, total) => `${used} de ${total} entradas ocupadas`,
  fsKeys: (n) => `${n} claves`,
  fsRestart: 'Reiniciar ahora',
  fsRestarting: 'Reiniciando …',
  fsConfirmRestart: '¿Reiniciar el reloj ahora? Estará inaccesible unos segundos.',
  fsRestarted: 'Reinicio solicitado: recarga la página en unos segundos.',
  fsGesture: 'Clic derecho, o pulsación larga, para abrir el menú.',
  fsNewFolderHere: 'Carpeta nueva dentro',
  fsProtected: 'No legible (hash de la contraseña)',
  fsCompact: 'guardar compacto',
  fsNotJson: 'JSON no válido',

  // --- the explorer itself ---
  fsUsage: (used, total) => `${used} de ${total} ocupados`,
  fsRoot: 'Raíz',
  fsEmpty: 'Esta carpeta está vacía.',
  fsTruncated: 'Solo las primeras entradas: la carpeta contiene más.',
  fsDownload: 'Descargar',
  fsEdit: 'Editar',
  fsDelete: 'Eliminar',
  fsUpload: 'Subir',
  fsNewFolder: 'Carpeta nueva',
  fsFolderName: 'Nombre de la carpeta',
  fsSave: 'Guardar',
  fsCancel: 'Cancelar',
  fsSaved: 'Guardado.',
  fsConfirmDelete: (name) => `¿Eliminar «${name}» definitivamente?`,
  fsUploading: (percent) => `Transfiriendo… ${percent} %`,
  fsTooLarge: 'Demasiado grande para el editor: descárgalo.',
  fsBinary: 'No es texto: solo descarga.',

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
  err_lumNoSuchPoint: 'Ese punto ya no existe: probablemente ya se eliminó en otro sitio',
  err_lumRange: 'Intervalo no válido: de 1 a 100 %, y al menos 5 % de separación',
  err_calibTooBright: 'Demasiada luz para medir: cubra el reloj u oscurezca la sala',
  err_calibBusy: 'Ya hay una medición en curso',
  err_calibLabActive: 'El laboratorio está ocupando los LED',
  err_calibNoSensor: 'Este reloj no tiene sensor de luz',
  err_calibSaturated: 'El sensor se satura en todos los escalones de sensibilidad',
  err_calibNoCoupling: 'Ninguna celda alcanza el sensor',
  err_calibStore: 'No se pudo guardar la medición',
  err_calibCancelled: 'Cancelado',
  err_calibNoTask: 'Memoria insuficiente para la medición',
  err_labCalibrating: 'El reloj está midiendo su propia luz',
  err_couplingInvalid: 'La medición de luz propia es ilegible',
  err_hostnameInvalid: 'Ese nombre no contiene caracteres utilizables',
  err_wifiConnect: (ssid) => `No se ha podido conectar a «${ssid}»`,
  err_wifiFallback: (ssid) => `Volver a «${ssid}» también ha fallado`,

  err_languageNotOnPanel: 'Ese idioma no encaja con el panel de letras del reloj — se cambia en modo experto',
  err_expertLocked: 'El modo experto está bloqueado',
  err_expertLockedOut: 'Demasiados intentos — inténtalo más tarde',
  err_expertWrongPassword: 'Contraseña incorrecta',
  err_expertPasswordShort: 'Esa contraseña es demasiado corta',
  err_expertNoGrace: 'La ventana para borrar la contraseña se ha cerrado',

  err_fsPath: 'Ruta no válida',
  err_fsBody: 'Petición ilegible',
  err_fsNotFound: 'No encontrado',
  err_fsNotDir: 'Eso no es una carpeta',
  err_fsIsDir: 'Eso es una carpeta',
  err_fsOpen: 'No se pudo crear el archivo',
  err_fsWrite: 'Error de escritura: seguramente el sistema de archivos está lleno',
  err_fsRename: 'No se pudo colocar el archivo en su sitio',
  err_fsAborted: 'Transferencia cancelada',
  err_fsTooBig: 'Demasiado grande para el editor',
  err_fsNotEmpty: 'La carpeta no está vacía',
  err_fsExists: 'Ya existe',
  err_fsDelete: 'No se pudo eliminar',
  err_fsMkdir: 'No se pudo crear la carpeta',

  err_nvsPath: 'Falta el espacio de nombres o la clave',
  err_nvsBody: 'Petición ilegible',
  err_nvsNamespace: 'Espacio de nombres no encontrado',
  err_nvsNotFound: 'Clave no encontrada',
  err_nvsProtected: 'Este valor no se entrega',
  err_nvsBinary: 'Valor binario: solo descarga',
  err_nvsTooBig: 'Demasiado grande para el editor',
  err_nvsNotANumber: 'Esta clave contiene un número',
  err_nvsRead: 'No se pudo leer el valor',
  err_nvsWrite: 'No se pudo escribir el valor',
  err_nvsDelete: 'No se pudo eliminar',
  err_nvsMemory: 'Memoria insuficiente',
  err_nvsNamespaceDelete: 'Un espacio de nombres desaparece con su última clave',
  err_factoryMissing: 'No hay perfil de fábrica en el sistema de archivos',
  err_factoryLayout: 'El perfil de fábrica no tiene la estructura esperada',
  err_factoryChecksum: 'El perfil de fábrica no coincide con su suma de verificación',
  err_factorySchema: 'Esquema de perfil de fábrica desconocido',
  err_factoryModel: 'El perfil de fábrica describe otro modelo',
  err_factoryShape: 'Perfil de fábrica incompleto o incorrecto',
  err_factoryNotMonotone: 'La rejilla de fábrica baja al subir la luz',
  err_factoryTooBig: 'El perfil de fábrica no tiene el tamaño de un perfil',
  err_factoryUnreadable: 'No se puede leer el perfil de fábrica',
  err_factoryUnavailable: 'No hay perfil de fábrica válido que restaurar',

  // --- api errors ---
  connectionLost: 'Se ha perdido la conexión con el reloj',
  uploadAborted: 'Subida interrumpida'
};
