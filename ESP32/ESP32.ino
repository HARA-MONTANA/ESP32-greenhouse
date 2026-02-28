#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <Wire.h>
#include <RTClib.h>
#include <sys/time.h>
#include <time.h>
#include <Preferences.h>
#include <vector>
#include <map>
#include <DHT.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <esp_wifi.h>

#include "pins.h"
#include "config.h"
#include "irrigation.h"
#include "dashboard.h"
#include <SD.h>
#include "sdcard.h"
#include "gdrive.h"

// =========================================================
//  VARIABLES GLOBALES
// =========================================================

// Credenciales activas y almacenadas (NVS namespace "cred")
Preferences credStore;
String wifiSsid;
String wifiPassword;
String telegramToken;
String storedWifiSsid;
String storedWifiPassword;
String storedTelegramToken;
std::vector<String> authorizedChatIds;

// Redes WiFi guardadas (hasta MAX_WIFI_NETWORKS; solo las que hayan conectado)
struct WifiNetwork { String ssid; String pass; };
const int MAX_WIFI_NETWORKS = 5;
std::vector<WifiNetwork> savedNetworks;

// Botón skip credenciales
bool skipCredentialPrompt = false;
bool credentialSkipNotified = false;
bool missingStoredCredsWarned = false;
volatile bool skipButtonFired = false;

// Telegram
WiFiClientSecure telegramClient;
UniversalTelegramBot *telegramBot = nullptr;
bool telegramEnabled = false;
String lastTelegramChatId;
String botName;
// Modo inscripcion: mientras sea true, cualquier chat desconocido que escriba
// queda autorizado automaticamente. Se cierra al llegar al limite de 5 IDs,
// con /acceso off, o al agotarse el temporizador opcional.
// No se persiste en NVS (vuelve a false tras cada reboot).
bool enrollmentOpen = false;
unsigned long enrollmentExpireMs = 0;  // 0 = sin expiración; > 0 = millis() de cierre

// Suscriptores de reportes periodicos (RAM, no persiste en NVS).
// Cada usuario elige con "reportes on/off" si quiere recibirlos,
// con su propio intervalo y formato. Los comandos del sistema siguen
// siendo globales; solo la entrega de mensajes es independiente.
struct ReportSub {
  unsigned long intervalMs = 30UL * 60 * 1000;  // 30 min por defecto
  bool          compact    = true;
  unsigned long lastMs     = 0;
};
std::map<String, ReportSub> reportSubscribers;

// RTC
RTC_DS3231 rtc;
bool rtcReady = false;

// DHT
DHT dht(PIN_DHT, DHT22);
float cachedTemp = NAN;
float cachedRh = NAN;
unsigned long lastDhtReadMs = 0;
bool dhtValid = false;

// Caché de ADC rápidos (suelo y MQ) — evita lecturas duplicadas en el mismo ciclo
int cachedSoilAdc = 0;
int cachedMqRaw   = 0;
unsigned long lastFastSensorMs = 0;

// Filtro de promedio movil para MQ-135 (8 muestras, ~16 s con ciclo 2 s)
// Reduce el ruido del sensor que puede generar alertas falsas.
const int MQ_AVG_SIZE = 8;
int mqSamples[MQ_AVG_SIZE] = {};
int mqSampleIdx = 0;
bool mqAvgReady = false;

// Fan PWM
const int FAN_PWM_CHANNEL = 0;
const int FAN_PWM_FREQ = 25000;
const int FAN_PWM_RES = 8;
const int FAN_PWM_MAX = (1 << FAN_PWM_RES) - 1;
bool fanAuto = true;
int fanPercent = 0;
int fanApplied = -1;

// LED morado PWM
const int LED_PWM_CHANNEL = 1;
const int LED_PWM_FREQ    = 1000;
const int LED_PWM_RES     = 8;
const int LED_PWM_MAX     = 255;
bool ledOn = false;  // true = sigue el horario de luz, false = apagado
volatile bool actuatorTestActive = false;  // true durante prueba de 5s: pausa applyLightSchedule

// Luces
const int LIGHTS_ON_HOUR = 6;
const int LIGHTS_ON_MINUTE = 0;

// Alertas (flags para no repetir)
bool alertWater = false;
bool alertTempHigh = false;
bool alertRhLow = false;
bool alertRhHigh = false;
bool alertMq = false;

// Contadores de lecturas consecutivas sin condición (para resetear flags)
const int ALERT_CLEAR_COUNT = 5;
int clearCountWater  = 0;
int clearCountTemp   = 0;
int clearCountRhLow  = 0;
int clearCountRhHigh = 0;
int clearCountMq     = 0;

// Timers del loop
unsigned long lastLightMs = 0;
unsigned long lastFanMs = 0;
unsigned long lastSoilMs = 0;
unsigned long lastTelegramMs = 0;
time_t nextSdLogEpoch = 0;   // epoch del próximo log alineado al reloj; 0 = no inicializado

// WiFi Modem Sleep — ahorra ~50-120mA durmiendo el radio entre polls de Telegram.
// Sensores, riego, luces y fan siguen activos (usan timers locales + RTC).
bool  wifiSleepMode      = false;               // false = operacion normal; true = sleep activo
unsigned long wifiSleepPollMs = 10000UL; // Intervalo de poll en sleep mode (10 s)
bool  wifiPsSleeping     = false;               // true si el modem esta en power-save ahora

// Web dashboard
AsyncWebServer webServer(80);
AsyncWebSocket wsEndpoint("/ws");

// Timezone string para POSIX
String tzPosix;

// Declaraciones anticipadas
void configureTimezone();
bool syncNtp(unsigned long maxWaitMs = 30000);
bool setTimeFromRtc();
void initWebServer();
void broadcastSensorData();

// =========================================================
//  UTILIDADES
// =========================================================

String stageToString(plantStage stage) {
  switch (stage) {
    case PLANTULA:      return "Plantula";
    case VEGETATIVO:    return "Vegetativo";
    case PRE_FLORACION: return "Pre-floracion";
    case FLORACION:     return "Floracion";
    case FINAL:         return "Final";
    default:            return "N/D";
  }
}

String stageToCode(plantStage stage) {
  switch (stage) {
    case PLANTULA:      return "pl";
    case VEGETATIVO:    return "veg";
    case PRE_FLORACION: return "pre";
    case FLORACION:     return "flo";
    case FINAL:         return "fin";
    default:            return "pl";
  }
}

String stageToShort(plantStage stage) {
  switch (stage) {
    case PLANTULA:      return "PL";
    case VEGETATIVO:    return "VEG";
    case PRE_FLORACION: return "PRE";
    case FLORACION:     return "FLO";
    case FINAL:         return "FIN";
    default:            return "N/D";
  }
}

plantStage stageFromString(const String &val) {
  String s = val;
  s.toLowerCase();
  if (s == "pl" || s == "plantula")      return PLANTULA;
  if (s == "veg" || s == "vegetativo")   return VEGETATIVO;
  if (s == "pre" || s == "prefloracion") return PRE_FLORACION;
  if (s == "flo" || s == "floracion")    return FLORACION;
  if (s == "fin" || s == "final")        return FINAL;
  return getCurrentStage();
}

bool readAmbient(float &tempC, float &rh) {
  unsigned long now = millis();
  if (dhtValid && now - lastDhtReadMs < 2000) {
    tempC = cachedTemp;
    rh = cachedRh;
    return true;
  }

  tempC = dht.readTemperature();
  rh = dht.readHumidity();

  if (isnan(tempC) || isnan(rh)) {
    delay(80);
    tempC = dht.readTemperature();
    rh = dht.readHumidity();
  }

  if (isnan(tempC) || isnan(rh)) return false;

  cachedTemp = tempC;
  cachedRh = rh;
  lastDhtReadMs = now;
  dhtValid = true;
  return true;
}

/// Lectura de ADC con caché de 500 ms: suelo y MQ leídos una sola vez por ciclo.
// El MQ-135 pasa por un promedio movil de MQ_AVG_SIZE muestras para reducir ruido.
void readFastSensors(int &soilAdc, int &mqRaw) {
  unsigned long now = millis();
  if (now - lastFastSensorMs < 500) {
    soilAdc = cachedSoilAdc;
    mqRaw   = cachedMqRaw;
    return;
  }
  cachedSoilAdc = soilAdc = readSoilMoisture();

  int rawMq = analogRead(PIN_MQ135);
  // Primera muestra: inicializar todo el array con la lectura actual para
  // evitar el periodo de arranque con valores en cero.
  if (!mqAvgReady && mqSampleIdx == 0) {
    for (int i = 0; i < MQ_AVG_SIZE; i++) mqSamples[i] = rawMq;
    mqAvgReady = true;
  }
  mqSamples[mqSampleIdx] = rawMq;
  mqSampleIdx = (mqSampleIdx + 1) % MQ_AVG_SIZE;
  long sum = 0;
  for (int i = 0; i < MQ_AVG_SIZE; i++) sum += mqSamples[i];
  cachedMqRaw = mqRaw = (int)(sum / MQ_AVG_SIZE);

  lastFastSensorMs = now;
}

bool parseOnOff(const String &val) {
  String s = val;
  s.toLowerCase();
  return s == "on" || s == "1" || s == "true";
}

String formatDateTime(const struct tm &t) {
  char buf[20];
  strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &t);
  return String(buf);
}

String formatLastIrrigation() {
  unsigned long epoch = getLastIrrigationEpoch();
  if (epoch == 0) return "Sin registro";
  struct tm t;
  time_t ts = static_cast<time_t>(epoch);
  if (!localtime_r(&ts, &t)) return "Sin registro";
  char buf[20];
  strftime(buf, sizeof(buf), "%H:%M %d/%m/%Y", &t);
  return String(buf);
}

// =========================================================
//  BROADCAST (simplificado, sin deduplicación)
// =========================================================

void broadcastMessage(const String &msg) {
  if (msg.isEmpty()) return;
  Serial.println(msg);

  if (!telegramEnabled || WiFi.status() != WL_CONNECTED || !telegramBot) return;

  for (const auto &id : authorizedChatIds) {
    telegramBot->sendMessage(id, msg, "");
  }
}

// =========================================================
//  WEB DASHBOARD — broadcast JSON por WebSocket
// =========================================================

