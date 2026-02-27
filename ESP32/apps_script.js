/**
 * Google Apps Script — Greenhouse Logger
 * ────────────────────────────────────────────────────────────────────────────
 * Instrucciones de despliegue:
 *
 *  1. Abre (o crea) un Google Sheet con el nombre que prefieras.
 *  2. Menú: Extensiones → Apps Script.
 *  3. Reemplaza el contenido del editor con este archivo.
 *  4. Ajusta SHEET_NAME si quieres usar una hoja específica (por defecto la 1ª).
 *  5. Guarda (Ctrl+S) y luego: Desplegar → Nueva implementación.
 *       - Tipo: Aplicación web
 *       - Ejecutar como: Yo
 *       - Acceso: Cualquier usuario
 *  6. Copia la URL de despliegue ("URL de la aplicación web").
 *  7. En el ESP32 ejecuta el comando:  /gdrive <URL_copiada>
 *
 * La hoja recibirá una fila por cada entrada de log con las columnas:
 *   fecha_hora | tipo | temp_c | hr_pct | suelo_pct | mq_raw | detalle
 * ────────────────────────────────────────────────────────────────────────────
 */

var SHEET_NAME = '';  // Deja vacío para usar la primera hoja activa

function doPost(e) {
  try {
    var data    = JSON.parse(e.postData.contents);
    var ss      = SpreadsheetApp.getActiveSpreadsheet();
    var sheet   = SHEET_NAME ? ss.getSheetByName(SHEET_NAME) : ss.getActiveSheet();

    // Crear cabecera si la hoja está vacía
    if (sheet.getLastRow() === 0) {
      sheet.appendRow(['fecha_hora', 'tipo', 'temp_c', 'hr_pct', 'suelo_pct', 'mq_raw', 'detalle']);
      sheet.getRange(1, 1, 1, 7).setFontWeight('bold');
    }

    sheet.appendRow([
      data.ts      || '',
      data.tipo    || '',
      data.temp    !== undefined ? data.temp  : '',
      data.rh      !== undefined ? data.rh    : '',
      data.suelo   !== undefined ? data.suelo : '',
      data.mq      !== undefined ? data.mq    : '',
      data.detalle || ''
    ]);

    return ContentService
      .createTextOutput(JSON.stringify({ status: 'ok' }))
      .setMimeType(ContentService.MimeType.JSON);

  } catch (err) {
    return ContentService
      .createTextOutput(JSON.stringify({ status: 'error', msg: err.toString() }))
      .setMimeType(ContentService.MimeType.JSON);
  }
}

// Puedes probar el script manualmente con esta función desde el editor
function testPost() {
  var mockEvent = {
    postData: {
      contents: JSON.stringify({
        ts: '2025-01-01 12:00:00',
        tipo: 'SENSOR',
        temp: '22.5',
        rh: '55.0',
        suelo: 35,
        mq: 420,
        detalle: ''
      })
    }
  };
  var result = doPost(mockEvent);
  Logger.log(result.getContent());
}
