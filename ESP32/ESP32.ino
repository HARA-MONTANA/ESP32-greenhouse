#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <Wire.h>
#include <RTClib.h>
#include <sys/time.h>
#include <time.h>
#include <Preferences.h>
#include <vector>
#include <DHT.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>

#include "pins.h"
#include "config.h"
#include "irrigation.h"
#include "dashboard.h"
#include "sdcard.h"

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

// RTC
RTC_DS3231 rtc;
bool rtcReady = false;

// DHT
DHT dht(PIN_DHT, DHT22);
float cachedTemp = NAN;
float cachedRh = NAN;
unsigned long lastDhtReadMs = 0;
bool dhtValid = false;

// Fan PWM
const int FAN_PWM_CHANNEL = 0;
const int FAN_PWM_FREQ = 25000;
const int FAN_PWM_RES = 8;
const int FAN_PWM_MAX = (1 << FAN_PWM_RES) - 1;
bool fanAuto = true;
int fanPercent = 0;
int fanApplied = -1;

// RPM estimado a partir del duty cycle (100 % ≈ 3000 RPM)
unsigned long fanRpm = 0;

// LED morado PWM
const int LED_PWM_CHANNEL = 1;
const int LED_PWM_FREQ    = 1000;
const int LED_PWM_RES     = 8;
const int LED_PWM_MAX     = 255;
bool ledManual = false;  // true = encender independiente de etapa (respeta horario)

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
unsigned long lastReportMs = 0;
time_t nextSdLogEpoch = 0;   // epoch del próximo log alineado al reloj; 0 = no inicializado

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
  bool valid   = readAmbient(tempC, rh);
  int  soilAdc = readSoilMoisture();
  int  soilPct = soilPercentFromAdc(soilAdc);
  int  mqRaw   = analogRead(PIN_MQ135);
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
  doc["fan_rpm"]     = (long)fanRpm;
  doc["temp_valid"]  = valid;
  doc["ts"]          = (long)ts;
  // System state
  doc["stage"]       = stageToCode(getCurrentStage());
  doc["fan_pct"]     = fanApplied;
  doc["fan_auto"]    = fanAuto;
  doc["light_on"]    = areLightsOn();
  doc["led_pct"]     = getLedIntensity();
  doc["led_manual"]  = ledManual;
  doc["auto_irr"]    = isAutoIrrigationEnabled();
  doc["tank_ok"]     = isTankWaterAvailable();
  // Alerts
  doc["alert_temp"]  = alertTempHigh;
  doc["alert_rh"]    = alertRhLow || alertRhHigh;
  doc["alert_mq"]    = alertMq;
  doc["alert_water"] = alertWater;
  // Metadata
  doc["last_irr"]    = (long)getLastIrrigationEpoch();
  doc["tz_offset"]   = getTimezoneOffsetHours();
  doc["bot_name"]    = botName;

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
      if (deserializeJson(cmd, msg) == DeserializationError::Ok) {
        String c = cmd["cmd"] | "";
        String a = cmd["args"] | "";
        if (!c.isEmpty()) {
          handleCommand("ws", a.length() ? c + " " + a : c);
          broadcastSensorData();
        }
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
    StaticJsonDocument<384> doc;
    doc["stage"]          = stageToCode(getCurrentStage());
    doc["pot_l"]          = getPotVolumeL();
    doc["ml_pl"]          = getMlPerLiterForStage(PLANTULA);
    doc["ml_veg"]         = getMlPerLiterForStage(VEGETATIVO);
    doc["ml_pre"]         = getMlPerLiterForStage(PRE_FLORACION);
    doc["ml_flo"]         = getMlPerLiterForStage(FLORACION);
    doc["ml_fin"]         = getMlPerLiterForStage(FINAL);
    doc["luz_pl"]         = getLightHoursForStage(PLANTULA);
    doc["luz_veg"]        = getLightHoursForStage(VEGETATIVO);
    doc["pause_days"]     = getIrrigationIntervalDays();
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
  int mqReading = analogRead(PIN_MQ135);

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
  fanRpm = (unsigned long)fanApplied * 30;  // estimado: 100% ≈ 3000 RPM
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

void applyLightSchedule() {
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

  bool shouldBeOn    = nowTs >= startTs && nowTs < offTs;
  bool ledShouldBeOn = shouldBeOn && (stageUsesLeds(stage) || ledManual);

  static bool prevLightOn = false;
  if (shouldBeOn != prevLightOn) {
    prevLightOn = shouldBeOn;
    logAccion(shouldBeOn ? "LUZ_ON" : "LUZ_OFF",
              "Etapa: " + stageToString(stage) + "; " +
              String(getLightHoursForStage(stage)) + "h programadas");
  }

  digitalWrite(PIN_ACLIGHT, shouldBeOn ? LOW : HIGH);

  if (ledShouldBeOn) {
    int duty = map(getLedIntensity(), 0, 100, 0, LED_PWM_MAX);
    ledcWrite(LED_PWM_CHANNEL, duty);
  } else {
    ledcWrite(LED_PWM_CHANNEL, 0);
  }
}

// =========================================================
//  ALERTAS
// =========================================================

void evaluateAlerts() {
  // Leer todos los sensores una sola vez al inicio para que estén disponibles
  // en todos los bloques de alerta (incluido el de agua, que va primero)
  float tempC, rh;
  bool ok = readAmbient(tempC, rh);
  int soilPct = soilPercentFromAdc(readSoilMoisture());
  int mqRaw   = analogRead(PIN_MQ135);
  plantStage stage = getCurrentStage();

  bool water = isTankWaterAvailable();
  if (!water && !alertWater) {
    broadcastMessage("ALERTA: tanque sin agua");
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
      broadcastMessage("ALERTA: temp alta (" + String(tempC, 1) + "C >= " + String(getTempAlertThreshold()) + "C)");
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
      broadcastMessage("ALERTA: humedad baja (" + String(rh, 0) + "% < " + String(getRhLowAlertThreshold()) + "%)");
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
      broadcastMessage("ALERTA: humedad alta (" + String(rh, 0) + "% > " + String(getRhHighAlertThreshold()) + "%)");
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
    broadcastMessage("ALERTA: aire pobre (MQ=" + String(mqRaw) + " >= " + String(getMqAlertThreshold()) + ")");
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

String formatStatus() {
  float temp, rh;
  bool ok = readAmbient(temp, rh);
  int mq = analogRead(PIN_MQ135);
  int soilAdc = readSoilMoisture();
  int soilPct = soilPercentFromAdc(soilAdc);
  bool water = isTankWaterAvailable();
  float stageMl = getMlPerLiterForStage(getCurrentStage()) * getPotVolumeL();

  String s;
  s += "==ESTADO==\n";
  s += "Temp: " + (ok ? String(temp, 1) + "C" : String("N/D"));
  s += " | HR: " + (ok ? String(rh, 0) + "%" : String("N/D"));
  s += " | MQ: " + String(mq) + "\n";
  s += "Suelo: " + String(soilPct) + "% | Fan: " + String(fanPercent) + "% [" + (fanAuto ? "AUTO" : "MANUAL") + "]\n";
  s += "Etapa: " + stageToString(getCurrentStage());
  s += " | Luz: " + String(areLightsOn() ? "ON" : "OFF") + " (OFF " + formatLightsOffTime() + ")";
  s += " | Agua: " + String(water ? "SI" : "NO") + "\n";
  s += "LED morado: ";
  s += (ledcRead(LED_PWM_CHANNEL) > 0) ? String(getLedIntensity()) + "%" : String("OFF");
  s += ledManual ? " [MANUAL]" : " [AUTO]";
  s += "\n";
  s += "Riego: " + formatLastIrrigation() + " | mL: " + String(stageMl, 0);
  s += " | Auto: " + String(isAutoIrrigationEnabled() ? "ON" : "OFF");
  return s;
}

String formatConfig() {
  struct tm t;
  String now = getLocalTime(&t) ? formatDateTime(t) : "Sin hora";
  float potL = getPotVolumeL();
  float stageMl = getMlPerLiterForStage(getCurrentStage()) * potL;

  String s;
  s += "==CONFIG==\n";
  s += "Etapa: " + stageToString(getCurrentStage()) + "\n";
  s += "mL/L: Pl=" + String(getMlPerLiterForStage(PLANTULA));
  s += " Veg=" + String(getMlPerLiterForStage(VEGETATIVO));
  s += " Pre=" + String(getMlPerLiterForStage(PRE_FLORACION));
  s += " Flo=" + String(getMlPerLiterForStage(FLORACION));
  s += " Fin=" + String(getMlPerLiterForStage(FINAL)) + "\n";
  s += "mL calculados: " + String(stageMl, 0) + " mL | Maceta: " + String(potL, 1) + " L\n";
  s += "Suelo: min " + String(getSoilThreshold()) + "% max " + String(getSoilHighThreshold()) + "%\n";
  s += "Intervalo riego: " + String(getIrrigationIntervalDays()) + " dias\n";
  s += "Alertas: Temp>" + String(getTempAlertThreshold()) + "C HR<" + String(getRhLowAlertThreshold());
  s += "% HR>" + String(getRhHighAlertThreshold()) + "% MQ>" + String(getMqAlertThreshold()) + "\n";
  s += "Hora: " + now;
  return s;
}

// =========================================================
//  COMANDO UNIFICADO (Serial + Telegram)
// =========================================================

String commandHelp() {
  String h;
  h += "== Uso diario ==\n";
  h += "estado - Ver estado\n";
  h += "regar [mL] - Riego manual\n";
  h += "autoriego [on|off] - Riego automatico\n";
  h += "vent [0-100] - Ventilador manual\n";
  h += "ventauto [on|off] - Ventilador automatico\n";
  h += "reportes [on|off] [min]\n";
  h += "\n== Configuracion ==\n";
  h += "config - Ver configuracion\n";
  h += "etapa [pl|veg|pre|flo|fin]\n";
  h += "maceta [litros]\n";
  h += "ml [etapa] [valor]\n";
  h += "luz [etapa] [horas]\n";
  h += "led [on|off|0-100] - LED morado manual (respeta horario)\n";
  h += "pausariego [dias]\n";
  h += "suelomin [%] / suelomax [%]\n";
  h += "calsuelo [SECO] [HUMEDO]\n";
  h += "timezone [offset]\n";
  h += "\n== Alertas ==\n";
  h += "tempmax [C] / hummin [%] / hummax [%] / airemax [N]\n";
  h += "\n== Bomba ==\n";
  h += "calibrar - Bomba 5s para medir\n";
  h += "caudal [mL] - Guardar volumen medido\n";
  h += "\n== Admin ==\n";
  h += "addid [ID] / delid [ID] / ids\n";
  h += "reset - Restablecer configuracion";
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
    return formatStatus();
  }

  if (cmd == "config" || cmd == "conf") {
    return formatConfig();
  }

  if (cmd == "regar") {
    float ml = args.toFloat();
    if (ml <= 0) return "Uso: regar [mL]";
    ml = min(ml, 1500.0f);
    if (!isPumpCalibrated()) return "Bomba sin calibrar. Usa: calibrar";
    if (!isTankWaterAvailable()) return "Tanque sin agua.";
    irrigateVolume(ml, readSoilMoisture());
    return "Riego manual: " + String(ml) + " mL";
  }

  if (cmd == "autoriego") {
    if (args.isEmpty()) return String("Riego auto: ") + (isAutoIrrigationEnabled() ? "ON" : "OFF");
    bool enable = parseOnOff(args);
    setAutoIrrigationEnabled(enable);
    logAccion("CMD", String("autoriego ") + (enable ? "on" : "off"));
    return String("Riego auto ") + (isAutoIrrigationEnabled() ? "activado" : "desactivado");
  }

  if (cmd == "vent") {
    int pct = constrain(args.toInt(), 0, 100);
    fanPercent = pct;
    fanAuto = false;
    updateFan(true);
    logAccion("VENT", "manual " + String(pct) + "%");
    return "Fan manual: " + String(pct) + "%";
  }

  if (cmd == "ventauto") {
    if (args.isEmpty()) return String("Fan auto: ") + (fanAuto ? "ON" : "OFF");
    fanAuto = parseOnOff(args);
    updateFan(true);
    logAccion("VENT", fanAuto ? "auto activado" : "auto desactivado");
    return String("Fan auto ") + (fanAuto ? "ON" : "OFF");
  }

  if (cmd == "reportes") {
    if (args.isEmpty()) {
      return String("Reportes ") + (getAutoReadingsEnabled() ? "ON" : "OFF") +
             " cada " + String(getAutoReadingsIntervalMs() / 60000) + " min";
    }
    int sp2 = args.indexOf(' ');
    String onoff = sp2 == -1 ? args : args.substring(0, sp2);
    bool enabled = parseOnOff(onoff);
    unsigned long interval = getAutoReadingsIntervalMs();
    if (sp2 > 0) {
      int mins = args.substring(sp2 + 1).toInt();
      if (mins > 0) interval = mins * 60000UL;
    }
    setAutoReadings(enabled, interval);
    return String("Reportes ") + (enabled ? "activados" : "desactivados") +
           " cada " + String(interval / 60000) + " min";
  }

  // --- Configuración ---

  if (cmd == "etapa" || cmd == "stage") {
    if (args.isEmpty()) return "Uso: etapa [pl|veg|pre|flo|fin]";
    updateStage(stageFromString(args));
    configSave();
    logAccion("CMD", "etapa " + stageToString(getCurrentStage()));
    return "Etapa: " + stageToString(getCurrentStage());
  }

  if (cmd == "maceta") {
    float l = args.toFloat();
    if (l <= 0) return "Uso: maceta [litros]";
    if (!setPotVolumeL(l)) return "Rango: 1-50 L";
    return "Maceta: " + String(l, 1) + " L";
  }

  if (cmd == "ml") {
    int sp2 = args.indexOf(' ');
    if (sp2 == -1) return "Uso: ml [etapa] [valor]";
    plantStage stage = stageFromString(args.substring(0, sp2));
    int val = args.substring(sp2 + 1).toInt();
    if (!setMlPerLiterForStage(stage, val)) return "Rango mL/L: 5-200";
    return "mL/L " + args.substring(0, sp2) + ": " + String(val);
  }

  if (cmd == "luz") {
    int sp2 = args.indexOf(' ');
    if (sp2 == -1) return "Uso: luz [etapa] [horas]";
    plantStage stage = stageFromString(args.substring(0, sp2));
    int hours = args.substring(sp2 + 1).toInt();
    if (stage == PRE_FLORACION || stage == FLORACION || stage == FINAL) {
      return "Pre/flo/fin fijas en 12h.";
    }
    if (!setLightHoursForStage(stage, hours)) return "Rango: 12-20 h";
    return "Luz " + args.substring(0, sp2) + ": " + String(hours) + " h";
  }

  if (cmd == "led") {
    if (args.isEmpty()) {
      bool on = (ledcRead(LED_PWM_CHANNEL) > 0);
      return String("LED morado: ") + (on ? "ON" : "OFF") +
             " | Modo: " + (ledManual ? "MANUAL" : "AUTO") +
             " | Brillo: " + String(getLedIntensity()) + "%";
    }
    int pct = args.toInt();
    if (args == String(pct) && pct >= 0 && pct <= 100) {
      if (pct == 0) {
        ledManual = false;
      } else {
        setLedIntensity(pct);
        ledManual = true;
      }
      applyLightSchedule();
      return String("LED morado brillo: ") + String(pct) + "%" +
             (pct == 0 ? " (auto)" : " (manual, sigue horario)");
    }
    ledManual = parseOnOff(args);
    applyLightSchedule();
    return String("LED morado: ") + (ledManual ? "ON manual (sigue horario)" : "auto por etapa");
  }

  if (cmd == "pausariego") {
    int days = args.toInt();
    if (days <= 0) return "Uso: pausariego [dias]";
    if (!setIrrigationIntervalDays(days)) return "Rango: 1-5 dias";
    return "Intervalo riego: " + String(days) + " dias";
  }

  if (cmd == "suelomin") {
    int pct = args.toInt();
    if (!setSoilThreshold(pct)) return "Rango: 0-50%";
    return "Suelo min: " + String(pct) + "%";
  }

  if (cmd == "suelomax") {
    int pct = args.toInt();
    if (!setSoilHighThreshold(pct)) return "Rango: 50-100%";
    return "Suelo max: " + String(pct) + "%";
  }

  if (cmd == "calsuelo") {
    int sp2 = args.indexOf(' ');
    if (sp2 == -1) return "Uso: calsuelo [SECO] [HUMEDO]";
    int dry = args.substring(0, sp2).toInt();
    int wet = args.substring(sp2 + 1).toInt();
    setSoilCalibration(dry, wet);
    return "Calibracion suelo: seco=" + String(getSoilDryAdc()) + " humedo=" + String(getSoilWetAdc());
  }

  if (cmd == "timezone" || cmd == "tz") {
    if (args.isEmpty()) return "Timezone actual: UTC" + String(getTimezoneOffsetHours());
    int offset = constrain(args.toInt(), -12, 14);
    setTimezoneOffsetHours(offset);
    configureTimezone();
    syncNtp();
    return "Timezone: UTC" + String(offset);
  }

  // --- Alertas ---

  if (cmd == "tempmax") {
    int val = args.toInt();
    if (val <= 0) return "Uso: tempmax [C]";
    setTempAlertThreshold(val);
    return "Alerta temp: " + String(getTempAlertThreshold()) + "C";
  }

  if (cmd == "hummin") {
    int val = args.toInt();
    if (val <= 0) return "Uso: hummin [%]";
    setRhLowAlertThreshold(val);
    return "Alerta HR baja: " + String(getRhLowAlertThreshold()) + "%";
  }

  if (cmd == "hummax") {
    int val = args.toInt();
    if (val <= 0) return "Uso: hummax [%]";
    setRhHighAlertThreshold(val);
    return "Alerta HR alta: " + String(getRhHighAlertThreshold()) + "%";
  }

  if (cmd == "airemax") {
    int val = args.toInt();
    if (val <= 0) return "Uso: airemax [N]";
    setMqAlertThreshold(val);
    return "Alerta MQ: " + String(getMqAlertThreshold());
  }

  // --- Bomba ---

  if (cmd == "calibrar") {
    pumpOn();
    delay(5000);
    pumpOff();
    return "Bomba activada 5s. Mide el volumen y envia: caudal [mL]";
  }

  if (cmd == "caudal") {
    float ml = args.toFloat();
    if (ml <= 0) return "Uso: caudal [mL medidos]";
    float flow = ml / 5.0f;
    if (!setPumpFlow(flow)) return "Caudal fuera de rango (1-50 mL/s)";
    setAutoIrrigationEnabled(false);
    return "Caudal: " + String(flow, 1) + " mL/s. Usa: autoriego on";
  }

  // --- Admin ---

  if (cmd == "addid") {
    if (authorizedChatIds.size() >= 5) return "Maximo 5 IDs.";
    if (args.isEmpty()) return "Uso: addid [ID]";
    authorizedChatIds.push_back(args);
    persistChatIds();
    return "ID agregado.";
  }

  if (cmd == "delid") {
    if (args.isEmpty()) return "Uso: delid [ID]";
    for (auto it = authorizedChatIds.begin(); it != authorizedChatIds.end(); ++it) {
      if (*it == args) {
        authorizedChatIds.erase(it);
        persistChatIds();
        return "ID eliminado.";
      }
    }
    return "ID no encontrado.";
  }

  if (cmd == "ids") {
    String r = "IDs autorizados:\n";
    for (size_t i = 0; i < authorizedChatIds.size(); i++) {
      r += String(i + 1) + ": " + authorizedChatIds[i] + "\n";
    }
    return r;
  }

  if (cmd == "reset") {
    logAccion("CMD", "reset de configuracion");
    configReset();
    return "Configuracion restablecida.";
  }

  return "Comando no reconocido. Usa: help";
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

bool isChatAuthorized(const String &chatId) {
  for (const auto &id : authorizedChatIds) {
    if (id == chatId) return true;
  }
  return false;
}

bool ensureChatAuthorized(const String &chatId) {
  if (isChatAuthorized(chatId)) return true;
  if (authorizedChatIds.size() < 4) {
    authorizedChatIds.push_back(chatId);
    persistChatIds();
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

  Serial.println();
  Serial.println("=========== CONFIGURACION INICIAL ===========");
  Serial.print("WiFi SSID: ");
  Serial.println(wifiSsid);
  Serial.print("WiFi Password: ");
  Serial.println(wifiPassword);
  Serial.print("Token Telegram: ");
  Serial.println(telegramToken);
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

bool connectWifi(unsigned long timeoutMs = 30000) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());

  Serial.print("Conectando WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start >= timeoutMs) {
      Serial.println(" TIMEOUT");
      return false;
    }
    delay(500);
    Serial.print('.');
  }
  Serial.println(" OK");
  Serial.println("IP: " + WiFi.localIP().toString());
  saveWifiCredentials(wifiSsid, wifiPassword);
  return true;
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

void pollTelegram() {
  if (!telegramEnabled || WiFi.status() != WL_CONNECTED || !telegramBot) return;
  unsigned long now = millis();
  if (now - lastTelegramMs < 1500) return;
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

void sendPeriodicReport() {
  if (!telegramEnabled || !getAutoReadingsEnabled() || !telegramBot) return;
  unsigned long now = millis();
  if (now - lastReportMs < getAutoReadingsIntervalMs()) return;
  lastReportMs = now;

  String target = !lastTelegramChatId.isEmpty() ? lastTelegramChatId :
                  (!authorizedChatIds.empty() ? authorizedChatIds.front() : String(""));
  if (target.isEmpty()) return;

  telegramBot->sendMessage(target, formatStatus(), "");
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

  // WiFi
  connectWifi();

  // NTP (o fallback a RTC)
  bool ntpOk = syncNtp();
  if (!ntpOk) {
    Serial.println("NTP fallo, intentando RTC...");
    setTimeFromRtc();
  }

  // SD card (después de NTP para que los timestamps sean correctos)
  sdInit();
  logAccion("INICIO", "Sistema iniciado; WiFi " +
            String(WiFi.status() == WL_CONNECTED ? "OK" : "FALLO") +
            "; NTP " + String(ntpOk ? "OK" : "FALLO"));

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
      int sdSoil   = soilPercentFromAdc(readSoilMoisture());
      int sdMq     = analogRead(PIN_MQ135);
      logSensors(sdTemp, sdRh, sdSoil, sdMq, sdValid);
    }
  }
}