void broadcastSensorData() {
  if (wsEndpoint.count() == 0) return;

  float tempC, rh;
  bool valid = readAmbient(tempC, rh);
  int  soilAdc, mqRaw;
  readFastSensors(soilAdc, mqRaw);
  int  soilPct = soilPercentFromAdc(soilAdc);
  time_t ts;
  time(&ts);

  StaticJsonDocument<768> doc;
  if (valid) {
    doc["temp_c"] = round(tempC * 10.0) / 10.0;
    doc["rh_pct"] = round(rh   * 10.0) / 10.0;
  } else {
    doc["temp_c"] = nullptr;
    doc["rh_pct"] = nullptr;
  }
  doc["soil_pct"]    = soilPct;
  doc["soil_adc"]    = soilAdc;
  doc["mq_raw"]      = mqRaw;
  doc["fan_rpm"]     = fanApplied;  // envia porcentaje (0-100); el campo conserva el nombre por compatibilidad con dashboard
  doc["temp_valid"]  = valid;
  doc["ts"]          = (long)ts;
  // System state
  doc["stage"]       = stageToCode(getCurrentStage());
  doc["fan_pct"]     = fanApplied;
  doc["fan_auto"]    = fanAuto;
  doc["light_on"]    = areLightsOn();
  doc["led_pct"]     = getLedIntensity();
  doc["led_on"]      = (ledcRead(LED_PWM_CHANNEL) > 0);
  doc["auto_irr"]    = isAutoIrrigationEnabled();
  doc["tank_ok"]     = isTankWaterAvailable();
  // Alerts
  doc["alert_temp"]  = alertTempHigh;
  doc["alert_rh"]    = alertRhLow || alertRhHigh;
  doc["alert_mq"]    = alertMq;
  doc["alert_water"] = alertWater;
  // Metadata
  doc["last_irr"]        = (long)getLastIrrigationEpoch();
  doc["pump_calibrated"] = isPumpCalibrated();
  doc["tz_offset"]       = getTimezoneOffsetHours();
  doc["bot_name"]        = botName;

  String payload;
  payload.reserve(384);
  serializeJson(doc, payload);
  wsEndpoint.textAll(payload);
}

void initWebServer() {
  webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send_P(200, "text/html", DASHBOARD_HTML);
  });

  wsEndpoint.onEvent([](AsyncWebSocket *server,
                         AsyncWebSocketClient *client,
                         AwsEventType type,
                         void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
      broadcastSensorData();  // enviar lectura inmediata al nuevo cliente
    } else if (type == WS_EVT_DATA) {
      String msg;
      msg.reserve(len);
      for (size_t i = 0; i < len; i++) msg += (char)data[i];
      StaticJsonDocument<128> cmd;
      DeserializationError jsonErr = deserializeJson(cmd, msg);
      if (jsonErr == DeserializationError::Ok) {
        String c = cmd["cmd"] | "";
        String a = cmd["args"] | "";
        if (!c.isEmpty()) {
          handleCommand("ws", a.length() ? c + " " + a : c);
          broadcastSensorData();
        }
      } else {
        Serial.printf("[WS] JSON invalido: %s\n", jsonErr.c_str());
      }
    }
  });

  // Ruta: listar meses y archivos de log en SD
  webServer.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest *req) {
    String json;
    if (!sdBuildLogIndex(json)) {
      req->send(503, "application/json", "{\"error\":\"SD no disponible\"}");
      return;
    }
    req->send(200, "application/json", json);
  });

  // Ruta: configuración actual como JSON
  webServer.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *req) {
    StaticJsonDocument<640> doc;
    doc["stage"]          = stageToCode(getCurrentStage());
    doc["pot_l"]          = getPotVolumeL();
    doc["ml_pl"]          = getMlPerLiterForStage(PLANTULA);
    doc["ml_veg"]         = getMlPerLiterForStage(VEGETATIVO);
    doc["ml_pre"]         = getMlPerLiterForStage(PRE_FLORACION);
    doc["ml_flo"]         = getMlPerLiterForStage(FLORACION);
    doc["ml_fin"]         = getMlPerLiterForStage(FINAL);
    doc["luz_pl"]         = getLightHoursForStage(PLANTULA);
    doc["luz_veg"]        = getLightHoursForStage(VEGETATIVO);
    doc["led_pct"]        = getLedIntensity();
    doc["soil_min_pct"]   = getSoilThreshold();
    doc["soil_max_pct"]   = getSoilHighThreshold();
    doc["soil_dry_adc"]   = getSoilDryAdc();
    doc["soil_wet_adc"]   = getSoilWetAdc();
    doc["temp_max"]       = getTempAlertThreshold();
    doc["hum_min"]        = getRhLowAlertThreshold();
    doc["hum_max"]        = getRhHighAlertThreshold();
    doc["mq_max"]         = getMqAlertThreshold();
    doc["tz_offset"]      = getTimezoneOffsetHours();
    doc["pump_calibrated"]= isPumpCalibrated();
    doc["bot_name"]       = botName;
    doc["gdrive_url"]     = gdriveGetUrl();
    doc["gdrive_enabled"] = gdriveIsEnabled();
    String json;
    serializeJson(doc, json);
    req->send(200, "application/json", json);
  });

  // Ruta: actualizar token y nombre del bot de Telegram
  webServer.on("/api/telegram", HTTP_POST,
    [](AsyncWebServerRequest *req) {},
    nullptr,
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
      StaticJsonDocument<384> doc;
      if (deserializeJson(doc, data, len) == DeserializationError::Ok) {
        String tok  = doc["token"] | "";
        String name = doc["name"]  | "";
        if (tok.length() > 0) {
          credStore.putString("token", tok);
          telegramToken = tok;
          storedTelegramToken = tok;
          if (telegramBot) {
            delete telegramBot;
            telegramBot = new UniversalTelegramBot(telegramToken, telegramClient);
          }
          telegramEnabled = (telegramBot != nullptr);
        }
        if (name.length() > 0) {
          credStore.putString("botname", name);
          botName = name;
        }
        req->send(200, "application/json", "{\"ok\":true}");
      } else {
        req->send(400, "application/json", "{\"error\":\"invalid json\"}");
      }
    }
  );

  // Ruta: configurar URL de Google Apps Script para logging en Drive
  webServer.on("/api/gdrive", HTTP_POST,
    [](AsyncWebServerRequest *req) {},
    nullptr,
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
      StaticJsonDocument<512> doc;
      if (deserializeJson(doc, data, len) == DeserializationError::Ok) {
        String url = doc["url"] | "";
        url.trim();
        if (url.length() > 0 && !url.startsWith("http")) {
          req->send(400, "application/json", "{\"error\":\"URL invalida\"}");
          return;
        }
        gdriveSetUrl(url);
        req->send(200, "application/json", "{\"ok\":true}");
      } else {
        req->send(400, "application/json", "{\"error\":\"invalid json\"}");
      }
    }
  );

  // Ruta: listar redes WiFi guardadas (sin contraseñas)
  webServer.on("/api/wifi", HTTP_GET, [](AsyncWebServerRequest *req) {
    StaticJsonDocument<320> doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < (int)savedNetworks.size(); i++) {
      JsonObject o = arr.createNestedObject();
      o["idx"]  = i;
      o["ssid"] = savedNetworks[i].ssid;
    }
    String json;
    serializeJson(doc, json);
    req->send(200, "application/json", json);
  });

  // Ruta: agregar red WiFi {ssid, pass}
  webServer.on("/api/wifi/add", HTTP_POST,
    [](AsyncWebServerRequest *req) {},
    nullptr,
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t) {
      StaticJsonDocument<256> doc;
      if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
        req->send(400, "application/json", "{\"error\":\"json invalido\"}"); return;
      }
      String ssid = doc["ssid"] | "";
      String pass = doc["pass"] | "";
      if (ssid.isEmpty()) {
        req->send(400, "application/json", "{\"error\":\"ssid requerido\"}"); return;
      }
      if (!addSavedNetwork(ssid, pass)) {
        req->send(409, "application/json", "{\"error\":\"lista llena (max 5)\"}"); return;
      }
      req->send(200, "application/json", "{\"ok\":true}");
    }
  );

  // Ruta: eliminar red WiFi {idx}
  webServer.on("/api/wifi/del", HTTP_POST,
    [](AsyncWebServerRequest *req) {},
    nullptr,
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t) {
      StaticJsonDocument<64> doc;
      if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
        req->send(400, "application/json", "{\"error\":\"json invalido\"}"); return;
      }
      int idx = doc["idx"] | -1;
      if (!deleteSavedNetworkAt(idx)) {
        req->send(404, "application/json", "{\"error\":\"indice invalido\"}"); return;
      }
      req->send(200, "application/json", "{\"ok\":true}");
    }
  );

  // Ruta: editar red WiFi existente {idx, ssid, pass}
  webServer.on("/api/wifi/edit", HTTP_POST,
    [](AsyncWebServerRequest *req) {},
    nullptr,
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t) {
      StaticJsonDocument<256> doc;
      if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
        req->send(400, "application/json", "{\"error\":\"json invalido\"}"); return;
      }
      int idx     = doc["idx"]  | -1;
      String ssid = doc["ssid"] | "";
      String pass = doc["pass"] | "";
      if (idx < 0 || idx >= (int)savedNetworks.size() || ssid.isEmpty()) {
        req->send(400, "application/json", "{\"error\":\"datos invalidos\"}"); return;
      }
      savedNetworks[idx].ssid = ssid;
      savedNetworks[idx].pass = pass;
      saveSavedNetworks();
      req->send(200, "application/json", "{\"ok\":true}");
    }
  );

  // Ruta: servir o descargar un archivo CSV de log
  // ?path=/logs/MM/YYYY-MM-DD.csv  [&dl=1 para descarga]
  webServer.on("/api/logfile", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!req->hasParam("path")) {
      req->send(400, "application/json", "{\"error\":\"Falta parametro path\"}");
      return;
    }
    String safe = sdValidateLogPath(req->getParam("path")->value());
    if (safe.isEmpty()) {
      req->send(404, "application/json", "{\"error\":\"Archivo no encontrado\"}");
      return;
    }
    bool dl = req->hasParam("dl") && req->getParam("dl")->value() == "1";
    req->send(SD, safe.c_str(), "text/csv", dl);
  });

  webServer.addHandler(&wsEndpoint);
  webServer.begin();
  Serial.println("[WEB] Dashboard en http://" + WiFi.localIP().toString());
}

// =========================================================
//  FAN CONTROL
// =========================================================

