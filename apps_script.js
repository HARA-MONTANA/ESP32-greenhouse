/**
 * Google Apps Script — Greenhouse Logger
 * ────────────────────────────────────────────────────────────────────────────
 * Crea en tu Google Drive la misma estructura de la tarjeta SD:
 *
 *   Mi unidad/
 *   └── logs/
 *       ├── 02-2026/
 *       │   ├── 2026-02-25.csv
 *       │   ├── 2026-02-26.csv
 *       │   └── 2026-02-27.csv
 *       └── 03-2026/
 *           └── 2026-03-01.csv
 *
 * Cada CSV tiene la misma cabecera y formato que el archivo de la SD:
 *   fecha_hora,tipo,temp_c,hr_pct,suelo_pct,mq_raw,detalle
 *
 * ── Instrucciones de despliegue ──────────────────────────────────────────────
 *  1. Abre (o crea) una hoja de cálculo de Google — aunque el script no la
 *     usa, es la forma más fácil de llegar al editor de Apps Script.
 *  2. Menú: Extensiones → Apps Script.
 *  3. Reemplaza todo el contenido del editor con este archivo y guarda (Ctrl+S).
 *  4. Desplegar → Nueva implementación:
 *       - Tipo:          Aplicación web
 *       - Ejecutar como: Yo
 *       - Acceso:        Cualquier usuario  ← imprescindible
 *  5. Autoriza los permisos de Google Drive cuando se soliciten.
 *  6. Copia la "URL de la aplicación web" que aparece al finalizar.
 *  7. Pégala en el ESP32 durante el arranque (prompt de credenciales) o con:
 *         /gdrive <URL>
 * ────────────────────────────────────────────────────────────────────────────
 */

// Nombre de la carpeta raíz que se creará en "Mi unidad"
var ROOT_FOLDER_NAME = 'logs';

// ── Punto de entrada del webhook ─────────────────────────────────────────────

function doPost(e) {
  try {
    var data = JSON.parse(e.postData.contents);
    // data: { ts, tipo, temp?, rh?, suelo?, mq?, detalle? }

    // Parsear timestamp "YYYY-MM-DD HH:MM:SS"
    var ts    = data.ts || '';                // "2026-02-27 12:00:00"
    var date  = ts.substring(0, 10);          // "2026-02-27"
    var parts = date.split('-');              // ["2026","02","27"]
    var year  = parts[0];
    var month = parts[1];

    var folderName = month + '-' + year;      // "02-2026"
    var fileName   = date + '.csv';           // "2026-02-27.csv"

    // Obtener o crear estructura de carpetas: logs/MM-YYYY/
    var root      = DriveApp.getRootFolder();
    var logsDir   = getOrCreateFolder(root, ROOT_FOLDER_NAME);
    var monthDir  = getOrCreateFolder(logsDir, folderName);
    var csvFile   = getOrCreateCsv(monthDir, fileName);

    // Construir fila CSV con el mismo formato que la SD
    var row = buildRow(data);

    // Leer contenido actual, agregar cabecera si el archivo es nuevo, y añadir fila
    var content = csvFile.getBlob().getDataAsString('UTF-8');
    if (content.length === 0) {
      content = 'fecha_hora,tipo,temp_c,hr_pct,suelo_pct,mq_raw,detalle\n';
    }
    csvFile.setContent(content + row + '\n');

    return ContentService
      .createTextOutput(JSON.stringify({ status: 'ok', file: fileName }))
      .setMimeType(ContentService.MimeType.JSON);

  } catch (err) {
    return ContentService
      .createTextOutput(JSON.stringify({ status: 'error', msg: err.toString() }))
      .setMimeType(ContentService.MimeType.JSON);
  }
}

// ── Helpers ──────────────────────────────────────────────────────────────────

function getOrCreateFolder(parent, name) {
  var it = parent.getFoldersByName(name);
  return it.hasNext() ? it.next() : parent.createFolder(name);
}

function getOrCreateCsv(folder, fileName) {
  var it = folder.getFilesByName(fileName);
  if (it.hasNext()) return it.next();
  return folder.createFile(fileName, '', MimeType.PLAIN_TEXT);
}

// Genera la fila CSV igual que writeLog() en sdcard.cpp
function buildRow(data) {
  var ts      = data.ts      || '';
  var tipo    = data.tipo    || '';
  var detalle = data.detalle || '';

  if (data.hasNumeric) {
    // Fila de sensores / acción con datos numéricos
    var temp  = data.temp  !== undefined ? String(data.temp)  : '';
    var rh    = data.rh    !== undefined ? String(data.rh)    : '';
    var suelo = data.suelo !== undefined ? String(data.suelo) : '';
    var mq    = data.mq    !== undefined ? String(data.mq)    : '';
    return [ts, tipo, temp, rh, suelo, mq, csvEscape(detalle)].join(',');
  } else {
    // Fila de acción sin datos numéricos (vacía en columnas numéricas)
    return ts + ',' + tipo + ',,,,,\"' + detalle.replace(/"/g, '""') + '\"';
  }
}

// Escapa un campo CSV si contiene comas o comillas
function csvEscape(val) {
  if (!val) return '';
  if (val.indexOf(',') >= 0 || val.indexOf('"') >= 0) {
    return '"' + val.replace(/"/g, '""') + '"';
  }
  return val;
}

// ── Prueba manual desde el editor ────────────────────────────────────────────

function testPostSensor() {
  var mock = {
    postData: {
      contents: JSON.stringify({
        ts: '2026-02-27 12:00:00',
        tipo: 'SENSOR',
        temp: '22.5',
        rh: '55.0',
        suelo: 35,
        mq: 420,
        hasNumeric: true,
        detalle: ''
      })
    }
  };
  Logger.log(doPost(mock).getContent());
}

function testPostAccion() {
  var mock = {
    postData: {
      contents: JSON.stringify({
        ts: '2026-02-27 12:05:00',
        tipo: 'RIEGO',
        hasNumeric: false,
        detalle: 'manual 250 mL'
      })
    }
  };
  Logger.log(doPost(mock).getContent());
}
