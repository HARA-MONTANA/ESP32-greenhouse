#include "gdrive.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>

// ─── Estado interno ───────────────────────────────────────────────────────────

static String    g_url;           // URL del Apps Script webhook
static Preferences g_prefs;

// ─── NVS ─────────────────────────────────────────────────────────────────────

void gdriveInit() {
  g_prefs.begin("gdrive", true);   // true = solo lectura
  g_url = g_prefs.getString("url", "");
  g_prefs.end();

  if (g_url.length() > 0) {
    Serial.printf("[GDrive] Webhook configurado (%u chars)\n", g_url.length());
  } else {
    Serial.println("[GDrive] Sin URL configurada. Usa: /gdrive <url>");
  }
}

String gdriveGetUrl()    { return g_url; }
bool   gdriveIsEnabled() { return g_url.length() > 0; }

void gdriveSetUrl(const String& url) {
  g_url = url;
  g_prefs.begin("gdrive", false);  // lectura-escritura
  if (url.length() > 0) {
    g_prefs.putString("url", url);
  } else {
    g_prefs.remove("url");
  }
  g_prefs.end();
}

// ─── Envío de log ─────────────────────────────────────────────────────────────

void gdriveWriteLog(const char* tipo,
                    float tempC, float rh,
                    int soilPct, int mqRaw,
                    bool hasNumeric,
                    const char* detalle) {
  if (g_url.isEmpty()) return;
  if (WiFi.status() != WL_CONNECTED) return;

  // Construir timestamp local
  struct tm dt;
  time_t now = time(nullptr);
  localtime_r(&now, &dt);
  char ts[20];
  snprintf(ts, sizeof(ts), "%04d-%02d-%02d %02d:%02d:%02d",
           1900 + dt.tm_year, dt.tm_mon + 1, dt.tm_mday,
           dt.tm_hour, dt.tm_min, dt.tm_sec);

  // Construir payload JSON
  StaticJsonDocument<320> doc;
  doc["ts"]         = ts;
  doc["tipo"]       = tipo;
  doc["hasNumeric"] = hasNumeric;
  if (hasNumeric) {
    if (!isnan(tempC))  doc["temp"]  = String(tempC, 1);
    if (!isnan(rh))     doc["rh"]    = String(rh, 1);
    if (soilPct >= 0)   doc["suelo"] = soilPct;
    if (mqRaw >= 0)     doc["mq"]    = mqRaw;
  }
  if (strlen(detalle) > 0) doc["detalle"] = detalle;

  String body;
  serializeJson(doc, body);

  // HTTPS con redirección (Apps Script redirige a script.googleusercontent.com)
  WiFiClientSecure client;
  client.setInsecure();  // sin verificación de CA — aceptable para webhook interno

  HTTPClient http;
  http.begin(client, g_url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(6000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  int code = http.POST(body);
  if (code > 0) {
    Serial.printf("[GDrive] POST %s → HTTP %d\n", tipo, code);
  } else {
    Serial.printf("[GDrive] POST fallo (%s)\n", HTTPClient::errorToString(code).c_str());
  }
  http.end();
}