void initFan() {
  ledcSetup(FAN_PWM_CHANNEL, FAN_PWM_FREQ, FAN_PWM_RES);
  ledcAttachPin(PIN_FAN_PWM, FAN_PWM_CHANNEL);
  ledcWrite(FAN_PWM_CHANNEL, 0);
  fanApplied = 0;
}

int computeAutoFan() {
  float tempC, rh;
  bool ok = readAmbient(tempC, rh);
  int mqReading = cachedMqRaw;  // usa el promedio movil filtrado

  int tempPct = 0;
  if (ok && getTempAlertThreshold() > 0) {
    float cool = 24.0f;
    float hot = max(cool + 1.0f, static_cast<float>(getTempAlertThreshold()));
    if (tempC > cool) {
      tempPct = tempC >= hot ? 100 : constrain((int)((tempC - cool) / (hot - cool) * 100.0f + 0.5f), 0, 100);
    }
  }

  int mqPct = 0;
  int mqTh = getMqAlertThreshold();
  int mqStart = max(0, mqTh - 50);
  int mqMax = min(4095, mqTh + 400);
  if (mqReading >= mqStart) {
    mqPct = mqReading >= mqMax ? 100 : map(mqReading, mqStart, mqMax, 0, 100);
  }

  return max(tempPct, mqPct);
}

void updateFan(bool force = false) {
  int target = fanAuto ? computeAutoFan() : fanPercent;
  target = constrain(target, 0, 100);

  if (force || target != fanApplied) {
    ledcWrite(FAN_PWM_CHANNEL, map(target, 0, 100, 0, FAN_PWM_MAX));
    fanApplied = target;
    fanPercent = target;
  }
}

// =========================================================
//  LUCES
// =========================================================

bool areLightsOn() { return digitalRead(PIN_ACLIGHT) == LOW; }

String formatLightsOffTime() {
  int offMin = (LIGHTS_ON_HOUR * 60 + LIGHTS_ON_MINUTE + getLightHoursForStage(getCurrentStage()) * 60) % 1440;
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", offMin / 60, offMin % 60);
  return String(buf);
}

bool stageUsesLeds(plantStage s) {
  return s == PRE_FLORACION || s == FLORACION || s == FINAL;
}

// Apaga el LED de forma gradual para evitar reinicios por picos de corriente
void fadeLedOff() {
  int current = (int)ledcRead(LED_PWM_CHANNEL);
  if (current == 0) return;
  actuatorTestActive = true;
  for (int d = current; d >= 0; d -= 3) {
    ledcWrite(LED_PWM_CHANNEL, max(d, 0));
    delay(20);
  }
  ledcWrite(LED_PWM_CHANNEL, 0);
  actuatorTestActive = false;
}

void applyLightSchedule() {
  if (actuatorTestActive) return;  // pausa durante prueba de actuador
  struct tm now;
  if (!getLocalTime(&now)) return;

  struct tm start = now;
  start.tm_hour = LIGHTS_ON_HOUR;
  start.tm_min = LIGHTS_ON_MINUTE;
  start.tm_sec = 0;

  time_t nowTs = mktime(&now);
  time_t startTs = mktime(&start);
  plantStage stage = getCurrentStage();
  time_t offTs = startTs + getLightHoursForStage(stage) * 3600L;

  bool shouldBeOn = nowTs >= startTs && nowTs < offTs;
  bool ledShouldBeOn = shouldBeOn && ledOn;  // LED sigue el horario de luz, sin restriccion de fase

  static bool prevLightOn = false;
  if (shouldBeOn != prevLightOn) {
    prevLightOn = shouldBeOn;
    logAccion(shouldBeOn ? "LUZ_ON" : "LUZ_OFF",
              "Etapa: " + stageToString(stage) + "; " +
              String(getLightHoursForStage(stage)) + "h programadas");
  }

  digitalWrite(PIN_ACLIGHT, shouldBeOn ? LOW : HIGH);

  static bool prevLedShouldBeOn = false;
  if (ledShouldBeOn) {
    int duty = map(getLedIntensity(), 0, 100, 0, LED_PWM_MAX);
    ledcWrite(LED_PWM_CHANNEL, duty);
  } else {
    if (prevLedShouldBeOn) {
      fadeLedOff();  // Apagado gradual al detectar transición ON→OFF
    }
  }
  prevLedShouldBeOn = ledShouldBeOn;
}

// =========================================================
//  ALERTAS
// =========================================================

void evaluateAlerts() {
  // Leer todos los sensores una sola vez al inicio para que estén disponibles
  // en todos los bloques de alerta (incluido el de agua, que va primero)
  float tempC, rh;
  bool ok = readAmbient(tempC, rh);
  int soilAdc, mqRaw;
  readFastSensors(soilAdc, mqRaw);
  int soilPct = soilPercentFromAdc(soilAdc);
  plantStage stage = getCurrentStage();

  bool water = isTankWaterAvailable();
  if (!water && !alertWater) {
    broadcastMessage("🚨 ALERTA: tanque sin agua");
    logAccionConSensores("ALERTA_ON", "tanque sin agua",
                         ok ? tempC : NAN, ok ? rh : NAN, soilPct, mqRaw);
    alertWater = true;
    clearCountWater = 0;
  } else if (!water && alertWater) {
    clearCountWater = 0;
  } else if (alertWater) {
    if (++clearCountWater >= ALERT_CLEAR_COUNT) {
      alertWater = false;
      clearCountWater = 0;
      logAccion("ALERTA_OFF", "tanque con agua");
    }
  }

  if (ok) {
    bool tempHigh = getTempAlertThreshold() > 0 && tempC >= getTempAlertThreshold();
    if (tempHigh && !alertTempHigh) {
      broadcastMessage("🚨🌡️ ALERTA: temp alta (" + String(tempC, 1) + "C >= " + String(getTempAlertThreshold()) + "C)");
      logAccionConSensores("ALERTA_ON",
                           "temp alta (" + String(tempC, 1) + "C >= " + String(getTempAlertThreshold()) + "C)",
                           tempC, rh, soilPct, mqRaw);
      alertTempHigh = true;
      clearCountTemp = 0;
    } else if (tempHigh && alertTempHigh) {
      clearCountTemp = 0;
    } else if (alertTempHigh) {
      if (++clearCountTemp >= ALERT_CLEAR_COUNT) {
        alertTempHigh = false;
        clearCountTemp = 0;
        logAccion("ALERTA_OFF", "temp alta resuelta");
      }
    }

    bool lowRhStage = (stage == PLANTULA || stage == VEGETATIVO);
    bool rhLow = lowRhStage && getRhLowAlertThreshold() > 0 && rh < getRhLowAlertThreshold();
    if (rhLow && !alertRhLow) {
      broadcastMessage("🚨💧 ALERTA: humedad baja (" + String(rh, 0) + "% < " + String(getRhLowAlertThreshold()) + "%)");
      logAccionConSensores("ALERTA_ON",
                           "humedad baja (" + String(rh, 0) + "% < " + String(getRhLowAlertThreshold()) + "%)",
                           tempC, rh, soilPct, mqRaw);
      alertRhLow = true;
      clearCountRhLow = 0;
    } else if (rhLow && alertRhLow) {
      clearCountRhLow = 0;
    } else if (alertRhLow) {
      if (++clearCountRhLow >= ALERT_CLEAR_COUNT) {
        alertRhLow = false;
        clearCountRhLow = 0;
        logAccion("ALERTA_OFF", "humedad baja resuelta");
      }
    }

    bool highRhStage = (stage == PRE_FLORACION || stage == FLORACION || stage == FINAL);
    bool rhHigh = highRhStage && getRhHighAlertThreshold() > 0 && rh > getRhHighAlertThreshold();
    if (rhHigh && !alertRhHigh) {
      broadcastMessage("🚨💧 ALERTA: humedad alta (" + String(rh, 0) + "% > " + String(getRhHighAlertThreshold()) + "%)");
      logAccionConSensores("ALERTA_ON",
                           "humedad alta (" + String(rh, 0) + "% > " + String(getRhHighAlertThreshold()) + "%)",
                           tempC, rh, soilPct, mqRaw);
      alertRhHigh = true;
      clearCountRhHigh = 0;
    } else if (rhHigh && alertRhHigh) {
      clearCountRhHigh = 0;
    } else if (alertRhHigh) {
      if (++clearCountRhHigh >= ALERT_CLEAR_COUNT) {
        alertRhHigh = false;
        clearCountRhHigh = 0;
        logAccion("ALERTA_OFF", "humedad alta resuelta");
      }
    }
  } else {
    alertTempHigh = false;
    alertRhLow = false;
    alertRhHigh = false;
    clearCountTemp = 0;
    clearCountRhLow = 0;
    clearCountRhHigh = 0;
  }

  bool poorAir = getMqAlertThreshold() > 0 && mqRaw >= getMqAlertThreshold();
  if (poorAir && !alertMq) {
    broadcastMessage("🚨💨 ALERTA: aire pobre (MQ=" + String(mqRaw) + " >= " + String(getMqAlertThreshold()) + ")");
    logAccionConSensores("ALERTA_ON",
                         "aire pobre (MQ=" + String(mqRaw) + " >= " + String(getMqAlertThreshold()) + ")",
                         ok ? tempC : NAN, ok ? rh : NAN, soilPct, mqRaw);
    alertMq = true;
    clearCountMq = 0;
  } else if (poorAir && alertMq) {
    clearCountMq = 0;
  } else if (alertMq) {
    if (++clearCountMq >= ALERT_CLEAR_COUNT) {
      alertMq = false;
      clearCountMq = 0;
      logAccion("ALERTA_OFF", "calidad de aire recuperada");
    }
  }
}

// =========================================================
//  FORMATO DE ESTADO
// =========================================================

