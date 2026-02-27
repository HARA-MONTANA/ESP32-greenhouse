#pragma once
#include <Arduino.h>

// ─── Google Drive logging via Apps Script webhook ─────────────────────────────
//
// El ESP32 envía un HTTP POST (JSON) a una URL de Google Apps Script que el
// usuario debe desplegar en su cuenta de Google. El script añade la fila al
// Google Sheet elegido. No se necesita OAuth2 en el ESP32.
//
// Configuración inicial:
//   1. Crea un Google Sheet y un Apps Script (herramientas → Apps Script).
//   2. Pega el código de apps_script.js que viene en este mismo directorio.
//   3. Despliega como aplicación web:  "Ejecutar como: yo" / "Acceso: cualquiera".
//   4. Copia la URL de despliegue y envíala al ESP32 con el comando:
//        /gdrive <url>
//   5. Para desactivar:  /gdrive off
//   6. Para consultar estado:  /gdrive
// ─────────────────────────────────────────────────────────────────────────────

// Inicializa el módulo (carga la URL almacenada en NVS)
void gdriveInit();

// Envía una fila de log al webhook de Google Apps Script.
// Diseñado para usarse como SdLogHook en sdcard.h.
void gdriveWriteLog(const char* tipo,
                    float tempC, float rh,
                    int soilPct, int mqRaw,
                    bool hasNumeric,
                    const char* detalle);

// Devuelve la URL configurada (vacía si no hay ninguna)
String gdriveGetUrl();

// Guarda la URL en NVS y la activa. Pasar String vacío para desactivar.
void gdriveSetUrl(const String& url);

// true si hay URL configurada y el módulo está activo
bool gdriveIsEnabled();