// Formato compacto para smartwatch (lineas cortas ~14 chars)
String formatStatusCompact() {
  float temp, rh;
  bool ok = readAmbient(temp, rh);
  int soilAdc, mq;
  readFastSensors(soilAdc, mq);
  int soilPct = soilPercentFromAdc(soilAdc);
  bool water = isTankWaterAvailable();

  String lastRiego = formatLastIrrigation();
  String riegoHora = (lastRiego == "Sin registro") ? "--:--" : lastRiego.substring(0, 5);

  bool ledPhysOn = ledcRead(LED_PWM_CHANNEL) > 0;

  String s;
  s += "ESTADO\n";
  s += "T:" + (ok ? String(temp, 1) + "C" : String("N/D"));
  s += " HR:" + (ok ? String(rh, 0) + "%" : String("N/D")) + "\n";
  s += "MQ:" + String(mq) + " S:" + String(soilPct) + "%\n";
  s += "💨 Fan:" + String(fanPercent) + "% [" + (fanAuto ? "A" : "M") + "]\n";
  s += "☀️ Luz:" + String(areLightsOn() ? "ON" : "OFF") + "->" + formatLightsOffTime() + "\n";
  s += "🟣 LED:" + (ledPhysOn ? String(getLedIntensity()) + "%" : String("OFF")) + "\n";
  s += "Riego:" + riegoHora + " [" + String(isAutoIrrigationEnabled() ? "A" : "M") + "]\n";
  s += "Agua:" + String(water ? "SI" : "NO") + " [" + stageToShort(getCurrentStage()) + "]";
  return s;
}

// Formato completo para Telegram en celular
String formatStatusFull() {
  float temp, rh;
  bool ok = readAmbient(temp, rh);
  int soilAdc, mq;
  readFastSensors(soilAdc, mq);
  int soilPct = soilPercentFromAdc(soilAdc);
  bool water = isTankWaterAvailable();
  float stageMl = getMlPerLiterForStage(getCurrentStage()) * getPotVolumeL();
  bool ledPhysOn = ledcRead(LED_PWM_CHANNEL) > 0;

  String s;
  s += "🌿 ESTADO\n";
  s += "🌡️ Temp: " + (ok ? String(temp, 1) + "C" : String("N/D"));
  s += "   💧 HR: " + (ok ? String(rh, 0) + "%" : String("N/D")) + "\n";
  s += "💨 Aire: " + String(mq) + "   🌱 Suelo: " + String(soilPct) + "%\n";
  s += "🌬️ Fan: " + String(fanPercent) + "%  [" + (fanAuto ? "AUTO" : "MANUAL") + "]\n";
  s += "☀️ Luz: " + String(areLightsOn() ? "ON" : "OFF") + "   apaga " + formatLightsOffTime() + "\n";
  s += "🟣 LED: " + (ledPhysOn ? String(getLedIntensity()) + "%" : String("OFF")) + "\n";
  s += "💧 Riego: " + formatLastIrrigation() + "\n";
  s += "🤖 Auto: " + String(isAutoIrrigationEnabled() ? "ON" : "OFF");
  s += "   " + String(stageMl, 0) + " mL prog.\n";
  s += "🪣 Agua: " + String(water ? "SI" : "NO");
  s += "   🌱 Etapa: " + stageToString(getCurrentStage());
  return s;
}

String formatConfig() {
  struct tm t;
  String now = getLocalTime(&t) ? formatDateTime(t) : "Sin hora";
  float potL = getPotVolumeL();
  plantStage stage = getCurrentStage();
  float stageMl = getMlPerLiterForStage(stage) * potL;

  String s;
  s += "⚙️ CONFIGURACION\n";
  s += "🌱 Etapa: " + stageToString(stage) + "\n";
  s += "🪴 Maceta: " + String(potL, 1) + " L   " + String(stageMl, 0) + " mL/riego\n";
  s += "🌱 Suelo: min " + String(getSoilThreshold()) + "%   max " + String(getSoilHighThreshold()) + "%\n";
  s += "💧 mL/L: PL=" + String(getMlPerLiterForStage(PLANTULA));
  s += " VEG=" + String(getMlPerLiterForStage(VEGETATIVO));
  s += " PRE=" + String(getMlPerLiterForStage(PRE_FLORACION));
  s += " FLO=" + String(getMlPerLiterForStage(FLORACION));
  s += " FIN=" + String(getMlPerLiterForStage(FINAL)) + "\n";
  s += "🚨 Alertas:\n";
  s += "  🌡️ Temp > " + String(getTempAlertThreshold()) + "C\n";
  s += "  💧 HR: " + String(getRhLowAlertThreshold()) + "% - " + String(getRhHighAlertThreshold()) + "%\n";
  s += "  💨 Aire > " + String(getMqAlertThreshold()) + "\n";
  s += "🕐 Hora: " + now;
  return s;
}

// =========================================================
//  COMANDO UNIFICADO (Serial + Telegram)
// =========================================================

String commandHelp() {
  String h;
  h += "🌿 Uso diario\n";
  h += "/estado - Estado actual\n";
  h += "/ip - Direccion IP actual\n";
  h += "/regar [mL] - Riego manual\n";
  h += "/autoriego [on|off] - Riego automatico\n";
  h += "/vent [0-100] - Ventilador manual\n";
  h += "/ventauto [on|off] - Ventilador automatico\n";
  h += "/reportes [on|off] [min] [compacto|completo]\n";
  h += "\n⚙️ Configuracion\n";
  h += "/config /ajustes - Ver ajustes\n";
  h += "/etapa [pl|veg|pre|flo|fin]\n";
  h += "/maceta [litros]\n";
  h += "/ml [etapa] [valor]\n";
  h += "/luz [etapa] [horas]\n";
  h += "/led [on|off|0-100] - LED morado (sigue horario de luz)\n";
  h += "/suelomin [%] / /suelomax [%]\n";
  h += "/calsuelo [SECO] [HUMEDO]\n";
  h += "/timezone [offset]\n";
  h += "\n🚨 Alertas\n";
  h += "/tempmax [C] / /hummin [%] / /hummax [%] / /airemax [N]\n";
  h += "\n🔧 Bomba\n";
  h += "/calibrar - Bomba 5s para medir\n";
  h += "/caudal [mL] - Guardar volumen medido\n";
  h += "\n🔐 Admin\n";
  h += "/acceso [on|off] - Abrir/cerrar inscripcion (toggle sin argumento)\n";
  h += "/addid [ID] / /delid [ID] / /ids\n";
  h += "/reset - Restablecer configuracion\n";
  h += "/dormir [on|off] [min] - Sleep WiFi (ahorra energia entre polls)\n";
  h += "/gdrive [<url>|off] - Logging a Google Drive via Apps Script";
  return h;
}

String handleCommand(const String &chatId, const String &raw) {
  String line = raw;
  line.trim();
  if (line.isEmpty()) return "";

  int sp = line.indexOf(' ');
  String cmd = sp == -1 ? line : line.substring(0, sp);
  String args = sp == -1 ? "" : line.substring(sp + 1);
  cmd.toLowerCase();
  args.trim();

  // --- Uso diario ---

  if (cmd == "start" || cmd == "ayuda" || cmd == "help") {
    return commandHelp();
  }

  if (cmd == "estado" || cmd == "status") {
    return formatStatusFull();
  }

  if (cmd == "ip") {
    String ip = WiFi.localIP().toString();
    String ssid = WiFi.SSID();
    return "📡 IP: " + ip + "\n🌐 Red: " + ssid;
  }

  if (cmd == "config" || cmd == "conf" || cmd == "ajustes") {
    return formatConfig();
  }

  if (cmd == "regar") {
    float ml = args.toFloat();
    if (ml <= 0) return "⚠️ Uso: regar [mL]";
    ml = min(ml, 1500.0f);
    if (!isPumpCalibrated()) return "⚠️ Bomba sin calibrar. Usa: calibrar";
    if (!isTankWaterAvailable()) return "🪣 Tanque sin agua.";
    irrigateVolume(ml, readSoilMoisture());
    return "💧 Riego manual: " + String(ml) + " mL";
  }

  if (cmd == "autoriego") {
    if (args.isEmpty()) return String("💧 Riego auto: ") + (isAutoIrrigationEnabled() ? "ON" : "OFF");
    bool enable = parseOnOff(args);
    setAutoIrrigationEnabled(enable);
    logAccion("CMD", String("autoriego ") + (enable ? "on" : "off"));
    return String("💧 Riego auto ") + (isAutoIrrigationEnabled() ? "activado ✅" : "desactivado");
  }

  if (cmd == "vent") {
    int pct = constrain(args.toInt(), 0, 100);
    fanPercent = pct;
    fanAuto = false;
    updateFan(true);
    logAccion("VENT", "manual " + String(pct) + "%");
    return "🌬️ Fan manual: " + String(pct) + "%";
  }

  if (cmd == "ventauto") {
    if (args.isEmpty()) return String("🌬️ Fan auto: ") + (fanAuto ? "ON" : "OFF");
    fanAuto = parseOnOff(args);
    updateFan(true);
    logAccion("VENT", fanAuto ? "auto activado" : "auto desactivado");
    return String("🌬️ Fan auto ") + (fanAuto ? "ON ✅" : "OFF");
  }

  if (cmd == "reportes") {
    if (chatId.isEmpty()) return "📊 Reportes solo disponibles via Telegram.";
    bool sub = reportSubscribers.count(chatId) > 0;
    if (args.isEmpty()) {
      if (!sub) return "📊 Tus reportes: OFF";
      ReportSub &r = reportSubscribers[chatId];
      return String("📊 Tus reportes: ON | ") + String(r.intervalMs / 60000) + " min" +
             " | Modo: " + (r.compact ? "compacto" : "completo");
    }
    bool enabled = parseOnOff(args);

    // Parsear opciones: [<minutos>] [<compacto|completo>]
    // Toma los valores actuales del usuario (o defaults si es nuevo)
    ReportSub &r = reportSubscribers[chatId];  // crea con defaults si no existe
    int sp2 = args.indexOf(' ');
    while (sp2 >= 0) {
      int sp3 = args.indexOf(' ', sp2 + 1);
      String tok = sp3 == -1 ? args.substring(sp2 + 1) : args.substring(sp2 + 1, sp3);
      tok.trim();
      String tokL = tok; tokL.toLowerCase();
      if      (tokL == "compacto") r.compact = true;
      else if (tokL == "completo") r.compact = false;
      else if (tok.toInt() > 0)   r.intervalMs = tok.toInt() * 60000UL;
      sp2 = sp3;
    }
    if (!enabled) {
      reportSubscribers.erase(chatId);
      saveReportSubscribers();
      return "📊 Tus reportes: OFF";
    }
    saveReportSubscribers();
    return String("📊 Tus reportes: ON ✅ | ") + String(r.intervalMs / 60000) + " min" +
           " | Modo: " + (r.compact ? "compacto" : "completo");
  }

  // --- Configuración ---

  if (cmd == "etapa" || cmd == "stage") {
    if (args.isEmpty()) return "⚠️ Uso: etapa [pl|veg|pre|flo|fin]";
    updateStage(stageFromString(args));
    applyLightSchedule();
    configSave();
    logAccion("CMD", "etapa " + stageToString(getCurrentStage()));
    return "🌱 Etapa: " + stageToString(getCurrentStage());
  }

  if (cmd == "maceta") {
    float l = args.toFloat();
    if (l <= 0) return "⚠️ Uso: maceta [litros]";
    if (!setPotVolumeL(l)) return "❌ Rango: 1-50 L";
    return "🪴 Maceta: " + String(l, 1) + " L";
  }

  if (cmd == "ml") {
    int sp2 = args.indexOf(' ');
    if (sp2 == -1) return "⚠️ Uso: ml [etapa] [valor]";
    plantStage stage = stageFromString(args.substring(0, sp2));
    int val = args.substring(sp2 + 1).toInt();
    if (!setMlPerLiterForStage(stage, val)) return "❌ Rango mL/L: 5-200";
    return "💧 mL/L " + args.substring(0, sp2) + ": " + String(val);
  }

  if (cmd == "luz") {
    int sp2 = args.indexOf(' ');
    if (sp2 == -1) return "⚠️ Uso: luz [etapa] [horas]";
    plantStage stage = stageFromString(args.substring(0, sp2));
    int hours = args.substring(sp2 + 1).toInt();
    if (stage == PRE_FLORACION || stage == FLORACION || stage == FINAL) {
      return "☀️ Pre/flo/fin fijas en 12h.";
    }
    if (!setLightHoursForStage(stage, hours)) return "❌ Rango: 12-20 h";
    return "☀️ Luz " + args.substring(0, sp2) + ": " + String(hours) + " h";
  }

  if (cmd == "led") {
    if (args.isEmpty()) {
      return String("🟣 LED: ") + (ledOn ? "ON" : "OFF") +
             " | Brillo: " + String(getLedIntensity()) + "%";
    }
    int pct = args.toInt();
    if (args == String(pct) && pct >= 0 && pct <= 100) {
      if (pct > 0) setLedIntensity(pct);
      ledOn = (pct > 0);
      applyLightSchedule();
      return String("🟣 LED: ") + (ledOn ? String(pct) + "% ✅" : "apagado");
    }
    ledOn = parseOnOff(args);
    applyLightSchedule();
    return String("🟣 LED: ") + (ledOn ? "encendido ✅" : "apagado");
  }

  if (cmd == "suelomin") {
    int pct = args.toInt();
    if (!setSoilThreshold(pct)) return "❌ Rango: 0-50%";
    return "🌱 Suelo min: " + String(pct) + "%";
  }

  if (cmd == "suelomax") {
    int pct = args.toInt();
    if (!setSoilHighThreshold(pct)) return "❌ Rango: 50-100%";
    return "🌱 Suelo max: " + String(pct) + "%";
  }

  if (cmd == "calsuelo") {
    int sp2 = args.indexOf(' ');
    if (sp2 == -1) return "⚠️ Uso: calsuelo [SECO] [HUMEDO]";
    int dry = args.substring(0, sp2).toInt();
    int wet = args.substring(sp2 + 1).toInt();
    if (!setSoilCalibration(dry, wet)) {
      return "❌ Calibracion invalida: seco debe superar a humedo en al menos 100 unidades ADC (seco=" +
             String(dry) + " humedo=" + String(wet) + ")";
    }
    return "🌱 Calibracion suelo: seco=" + String(getSoilDryAdc()) + " humedo=" + String(getSoilWetAdc());
  }

  if (cmd == "timezone" || cmd == "tz") {
    if (args.isEmpty()) return "🕐 Timezone actual: UTC" + String(getTimezoneOffsetHours());
    int offset = constrain(args.toInt(), -12, 14);
    setTimezoneOffsetHours(offset);
    configureTimezone();
    syncNtp();
    return "🕐 Timezone: UTC" + String(offset);
  }

  // --- Alertas ---

  if (cmd == "tempmax") {
    int val = args.toInt();
    if (val <= 0) return "⚠️ Uso: tempmax [C]";
    setTempAlertThreshold(val);
    return "🌡️ Alerta temp: " + String(getTempAlertThreshold()) + "C";
  }

  if (cmd == "hummin") {
    int val = args.toInt();
    if (val <= 0) return "⚠️ Uso: hummin [%]";
    setRhLowAlertThreshold(val);
    return "💧 Alerta HR baja: " + String(getRhLowAlertThreshold()) + "%";
  }

  if (cmd == "hummax") {
    int val = args.toInt();
    if (val <= 0) return "⚠️ Uso: hummax [%]";
    setRhHighAlertThreshold(val);
    return "💧 Alerta HR alta: " + String(getRhHighAlertThreshold()) + "%";
  }

  if (cmd == "airemax") {
    int val = args.toInt();
    if (val <= 0) return "⚠️ Uso: airemax [N]";
    setMqAlertThreshold(val);
    return "💨 Alerta MQ: " + String(getMqAlertThreshold());
  }

  // --- Bomba ---

  if (cmd == "calibrar") {
    pumpOn();
    delay(5000);
    pumpOff();
    return "🔧 Bomba activada 5s. Mide el volumen y envia: caudal [mL]";
  }

  // --- Prueba de actuadores (5 s cada uno) ---

  if (cmd == "test_act") {
    if (args == "bomba") {
      pumpOn();
      delay(5000);
      pumpOff();
      return "✅ Bomba: prueba 5s OK";
    }
    if (args == "led") {
      actuatorTestActive = true;
      // Subida gradual: 0→max en ~2 s (pasos de 5, 40 ms c/u)
      for (int d = 0; d <= LED_PWM_MAX; d += 5) {
        ledcWrite(LED_PWM_CHANNEL, min(d, (int)LED_PWM_MAX));
        delay(40);
      }
      ledcWrite(LED_PWM_CHANNEL, LED_PWM_MAX);
      delay(900);  // Mantener al máximo ~1 s
      // Bajada gradual: max→0 en ~2 s
      for (int d = LED_PWM_MAX; d >= 0; d -= 5) {
        ledcWrite(LED_PWM_CHANNEL, max(d, 0));
        delay(40);
      }
      ledcWrite(LED_PWM_CHANNEL, 0);
      actuatorTestActive = false;
      applyLightSchedule();
      return "✅ LED: prueba OK";
    }
    if (args == "luz") {
      actuatorTestActive = true;
      bool wasOn = areLightsOn();
      digitalWrite(PIN_ACLIGHT, LOW);   // LOW = relé ON
      delay(5000);
      digitalWrite(PIN_ACLIGHT, wasOn ? LOW : HIGH);
      actuatorTestActive = false;
      return "✅ Luz AC: prueba 5s OK";
    }
    if (args == "fan") {
      bool prevAuto = fanAuto;
      int  prevPct  = fanPercent;
      fanAuto = false;
      // Subida gradual: 0→100% en ~2 s (pasos de 5%, 100 ms c/u)
      for (int p = 0; p <= 100; p += 5) {
        ledcWrite(FAN_PWM_CHANNEL, map(p, 0, 100, 0, FAN_PWM_MAX));
        delay(100);
      }
      delay(900);  // Mantener al máximo ~1 s
      // Bajada gradual: 100%→0 en ~2 s
      for (int p = 100; p >= 0; p -= 5) {
        ledcWrite(FAN_PWM_CHANNEL, map(p, 0, 100, 0, FAN_PWM_MAX));
        delay(100);
      }
      ledcWrite(FAN_PWM_CHANNEL, 0);
      fanAuto    = prevAuto;
      fanPercent = prevPct;
      updateFan(true);
      return "✅ Fan: prueba OK";
    }
    return "⚠️ Uso: test_act [bomba|led|luz|fan]";
  }

  if (cmd == "caudal") {
    float ml = args.toFloat();
    if (ml <= 0) return "⚠️ Uso: caudal [mL medidos]";
    float flow = ml / 5.0f;
    if (!setPumpFlow(flow)) return "❌ Caudal fuera de rango (1-50 mL/s)";
    setAutoIrrigationEnabled(false);
    return "🔧 Caudal: " + String(flow, 1) + " mL/s. Usa: autoriego on";
  }

  // --- Admin ---

  if (cmd == "addid") {
    if (authorizedChatIds.size() >= 5) return "❌ Maximo 5 IDs.";
    if (args.isEmpty()) return "⚠️ Uso: addid [ID]";
    authorizedChatIds.push_back(args);
    persistChatIds();
    return "✅ ID agregado.";
  }

  if (cmd == "delid") {
    if (args.isEmpty()) return "⚠️ Uso: delid [ID]";
    for (auto it = authorizedChatIds.begin(); it != authorizedChatIds.end(); ++it) {
      if (*it == args) {
        authorizedChatIds.erase(it);
        persistChatIds();
        return "🗑️ ID eliminado.";
      }
    }
    return "❌ ID no encontrado.";
  }

  if (cmd == "ids") {
    String r = "🔑 IDs autorizados:\n";
    for (size_t i = 0; i < authorizedChatIds.size(); i++) {
      r += String(i + 1) + ": " + authorizedChatIds[i] + "\n";
    }
    return r;
  }

  if (cmd == "acceso") {
    // Caso numérico: /acceso [minutos] — abre el modo con temporizador (máx 60 min)
    int minutes = args.toInt();
    if (minutes > 0) {
      if ((int)authorizedChatIds.size() >= 5)
        return "❌ Maximo de IDs alcanzado (5). Elimina uno con /delid antes de abrir acceso.";
      minutes = min(minutes, 60);
      enrollmentOpen     = true;
      enrollmentExpireMs = millis() + (unsigned long)minutes * 60000UL;
      int slots = 5 - (int)authorizedChatIds.size();
      return "🔓 Modo inscripcion ACTIVO por " + String(minutes) + " min. (" +
             String(slots) + " lugar(es) disponibles). Cierra solo o con /acceso off.";
    }
    if (args == "on") {
      if ((int)authorizedChatIds.size() >= 5) return "❌ Maximo de IDs alcanzado (5). Elimina uno con /delid antes de abrir acceso.";
      enrollmentOpen     = true;
      enrollmentExpireMs = 0;  // sin expiracion
    } else if (args == "off") {
      enrollmentOpen     = false;
      enrollmentExpireMs = 0;
    } else {
      // toggle
      if (!enrollmentOpen && (int)authorizedChatIds.size() >= 5)
        return "❌ Maximo de IDs alcanzado (5). Elimina uno con /delid antes de abrir acceso.";
      enrollmentOpen = !enrollmentOpen;
      if (!enrollmentOpen) enrollmentExpireMs = 0;
    }
    if (enrollmentOpen) {
      int slots = 5 - (int)authorizedChatIds.size();
      return "🔓 Modo inscripcion ACTIVO (sin limite de tiempo). (" +
             String(slots) + " lugar(es) disponibles). Cierra con /acceso off.";
    }
    return "🔒 Modo inscripcion CERRADO. Solo IDs autorizados pueden interactuar.";
  }

  if (cmd == "reset") {
    logAccion("CMD", "reset de configuracion");
    configReset();
    return "♻️ Configuracion restablecida.";
  }

  // --- Sleep WiFi ---

  if (cmd == "dormir" || cmd == "sleep") {
    if (args.isEmpty()) {
      if (!wifiSleepMode) return "😴 Sleep WiFi: OFF (WiFi siempre activo)";
      return "😴 Sleep WiFi: ON | Poll Telegram cada " + String(wifiSleepPollMs / 60000) + " min\n"
             "Modem duerme entre polls. Sensores/riego/luces: OK.";
    }
    int sp2 = args.indexOf(' ');
    String sw = sp2 == -1 ? args : args.substring(0, sp2);
    sw.toLowerCase();
    if (sw == "off") {
      wifiSleepMode = false;
      setWifiPowerSave(false);
      return "😴 Sleep WiFi: OFF. WiFi en modo normal.";
    }
    if (sw == "on") {
      if (sp2 != -1) {
        int mins = constrain(args.substring(sp2 + 1).toInt(), 1, 60);
        if (mins > 0) wifiSleepPollMs = (unsigned long)mins * 60000UL;
      }
      wifiSleepMode = true;
      return "😴 Sleep WiFi: ON ✅ | Telegram cada " + String(wifiSleepPollMs / 60000) + " min\n"
             "Modem duerme entre polls. Sensores/riego/luces siguen activos.";
    }
    return "⚠️ Uso: dormir [on|off] [minutos 1-60]";
  }

  // --- Google Drive ---

  if (cmd == "gdrive") {
    if (args.isEmpty()) {
      if (!gdriveIsEnabled()) return "📊 Google Drive: desactivado. Usa: gdrive <url>";
      return "📊 Google Drive: activo\nURL: " + gdriveGetUrl();
    }
    String argsL = args; argsL.toLowerCase();
    if (argsL == "off" || argsL == "0" || argsL == "no") {
      gdriveSetUrl("");
      return "📊 Google Drive: desactivado.";
    }
    if (!args.startsWith("http")) return "❌ URL invalida. Debe comenzar con https://";
    gdriveSetUrl(args);
    return "📊 Google Drive: activado ✅\nURL guardada. Los proximos logs se enviaran al Sheet.";
  }

  return "❓ Comando no reconocido. Usa: help";
}

// =========================================================
//  CREDENCIALES NVS
// =========================================================

void loadStoredCredentials() {
  credStore.begin("cred", false);
  storedWifiSsid = credStore.getString("ssid", "");
  storedWifiPassword = credStore.getString("pass", "");
  storedTelegramToken = credStore.getString("token", "");
  botName = credStore.getString("botname", "");

  authorizedChatIds.clear();
  String stored = credStore.getString("ids", "");
  int start = 0;
  while (start < (int)stored.length()) {
    int comma = stored.indexOf(',', start);
    if (comma == -1) comma = stored.length();
    String id = stored.substring(start, comma);
    id.trim();
    if (!id.isEmpty()) authorizedChatIds.push_back(id);
    start = comma + 1;
  }
  loadSavedNetworks();
}

bool hasStoredCredentials() {
  return !storedWifiSsid.isEmpty() && !storedWifiPassword.isEmpty() &&
         !storedTelegramToken.isEmpty();
}

void saveWifiCredentials(const String &ssid, const String &password) {
  credStore.putString("ssid", ssid);
  credStore.putString("pass", password);
  storedWifiSsid = ssid;
  storedWifiPassword = password;
}

void saveTelegramToken(const String &token) {
  credStore.putString("token", token);
  storedTelegramToken = token;
}

void persistChatIds() {
  String s;
  for (size_t i = 0; i < authorizedChatIds.size(); i++) {
    if (i > 0) s += ',';
    s += authorizedChatIds[i];
  }
  credStore.putString("ids", s);
}

// ---- Redes WiFi guardadas ----

void loadSavedNetworks() {
  savedNetworks.clear();
  uint8_t n = credStore.getUChar("wn", 0);
  if (n > MAX_WIFI_NETWORKS) n = MAX_WIFI_NETWORKS;
  for (uint8_t i = 0; i < n; i++) {
    char ks[6], kp[6];
    snprintf(ks, sizeof(ks), "wn%ds", i);
    snprintf(kp, sizeof(kp), "wn%dp", i);
    WifiNetwork net;
    net.ssid = credStore.getString(ks, "");
    net.pass = credStore.getString(kp, "");
    if (!net.ssid.isEmpty()) savedNetworks.push_back(net);
  }
}

void saveSavedNetworks() {
  credStore.putUChar("wn", (uint8_t)savedNetworks.size());
  for (int i = 0; i < (int)savedNetworks.size(); i++) {
    char ks[6], kp[6];
    snprintf(ks, sizeof(ks), "wn%ds", i);
    snprintf(kp, sizeof(kp), "wn%dp", i);
    credStore.putString(ks, savedNetworks[i].ssid);
    credStore.putString(kp, savedNetworks[i].pass);
  }
  // Limpiar slots sobrantes (tras un delete)
  for (int i = savedNetworks.size(); i < MAX_WIFI_NETWORKS; i++) {
    char ks[6], kp[6];
    snprintf(ks, sizeof(ks), "wn%ds", i);
    snprintf(kp, sizeof(kp), "wn%dp", i);
    credStore.remove(ks);
    credStore.remove(kp);
  }
}

// Agrega o actualiza una red en la lista. Retorna false si la lista está llena
// y la red es nueva.
bool addSavedNetwork(const String &ssid, const String &pass) {
  for (auto &net : savedNetworks) {
    if (net.ssid == ssid) { net.pass = pass; saveSavedNetworks(); return true; }
  }
  if ((int)savedNetworks.size() >= MAX_WIFI_NETWORKS) return false;
  WifiNetwork net; net.ssid = ssid; net.pass = pass;
  savedNetworks.push_back(net);
  saveSavedNetworks();
  return true;
}

bool deleteSavedNetworkAt(int idx) {
  if (idx < 0 || idx >= (int)savedNetworks.size()) return false;
  savedNetworks.erase(savedNetworks.begin() + idx);
  saveSavedNetworks();
  return true;
}

bool isChatAuthorized(const String &chatId) {
  for (const auto &id : authorizedChatIds) {
    if (id == chatId) return true;
  }
  return false;
}

bool ensureChatAuthorized(const String &chatId) {
  if (isChatAuthorized(chatId)) return true;
  // Primer usuario: si no hay IDs autorizados, se autoriza automaticamente
  if (authorizedChatIds.empty()) {
    authorizedChatIds.push_back(chatId);
    persistChatIds();
    if (telegramBot) {
      telegramBot->sendMessage(chatId,
        "[Acceso] Eres el primer usuario registrado.\n"
        "Acceso de administrador concedido automaticamente.", "");
    }
    return true;
  }
  // Modo inscripcion activo: autorizar a cualquier usuario hasta llenar el limite.
  // El modo permanece abierto hasta que se cierre explicitamente con /acceso off.
  if (enrollmentOpen && authorizedChatIds.size() < 5) {
    authorizedChatIds.push_back(chatId);
    persistChatIds();
    int remaining = 5 - (int)authorizedChatIds.size();
    String nota = remaining > 0
      ? " (" + String(remaining) + " lugar(es) disponibles)"
      : " Limite alcanzado; acceso cerrado automaticamente.";
    if (remaining == 0) { enrollmentOpen = false; enrollmentExpireMs = 0; }
    broadcastMessage("[Acceso] Nuevo chat autorizado: " + chatId + nota);
    return true;
  }
  return false;
}

// =========================================================
//  SISTEMA DE CREDENCIALES (botón skip + serial)
// =========================================================

void IRAM_ATTR onSkipButtonFalling() {
  skipButtonFired = true;
}

bool isSkipButtonPressed() { return skipButtonFired; }

void warnMissingStoredCredentials() {
  if (missingStoredCredsWarned) return;
  Serial.println();
  Serial.println(
      "Boton de salto presionado pero no hay credenciales guardadas. "
      "Ingresa un dato valido.");
  missingStoredCredsWarned = true;
}

void notifyCredentialSkipUse() {
  if (credentialSkipNotified) return;
  Serial.println();
  Serial.println("Boton de salto presionado: usando credenciales guardadas en NVS.");
  credentialSkipNotified = true;
}

String readLineFromSerial(const char *prompt, uint32_t timeoutMs = 0,
                          bool allowSkipButton = false) {
  Serial.print(prompt);
  Serial.flush();

  String line;
  unsigned long start = millis();
  unsigned long lastDataTime = start;

  while (true) {
    while (Serial.available()) {
      char c = Serial.read();
      lastDataTime = millis();

      if (c == '\n' || c == '\r') {
        line.trim();
        return line;
      }
      line += c;
    }

    if (allowSkipButton) {
      if (skipCredentialPrompt && hasStoredCredentials()) {
        return "";
      }

      if (!skipCredentialPrompt && isSkipButtonPressed()) {
        if (hasStoredCredentials()) {
          notifyCredentialSkipUse();
          skipCredentialPrompt = true;
          return "";
        }
        warnMissingStoredCredentials();
      }
    }

    if (!line.isEmpty() && millis() - lastDataTime > 150) {
      line.trim();
      return line;
    }

    if (timeoutMs > 0 && millis() - start >= timeoutMs) {
      Serial.println();
      line.trim();
      return "";
    }

    delay(20);
  }
}

String promptOrStoredValue(const char *label, const String &storedValue, uint32_t timeoutMs) {
  if (skipCredentialPrompt && hasStoredCredentials()) {
    notifyCredentialSkipUse();
    return storedValue;
  }

  while (true) {
    if (!skipCredentialPrompt && isSkipButtonPressed()) {
      if (hasStoredCredentials()) {
        notifyCredentialSkipUse();
        skipCredentialPrompt = true;
        return storedValue;
      }
      warnMissingStoredCredentials();
    }

    Serial.println();
    Serial.print(label);

    if (!storedValue.isEmpty()) {
      Serial.print(" (Enter para usar el valor guardado)");
    }

    Serial.println();
    String value = readLineFromSerial("> ", timeoutMs, true);

    if (value.isEmpty()) {
      if (!storedValue.isEmpty()) {
        Serial.println("Usando valor almacenado en NVS.");
        return storedValue;
      }
      Serial.println("No hay un valor almacenado, ingresa un dato valido.");
      continue;
    }

    return value;
  }
}

void requestCredentials() {
  const uint32_t promptTimeoutMs = 30000;

  wifiSsid = promptOrStoredValue("WiFi SSID:", storedWifiSsid, promptTimeoutMs);
  wifiPassword = promptOrStoredValue("WiFi Password:", storedWifiPassword, promptTimeoutMs);
  telegramToken = promptOrStoredValue("Token Telegram:", storedTelegramToken, promptTimeoutMs);

  // Google Drive Apps Script URL — completamente opcional, Enter para omitir
  if (!skipCredentialPrompt) {
    Serial.println();
    Serial.println("Google Drive Apps Script URL (opcional):");
    Serial.println("  Pega la URL del webhook para guardar logs en Drive.");
    String stored = gdriveGetUrl();
    if (!stored.isEmpty()) {
      Serial.print("  [actual: ");
      Serial.print(stored);
      Serial.println("]");
      Serial.println("  Enter para conservar, \"off\" para desactivar.");
    } else {
      Serial.println("  Enter para omitir.");
    }
    String input = readLineFromSerial("> ", promptTimeoutMs, true);
    input.trim();
    if (!input.isEmpty()) {
      String inputL = input;
      inputL.toLowerCase();
      if (inputL == "off" || inputL == "no") {
        gdriveSetUrl("");
        Serial.println("Google Drive: desactivado.");
      } else if (input.startsWith("http")) {
        gdriveSetUrl(input);
        Serial.println("Google Drive: URL guardada.");
      } else {
        Serial.println("URL ignorada (debe comenzar con http). Usa /gdrive para configurarla luego.");
      }
    }
    // Si input está vacío se conserva el valor existente (o ninguno)
  }

  Serial.println();
  Serial.println("=========== CONFIGURACION INICIAL ===========");
  Serial.print("WiFi SSID: ");
  Serial.println(wifiSsid);
  Serial.print("WiFi Password: ");
  Serial.println(wifiPassword);
  Serial.print("Token Telegram: ");
  {
    int tLen = telegramToken.length();
    if (tLen > 8) {
      Serial.println(telegramToken.substring(0, 4) + "..." + telegramToken.substring(tLen - 4));
    } else if (tLen > 0) {
      Serial.println("(configurado)");
    } else {
      Serial.println("(no configurado)");
    }
  }
  Serial.print("Google Drive: ");
  Serial.println(gdriveIsEnabled() ? gdriveGetUrl() : "desactivado");
  Serial.println();
  Serial.println("Credenciales recibidas.");
  Serial.println("==============================================");
}

int promptTimezoneOffset(int defaultOffset) {
  const int storedOffset = getTimezoneOffsetHours();

  if (skipCredentialPrompt) {
    Serial.println();
    Serial.println("Boton de salto: usando offset guardado en NVS.");
    return storedOffset;
  }

  Serial.println();
  Serial.println("Zona horaria: ingresa el offset UTC en horas (ej: -5, -7, +4).");
  Serial.print("Valor actual ");
  Serial.print(defaultOffset);
  Serial.println(". Presiona Enter para mantenerlo.");

  while (true) {
    if (!skipCredentialPrompt && isSkipButtonPressed()) {
      notifyCredentialSkipUse();
      skipCredentialPrompt = true;
      Serial.println();
      Serial.println("Boton de salto: usando offset guardado en NVS.");
      return storedOffset;
    }

    String input = readLineFromSerial("> ", 20000, true);
    input.trim();

    if (skipCredentialPrompt) {
      Serial.println("Usando offset almacenado en NVS.");
      return storedOffset;
    }

    if (input.isEmpty()) {
      Serial.println("Usando offset existente.");
      setTimezoneOffsetHours(defaultOffset);
      return defaultOffset;
    }

    int offset = constrain(input.toInt(), -12, 14);
    Serial.print("Offset seleccionado: ");
    Serial.println(offset);
    setTimezoneOffsetHours(offset);
    return offset;
  }
}

// =========================================================
//  WIFI + NTP + RTC
// =========================================================

// Intenta conectar a una red específica con timeout. Deja el WiFi desconectado
// si falla. Retorna true si se obtiene IP.
bool tryConnectNet(const String &ssid, const String &pass, unsigned long tMs) {
  WiFi.disconnect(false);
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.print("Conectando a " + ssid);
  unsigned long s = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - s >= tMs) {
      Serial.println(" TIMEOUT");
      WiFi.disconnect(true);
      return false;
    }
    delay(500);
    Serial.print('.');
  }
  Serial.println(" OK");
  Serial.println("IP: " + WiFi.localIP().toString());
  return true;
}

bool connectWifi(unsigned long timeoutMs = 30000) {
  WiFi.mode(WIFI_STA);

  // 1. Intentar la red principal
  if (tryConnectNet(wifiSsid, wifiPassword, timeoutMs)) {
    addSavedNetwork(wifiSsid, wifiPassword);
    saveWifiCredentials(wifiSsid, wifiPassword);
    return true;
  }

  // 2. Fallback: probar redes guardadas que hayan funcionado antes
  if (!savedNetworks.empty()) {
    Serial.println("Red principal fallo. Probando redes guardadas...");
    for (auto &net : savedNetworks) {
      if (net.ssid == wifiSsid) continue;  // ya la intentamos
      if (tryConnectNet(net.ssid, net.pass, 12000)) {
        wifiSsid     = net.ssid;
        wifiPassword = net.pass;
        addSavedNetwork(wifiSsid, wifiPassword);
        saveWifiCredentials(wifiSsid, wifiPassword);
        return true;
      }
    }
  }

  return false;
}

String tzFromOffset(int off) {
  if (off == 0) return "GMT";
  return off > 0 ? "GMT-" + String(off) : "GMT" + String(abs(off));
}

void configureTimezone() {
  tzPosix = tzFromOffset(getTimezoneOffsetHours());
  setenv("TZ", tzPosix.c_str(), 1);
  tzset();
}

bool initRtc() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  if (!rtc.begin()) {
    Serial.println("RTC no encontrado.");
    return false;
  }
  Serial.println("RTC OK.");
  return true;
}

bool setTimeFromRtc() {
  if (!rtcReady) return false;
  DateTime dt = rtc.now();
  if (dt.year() < 2020) return false;
  timeval tv{dt.unixtime(), 0};
  settimeofday(&tv, nullptr);
  configureTimezone();
  Serial.println("Hora desde RTC.");
  return true;
}

bool syncNtp(unsigned long maxWaitMs) {
  if (WiFi.status() != WL_CONNECTED) return false;

  Serial.print("Sincronizando NTP");
  configTzTime(tzPosix.c_str(), "pool.ntp.org", "time.nist.gov", "time.cloudflare.com");

  struct tm t;
  unsigned long start = millis();
  while (millis() - start < maxWaitMs) {
    if (getLocalTime(&t)) {
      Serial.println(" OK");
      Serial.println("Hora: " + formatDateTime(t));
      if (rtcReady) {
        time_t now;
        time(&now);
        if (now > 10) rtc.adjust(DateTime(now));
      }
      return true;
    }
    delay(500);
    Serial.print('.');
  }
  Serial.println(" TIMEOUT");
  return false;
}

bool verifyTelegramToken(uint8_t maxAttempts = 5, uint16_t retryDelayMs = 1000) {
  if (!telegramBot || WiFi.status() != WL_CONNECTED) return false;

  Serial.print("Verificando token Telegram");
  for (uint8_t i = 0; i < maxAttempts; i++) {
    if (telegramBot->getMe()) {
      Serial.println(" OK (@" + telegramBot->userName + ")");
      return true;
    }
    delay(retryDelayMs);
    Serial.print('.');
  }
  Serial.println(" FALLO");
  return false;
}

bool initializeTelegramBot(uint8_t maxTokenRetries = 2) {
  bool tokenVerificado = false;
  uint8_t intentosToken = 0;

  while (!tokenVerificado && intentosToken < maxTokenRetries) {
    delete telegramBot;
    telegramBot = new UniversalTelegramBot(telegramToken, telegramClient);

    tokenVerificado = verifyTelegramToken();

    if (!tokenVerificado) {
      intentosToken++;

      if (intentosToken >= maxTokenRetries) {
        Serial.println("No se pudo verificar el token tras varios intentos.");
        Serial.println("Se continuara sin verificacion; si el token es incorrecto el bot no respondera.");
        break;
      }

      Serial.println("Ingresa un token valido o presiona Enter para reutilizarlo.");
      String nuevoToken = readLineFromSerial("Nuevo token (vacio para mantener): ");

      if (!nuevoToken.isEmpty()) {
        telegramToken = nuevoToken;
      }
    }
  }

  if (tokenVerificado) {
    saveTelegramToken(telegramToken);
    Serial.println("Token verificado correctamente.");
  }

  telegramEnabled = tokenVerificado;
  return tokenVerificado;
}

// =========================================================
//  TELEGRAM POLLING
// =========================================================

// Activa o desactiva el power-save del radio WiFi.
// WIFI_PS_MAX_MODEM: radio duerme entre beacons DTIM → ahorra ~50-120mA.
// WIFI_PS_NONE: radio siempre activo → máximo rendimiento.
void setWifiPowerSave(bool enable) {
  if (enable == wifiPsSleeping) return;
  if (WiFi.status() != WL_CONNECTED) return;
  esp_wifi_set_ps(enable ? WIFI_PS_MAX_MODEM : WIFI_PS_NONE);
  wifiPsSleeping = enable;
}

void pollTelegram() {
  if (!telegramEnabled || WiFi.status() != WL_CONNECTED || !telegramBot) return;
  unsigned long now = millis();

  if (wifiSleepMode && wsEndpoint.count() == 0) {
    // Sleep mode activo y sin clientes web conectados:
    // dormir el modem entre polls y despertar solo cuando toca.
    if (now - lastTelegramMs < wifiSleepPollMs) {
      setWifiPowerSave(true);   // modem a power-save hasta que toque el siguiente poll
      return;
    }
    // Toca hacer poll: despertar el radio brevemente
    setWifiPowerSave(false);
    delay(20);  // pausa mínima para que el radio esté operativo
  } else {
    // Modo normal (sleep desactivado o hay clientes web activos)
    setWifiPowerSave(false);
    if (now - lastTelegramMs < 1500) return;
  }

  lastTelegramMs = now;

  int n = telegramBot->getUpdates(telegramBot->last_message_received + 1);
  while (n) {
    for (int i = 0; i < n; i++) {
      String chatId = telegramBot->messages[i].chat_id;
      String text = telegramBot->messages[i].text;
      lastTelegramChatId = chatId;

      if (!ensureChatAuthorized(chatId)) {
        telegramBot->sendMessage(chatId, "Chat no autorizado.", "");
        continue;
      }

      // Normalizar: quitar "/" del inicio para usar handler unificado
      String normalized = text;
      normalized.trim();
      if (normalized.startsWith("/")) {
        normalized = normalized.substring(1);
      }

      String response = handleCommand(chatId, normalized);
      if (!response.isEmpty()) {
        telegramBot->sendMessage(chatId, response, "");
      }
    }
    n = telegramBot->getUpdates(telegramBot->last_message_received + 1);
  }
}

// =========================================================
//  PERSISTENCIA DE SUSCRIPCIONES DE REPORTES
// =========================================================
// Guarda hasta 5 suscriptores en NVS (namespace "reporters").
// Formato: cnt + id0..4 (String) + ms0..4 (uint32) + cp0..4 (bool)
void saveReportSubscribers() {
  Preferences rp;
  rp.begin("reporters", false);
  rp.clear();
  int i = 0;
  for (auto &kv : reportSubscribers) {
    String b = String(i);
    rp.putString(("id" + b).c_str(), kv.first);
    rp.putUInt(("ms" + b).c_str(), (uint32_t)kv.second.intervalMs);
    rp.putBool(("cp" + b).c_str(), kv.second.compact);
    if (++i >= 5) break;
  }
  rp.putInt("cnt", i);
  rp.end();
}

void loadReportSubscribers() {
  Preferences rp;
  rp.begin("reporters", true);
  int cnt = rp.getInt("cnt", 0);
  for (int i = 0; i < cnt && i < 5; i++) {
    String b = String(i);
    String id = rp.getString(("id" + b).c_str(), "");
    if (id.isEmpty()) continue;
    ReportSub r;
    r.intervalMs = (unsigned long)rp.getUInt(("ms" + b).c_str(), 30 * 60000UL);
    r.compact    = rp.getBool(("cp" + b).c_str(), true);
    r.lastMs     = 0;  // reiniciar timer tras reboot para no enviar de inmediato
    reportSubscribers[id] = r;
  }
  rp.end();
}

// Cierra el modo inscripcion cuando vence el temporizador.
// Llamar desde loop(). Opera solo si hay un plazo activo.
void tickEnrollment() {
  if (!enrollmentOpen || enrollmentExpireMs == 0) return;
  if (millis() >= enrollmentExpireMs) {
    enrollmentOpen    = false;
    enrollmentExpireMs = 0;
    broadcastMessage("🔒 Modo inscripcion cerrado automaticamente (tiempo agotado).");
  }
}

void sendPeriodicReport() {
  if (!telegramEnabled || !telegramBot || reportSubscribers.empty()) return;
  unsigned long now = millis();
  for (auto &kv : reportSubscribers) {
    if (!isChatAuthorized(kv.first)) continue;
    ReportSub &r = kv.second;
    if (now - r.lastMs < r.intervalMs) continue;
    r.lastMs = now;
    String msg = r.compact ? formatStatusCompact() : formatStatusFull();
    telegramBot->sendMessage(kv.first, msg, "");
  }
}

// =========================================================
//  SERIAL COMMANDS
// =========================================================

void handleSerialInput() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  String response = handleCommand("", line);
  if (!response.isEmpty()) Serial.println(response);
}

// =========================================================
//  SETUP
// =========================================================

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  dht.begin();
  configInit();
  initIrrigationHardware();
  initFan();

  pinMode(PIN_ACLIGHT, OUTPUT);
  digitalWrite(PIN_ACLIGHT, HIGH);
  ledcSetup(LED_PWM_CHANNEL, LED_PWM_FREQ, LED_PWM_RES);
  ledcAttachPin(PIN_LED_MORADO, LED_PWM_CHANNEL);
  ledcWrite(LED_PWM_CHANNEL, 0);
  pinMode(PIN_CRED_SKIP, INPUT_PULLUP);
  delay(10);
  attachInterrupt(digitalPinToInterrupt(PIN_CRED_SKIP), onSkipButtonFalling, FALLING);

  // Cargar credenciales almacenadas
  loadStoredCredentials();

  // Cargar URL de Google Drive antes del prompt (para mostrar valor actual)
  gdriveInit();

  const bool credSkipPressed = isSkipButtonPressed();
  const bool hasStoredCreds = hasStoredCredentials();
  skipCredentialPrompt = credSkipPressed && hasStoredCreds;

  if (skipCredentialPrompt) {
    notifyCredentialSkipUse();
    wifiSsid = storedWifiSsid;
    wifiPassword = storedWifiPassword;
    telegramToken = storedTelegramToken;
  } else {
    if (credSkipPressed && !hasStoredCreds) {
      warnMissingStoredCredentials();
      Serial.println("Solicitando datos por Serial.");
    }
    requestCredentials();
  }

  // Timezone
  int tzOffset = promptTimezoneOffset(getTimezoneOffsetHours());
  setTimezoneOffsetHours(tzOffset);
  configureTimezone();

  // RTC
  rtcReady = initRtc();

  // WiFi — si todo falla, re-solicita credenciales y reintenta
  bool wifiOk = false;
  while (!wifiOk) {
    wifiOk = connectWifi();
    if (!wifiOk) {
      Serial.println();
      Serial.println("[WiFi] Todas las redes fallaron (principal + guardadas).");
      Serial.println("       Ingresa un SSID diferente. Enter no es valido.");
      Serial.println();
      // SSID: obligatorio escribir algo; Enter rechazado porque todas las
      // redes conocidas ya fueron probadas y fallaron.
      String s;
      do {
        Serial.println("WiFi SSID:");
        s = readLineFromSerial("> ");
        if (s.isEmpty()) Serial.println("Debes ingresar un SSID. Enter no es valido aqui.");
      } while (s.isEmpty());
      wifiSsid = s;
      // Password: tambien obligatorio; Enter rechazado igual que el SSID.
      String p;
      do {
        Serial.println("WiFi Password:");
        p = readLineFromSerial("> ");
        if (p.isEmpty()) Serial.println("Debes ingresar una contrasena. Enter no es valido aqui.");
      } while (p.isEmpty());
      wifiPassword = p;
    }
  }

  // NTP (o fallback a RTC)
  bool ntpOk = wifiOk && syncNtp();
  if (!ntpOk) {
    Serial.println("NTP fallo, intentando RTC...");
    if (!setTimeFromRtc()) {
      Serial.println("[HORA] Sin fuente de tiempo valida. Timestamps pueden ser incorrectos.");
    }
  }

  // SD card (después de NTP para que los timestamps sean correctos)
  bool sdOk = sdInit();
  sdSetLogHook(gdriveWriteLog);  // registrar hook de Google Drive

  logAccion("INICIO", "Sistema iniciado; WiFi " +
            String(wifiOk ? "OK" : "FALLO") +
            "; NTP " + String(ntpOk ? "OK" : "FALLO") +
            "; SD " + String(sdOk ? "OK" : "FALLO"));

  // Calcular el primer slot de log alineado al reloj (múltiplo de 5 min)
  // Usa el RTC directamente como fuente de tiempo; cae back a NTP si no hay RTC
  {
    const time_t SD_INTERVAL = 300;  // 5 minutos en segundos
    time_t ref = 0;
    if (rtcReady) {
      ref = rtc.now().unixtime();  // epoch directo del hardware RTC
    } else {
      time(&ref);                  // fallback: epoch del sistema (NTP)
    }
    if (ref > 0) {
      nextSdLogEpoch = ((ref / SD_INTERVAL) + 1) * SD_INTERVAL;
    }
  }

  // Luces y fan
  applyLightSchedule();
  updateFan(true);

  // Telegram
  telegramClient.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  telegramClient.setTimeout(15000);
  initializeTelegramBot();
  loadReportSubscribers();  // restaurar suscripciones de reportes tras reboot

  // Web dashboard (requiere WiFi conectado)
  if (WiFi.status() == WL_CONNECTED) {
    initWebServer();
  }

  Serial.println(formatConfig());
  Serial.println("Sistema listo.");
}

// =========================================================
//  LOOP
// =========================================================

void loop() {
  tickIrrigation();   // avanzar maquina de estados de riego (no bloqueante)
  tickEnrollment();   // cerrar modo inscripcion si venció el temporizador
  handleSerialInput();
  pollTelegram();
  wsEndpoint.cleanupClients();
  sendPeriodicReport();

  unsigned long now = millis();

  if (now - lastLightMs >= 10000) {
    lastLightMs = now;
    applyLightSchedule();
  }

  if (now - lastFanMs >= 2000) {
    lastFanMs = now;
    updateFan();
  }

  if (now - lastSoilMs >= 2000) {
    lastSoilMs = now;
    updateTankFloat();  // debounce del flotador antes de chequear riego y alertas
    checkSoilAndIrrigate();
    evaluateAlerts();
    broadcastSensorData();
  }

  // Log periódico de sensores a SD alineado al reloj (cada :00, :05, :10, :15...)
  if (nextSdLogEpoch > 0) {
    time_t epoch_now = rtcReady ? (time_t)rtc.now().unixtime() : time(nullptr);
    if (epoch_now >= nextSdLogEpoch) {
      const time_t SD_INTERVAL = 300;
      nextSdLogEpoch = ((epoch_now / SD_INTERVAL) + 1) * SD_INTERVAL;
      float sdTemp, sdRh;
      bool sdValid = readAmbient(sdTemp, sdRh);
      int sdSoil, sdMq;
      readFastSensors(sdSoil, sdMq);
      sdSoil = soilPercentFromAdc(sdSoil);
      logSensors(sdTemp, sdRh, sdSoil, sdMq, sdValid);
    }
  }
}
