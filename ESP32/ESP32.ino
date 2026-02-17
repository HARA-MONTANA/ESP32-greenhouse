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

#include "pins.h"
#include "config.h"
#include "irrigation.h"

// =========================================================
//  VARIABLES GLOBALES
// =========================================================
String wifiSsid;
String wifiPassword;
String telegramToken;
String storedWifiSsid;
String storedWifiPassword;
String storedTelegramToken;
bool skipCredentialPrompt = false;
bool credentialSkipNotified = false;
bool missingStoredCredsWarned = false;
const char *kStoredTimezoneOffsetKey = "tzOff";

Preferences credentialsStore;
Preferences settingsStore;
unsigned long lastSoilCheckMs = 0;
unsigned long lastTelegramPollMs = 0;
unsigned long lastAutoReadingMs = 0;

bool autoReadingsEnabled = false;
unsigned long autoReadingsIntervalMs = 300000;  // 5 minutos

int soilDryAdc = 2150;
int soilWetAdc = 500;
int tempAlertThreshold = 30;
int rhLowAlertThreshold = 50;
int rhHighAlertThreshold = 50;
int mqAlertThreshold = 500;

bool alertWaterSent = false;
bool alertTempHighSent = false;
bool alertRhLowSent = false;
bool alertRhHighSent = false;
bool alertMqSent = false;

bool awaitingCalibrationVolume = false;
bool fanAuto = true;
int fanPercent = 0;
int fanLastAppliedPercent = -1;

std::vector<String> authorizedChatIds;

bool telegramEnabled = false;

enum ReportFormat { REPORT_COMPACT = 0, REPORT_FULL = 1 };
ReportFormat reportFormat = REPORT_COMPACT;

WiFiClientSecure telegramClient;
UniversalTelegramBot *telegramBot = nullptr;
RTC_DS3231 rtc;
bool rtcReady = false;
const int LIGHTS_ON_HOUR = 6;
const int LIGHTS_ON_MINUTE = 0;
unsigned long lastLightCheckMs = 0;
const int FAN_PWM_CHANNEL = 0;
const int FAN_PWM_FREQ = 25000;
const int FAN_PWM_RES_BITS = 8;
const int FAN_PWM_MAX_DUTY = (1 << FAN_PWM_RES_BITS) - 1;
unsigned long lastFanUpdateMs = 0;
String lastTelegramChatId;
DHT dht(PIN_DHT, DHT22);
float lastDhtTempC = NAN;
float lastDhtRh = NAN;
unsigned long lastDhtReadMs = 0;
bool lastDhtValid = false;

bool lastSerialMessageSent = false;
String lastSerialMessage;
bool lastTelegramMessageSent = false;
String lastTelegramMessage;
String lastTelegramChatIdSent;

// Configuración de zona horaria (por defecto UTC-5, sin horario de verano).
// En la especificación POSIX el valor numérico representa las horas al oeste
// de Greenwich, por lo que se utiliza "GMT5" para obtener UTC-5.
String timezoneInfo = "GMT5";
int timezoneOffsetHours = -5;

// Declaraciones anticipadas para funciones definidas más adelante.
bool syncTimeWithOffset(int offsetHours = -5, unsigned long maxWaitMs = 60000);
bool verifyTelegramToken(uint8_t maxAttempts = 5, uint16_t retryDelayMs = 1000);
void loadRuntimeSettings();
void saveRuntimeSettings();
String formatLightsOffTime();
int getLightHoursForStage(plantStage stage);
int getLightsOffMinutesOfDay(plantStage stage);
void applyLightSchedule();
void initFanPwm();
int computeAutoFanPercent();
void updateFanControl(bool forceApply = false);


// =========================================================
//  FUNCIONES DE UTILIDAD
// =========================================================
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

String formatDateTime(const struct tm &timeinfo) {
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeinfo);
  return String(buffer);
}

bool areLightsOn() { return digitalRead(PIN_RELE2) == LOW; }

bool readAmbient(float &tempC, float &humidity) {
  const unsigned long now = millis();
  const unsigned long minIntervalMs = 2000;

  if (lastDhtValid && now - lastDhtReadMs < minIntervalMs) {
    tempC = lastDhtTempC;
    humidity = lastDhtRh;
    return true;
  }

  tempC = dht.readTemperature();
  humidity = dht.readHumidity();

  if (isnan(tempC) || isnan(humidity)) {
    delay(80);
    tempC = dht.readTemperature();
    humidity = dht.readHumidity();
  }

  if (isnan(tempC) || isnan(humidity)) {
    return false;
  }

  lastDhtTempC = tempC;
  lastDhtRh = humidity;
  lastDhtReadMs = now;
  lastDhtValid = true;
  return true;
}

void initFanPwm() {
  ledcSetup(FAN_PWM_CHANNEL, FAN_PWM_FREQ, FAN_PWM_RES_BITS);
  ledcAttachPin(PIN_FAN_PWM, FAN_PWM_CHANNEL);
  ledcWrite(FAN_PWM_CHANNEL, 0);
  fanLastAppliedPercent = 0;
  lastFanUpdateMs = millis();
}

int computeAutoFanPercent() {
  float tempC = NAN;
  float rh = NAN;
  const bool ambientOk = readAmbient(tempC, rh);
  const int mqReading = analogRead(PIN_MQ135);

  int tempPct = 0;
  if (ambientOk && tempAlertThreshold > 0) {
    const float coolStart = 24.0f;
    const float fullSpeedTemp = max(coolStart + 1.0f, static_cast<float>(tempAlertThreshold));

    if (tempC <= coolStart) {
      tempPct = 0;
    } else if (tempC >= fullSpeedTemp) {
      tempPct = 100;
    } else {
      const float ratio = (tempC - coolStart) / (fullSpeedTemp - coolStart);
      tempPct = constrain(static_cast<int>(ratio * 100.0f + 0.5f), 0, 100);
    }
  }

  int mqPct = 0;
  const int mqStart = max(0, mqAlertThreshold - 50);
  const int mqMax = min(4095, mqAlertThreshold + 400);
  if (mqReading >= mqStart) {
    if (mqReading >= mqMax) {
      mqPct = 100;
    } else {
      mqPct = map(mqReading, mqStart, mqMax, 0, 100);
    }
  }

  return max(tempPct, mqPct);
}

void updateFanControl(bool forceApply) {
  int targetPercent = fanAuto ? computeAutoFanPercent() : fanPercent;
  targetPercent = constrain(targetPercent, 0, 100);

  const unsigned long now = millis();
  if (forceApply || targetPercent != fanLastAppliedPercent) {
    const int duty = map(targetPercent, 0, 100, 0, FAN_PWM_MAX_DUTY);
    ledcWrite(FAN_PWM_CHANNEL, duty);
    fanLastAppliedPercent = targetPercent;
    fanPercent = targetPercent;
    lastFanUpdateMs = now;
  }
}

bool isSkipButtonPressed() { return digitalRead(PIN_CRED_SKIP) == LOW; }

void warnMissingStoredCredentials() {
  if (missingStoredCredsWarned) {
    return;
  }

  Serial.println();
  Serial.println(
      "Botón de salto presionado pero no hay credenciales guardadas. "
      "Ingresa un dato válido.");
  missingStoredCredsWarned = true;
}

void notifyCredentialSkipUse() {
  if (credentialSkipNotified) {
    return;
  }

  Serial.println();
  Serial.println("Botón de salto presionado: usando credenciales guardadas en NVS.");
  credentialSkipNotified = true;
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

      Serial.println("No hay un valor almacenado, ingresa un dato válido.");
      continue;
    }

    return value;
  }
}

void ensureSettingsStore() {
  static bool started = false;
  if (!started) {
    settingsStore.begin("runtime", false);
    started = true;
  }
}

void loadRuntimeSettings() {
  ensureSettingsStore();

  soilDryAdc = constrain(settingsStore.getInt("soilDry", soilDryAdc), 0, 4095);
  soilWetAdc = constrain(settingsStore.getInt("soilWet", soilWetAdc), 0, 4095);

  if (soilDryAdc == soilWetAdc) {
    soilDryAdc = min(soilWetAdc + 1, 4095);
  }

  tempAlertThreshold = constrain(settingsStore.getInt("tempHi", tempAlertThreshold), 1, 100);
  rhLowAlertThreshold = constrain(settingsStore.getInt("rhLow", rhLowAlertThreshold), 1, 100);
  rhHighAlertThreshold = constrain(settingsStore.getInt("rhHigh", rhHighAlertThreshold), 1, 100);
  mqAlertThreshold = max(settingsStore.getInt("mqTh", mqAlertThreshold), 1);

  autoReadingsEnabled = settingsStore.getBool("autoRpt", autoReadingsEnabled);
  autoReadingsIntervalMs = settingsStore.getUInt("autoInt", autoReadingsIntervalMs);
  autoReadingsIntervalMs = max(autoReadingsIntervalMs, 60000UL);

  int storedFormat = settingsStore.getInt("repFmt", static_cast<int>(reportFormat));
  reportFormat = storedFormat == static_cast<int>(REPORT_FULL) ? REPORT_FULL : REPORT_COMPACT;

  if (settingsStore.isKey(kStoredTimezoneOffsetKey)) {
    timezoneOffsetHours = constrain(settingsStore.getInt(kStoredTimezoneOffsetKey, timezoneOffsetHours), -12, 14);
  }
}

void saveRuntimeSettings() {
  ensureSettingsStore();

  settingsStore.putInt("soilDry", soilDryAdc);
  settingsStore.putInt("soilWet", soilWetAdc);
  settingsStore.putInt("tempHi", tempAlertThreshold);
  settingsStore.putInt("rhLow", rhLowAlertThreshold);
  settingsStore.putInt("rhHigh", rhHighAlertThreshold);
  settingsStore.putInt("mqTh", mqAlertThreshold);
  settingsStore.putBool("autoRpt", autoReadingsEnabled);
  settingsStore.putUInt("autoInt", autoReadingsIntervalMs);
  settingsStore.putInt("repFmt", static_cast<int>(reportFormat));
  settingsStore.putInt(kStoredTimezoneOffsetKey, timezoneOffsetHours);
}

plantStage stageFromString(const String &value) {
  String lower = value;
  lower.toLowerCase();

  if (lower == "pl" || lower == "plantula") {
    return PLANTULA;
  }
  if (lower == "veg" || lower == "vegetativo") {
    return VEGETATIVO;
  }
  if (lower == "pre" || lower == "prefloracion") {
    return PRE_FLORACION;
  }
  if (lower == "flo" || lower == "floracion") {
    return FLORACION;
  }
  if (lower == "fin" || lower == "final") {
    return FINAL;
  }

  return getCurrentStage();
}

String stageToString(plantStage stage) {
  switch (stage) {
    case PLANTULA:
      return "Plántula";
    case VEGETATIVO:
      return "Vegetativo";
    case PRE_FLORACION:
      return "Pre-floración";
    case FLORACION:
      return "Floración";
    case FINAL:
      return "Final";
    default:
      return "N/D";
  }
}

String stageToCode(plantStage stage) {
  switch (stage) {
    case PLANTULA:
      return "P";
    case VEGETATIVO:
      return "V";
    case PRE_FLORACION:
      return "PF";
    case FLORACION:
      return "F";
    case FINAL:
      return "FN";
    default:
      return "?";
  }
}

String formatLastIrrigation() {
  const unsigned long lastEpoch = getLastIrrigationEpoch();
  if (lastEpoch == 0) {
    return "Sin registro";
  }

  struct tm timeinfo;
  time_t ts = static_cast<time_t>(lastEpoch);
  if (localtime_r(&ts, &timeinfo) == nullptr) {
    return "Sin registro";
  }

  char buffer[20];
  strftime(buffer, sizeof(buffer), "%H:%M %d/%m/%Y", &timeinfo);
  return String(buffer);
}

String formatShortLastIrrigation() {
  const unsigned long lastEpoch = getLastIrrigationEpoch();
  if (lastEpoch == 0) {
    return "Sin registro";
  }

  struct tm timeinfo;
  time_t ts = static_cast<time_t>(lastEpoch);
  if (localtime_r(&ts, &timeinfo) == nullptr) {
    return "Sin registro";
  }

  char buffer[12];
  strftime(buffer, sizeof(buffer), "%H:%M %d/%m", &timeinfo);
  return String(buffer);
}

String formatIrrigationConfig() {
  struct tm timeinfo;
  String nowStr = "Sin hora";
  if (getLocalTime(&timeinfo)) {
    nowStr = formatDateTime(timeinfo);
  }

  const float potVolumeL = getPotVolumeL();
  const float stageMl = getMlPerLiterForStage(getCurrentStage()) * potVolumeL;

  String msg;
  msg += "====Configuración del invernadero====\n";
  msg += "Etapa actual: " + stageToString(getCurrentStage()) + "\n";
  msg += "mL/L etapa actual: " + String(getMlPerLiterForStage(getCurrentStage())) + " mL\n";
  msg += "mL calculados para la maceta: " + String(stageMl, 0) + " mL\n";
  msg += "mL/L por etapa: Plántula=" + String(getMlPerLiterForStage(PLANTULA)) +
         ", Vegetativo=" + String(getMlPerLiterForStage(VEGETATIVO)) +
         ", Pre-floración=" + String(getMlPerLiterForStage(PRE_FLORACION)) +
         ", Floración=" + String(getMlPerLiterForStage(FLORACION)) +
         ", Final=" + String(getMlPerLiterForStage(FINAL)) + "\n";
  msg += "Último riego: " + formatLastIrrigation() + "\n";
  msg += "Riego automático: " + String(isAutoIrrigationEnabled() ? "ON" : "OFF") + "\n";
  msg += "Maceta: " + String(potVolumeL, 1) + " L\n";
  msg += "Intervalo mínimo entre riegos: " + String(getIrrigationIntervalDays()) + " días\n";
  msg += "Suelo: mínimo " + String(getSoilThreshold()) + "%, máximo " + String(getSoilHighThreshold()) + "%\n";
  msg += "Alertas: Temp máx=" + String(tempAlertThreshold) + "°C, HR min=" + String(rhLowAlertThreshold) +
         "%, HR máx=" + String(rhHighAlertThreshold) + "%, MQ máx=" + String(mqAlertThreshold) + "\n";
  msg += "Hora local: " + nowStr;
  return msg;
}

void printIrrigationConfig() { Serial.println(formatIrrigationConfig()); }

int soilPercentFromAdc(int reading) {
  const int minReading = min(soilWetAdc, soilDryAdc);
  const int maxReading = max(soilWetAdc, soilDryAdc);
  int clampedReading = constrain(reading, minReading, maxReading);
  int percent = map(clampedReading, soilWetAdc, soilDryAdc, 100, 0);
  return constrain(percent, 0, 100);
}

String formatStatus() {
  float ambientTemp = NAN;
  float ambientRh = NAN;
  bool ambientOk = readAmbient(ambientTemp, ambientRh);

  const int mqReading = analogRead(PIN_MQ135);
  const int soilAdc = readSoilMoisture();
  const int soilPercent = soilPercentFromAdc(soilAdc);
  const bool waterAvailable = isTankWaterAvailable();
  const float stageMl = getMlPerLiterForStage(getCurrentStage()) * getPotVolumeL();

  String stageStr = stageToString(getCurrentStage());
  stageStr.toUpperCase();

  String msg;
  msg += "=======ESTADO DEL INVERNADERO======\n";
  msg += "Temperatura: ";
  msg += ambientOk ? String(ambientTemp, 1) + "°C" : "N/D";
  msg += " | Humedad Relativa: ";
  msg += ambientOk ? String(ambientRh, 0) + "%" : "N/D";
  msg += " | MQ: " + String(mqReading) + "\n";

  msg += "Humedad del suelo: " + String(soilPercent) + "% (ADC: " + String(soilAdc) + ") | FAN: " + String(fanPercent) + "% [" +
         String(fanAuto ? "AUTO" : "MANUAL") + "] \n";

  msg += "Etapa: [" + stageStr + "] | Luz: [" + String(areLightsOn() ? "ON" : "OFF") + "] (OFF [" + formatLightsOffTime() + "]) | Agua: [" +
         String(waterAvailable ? "SI" : "NO") + "]\n";

  msg += "Ultimo riego: " + formatLastIrrigation() + "  | mL: [" + String(stageMl, 0) + "] mL\n";

  msg += "Riego: [" + String(isAutoIrrigationEnabled() ? "AUTO" : "MANUAL") + "] | Reportes: [" +
         String(autoReadingsEnabled ? "ON" : "OFF") + "]";
  return msg;
}

String formatCompactReport() {
  float ambientTemp = NAN;
  float ambientRh = NAN;
  bool ambientOk = readAmbient(ambientTemp, ambientRh);

  const int mqReading = analogRead(PIN_MQ135);
  const int soilAdc = readSoilMoisture();
  const int soilPercent = soilPercentFromAdc(soilAdc);
  const bool waterAvailable = isTankWaterAvailable();
  const float stageMl = getMlPerLiterForStage(getCurrentStage()) * getPotVolumeL();

  String msg;
  msg += "==GH==\n";
  if (ambientOk) {
    msg += "T:" + String(ambientTemp, 0) + "°C HR:" + String(ambientRh, 0) + "% MQ:" + String(mqReading) + "\n";
  } else {
    msg += "T:N/D HR:N/D MQ:" + String(mqReading) + "\n";
  }

  msg += "Soil: " + String(soilPercent) + "% FAN: " + String(fanPercent) + "% " + String(fanAuto ? "A" : "M") + "\n";

  msg += "E:" + stageToCode(getCurrentStage()) + " L:" + String(areLightsOn() ? "ON" : "OFF") + " A:" + String(waterAvailable ? "SI" : "NO") +
         "  mL:" + String(stageMl, 0) + " \n";

  msg += formatShortLastIrrigation();
  return msg;
}

String formatReportMessage() { return reportFormat == REPORT_COMPACT ? formatCompactReport() : formatStatus(); }


bool stageSupportsLowRhAlerts(plantStage stage) {
  return stage == PLANTULA || stage == VEGETATIVO;
}

bool stageSupportsHighRhAlerts(plantStage stage) {
  return stage == PRE_FLORACION || stage == FLORACION || stage == FINAL;
}

bool stageUsesLeds(plantStage stage) {
  return stage == PRE_FLORACION || stage == FLORACION || stage == FINAL;
}

void evaluateAlerts() {
  const bool waterAvailable = isTankWaterAvailable();
  if (!waterAvailable && !alertWaterSent) {
    broadcastMessage("ALERTA: tanque sin agua");
    alertWaterSent = true;
  } else if (waterAvailable) {
    alertWaterSent = false;
  }

  float tempC = NAN;
  float rh = NAN;
  const bool ambientOk = readAmbient(tempC, rh);
  plantStage stage = getCurrentStage();

  if (ambientOk) {
    const bool tempHigh = tempAlertThreshold > 0 && tempC >= tempAlertThreshold;
    if (tempHigh && !alertTempHighSent) {
      broadcastMessage("ALERTA: temperatura alta (" + String(tempC, 1) + "°C >= " + String(tempAlertThreshold) + "°C)");
      alertTempHighSent = true;
    } else if (!tempHigh) {
      alertTempHighSent = false;
    }

    const bool lowRhActive = stageSupportsLowRhAlerts(stage);
    const bool highRhActive = stageSupportsHighRhAlerts(stage);

    const bool rhTooLow = lowRhActive && rhLowAlertThreshold > 0 && rh < rhLowAlertThreshold;
    if (rhTooLow && !alertRhLowSent) {
      broadcastMessage("ALERTA: humedad ambiente baja (" + String(rh, 0) + "% < " + String(rhLowAlertThreshold) + "%)");
      alertRhLowSent = true;
    } else if (!rhTooLow || !lowRhActive) {
      alertRhLowSent = false;
    }

    const bool rhTooHigh = highRhActive && rhHighAlertThreshold > 0 && rh > rhHighAlertThreshold;
    if (rhTooHigh && !alertRhHighSent) {
      broadcastMessage("ALERTA: humedad ambiente alta (" + String(rh, 0) + "% > " + String(rhHighAlertThreshold) + "%)");
      alertRhHighSent = true;
    } else if (!rhTooHigh || !highRhActive) {
      alertRhHighSent = false;
    }
  } else {
    alertTempHighSent = false;
    alertRhLowSent = false;
    alertRhHighSent = false;
  }

  const int mqReading = analogRead(PIN_MQ135);
  const bool poorAir = mqAlertThreshold > 0 && mqReading >= mqAlertThreshold;
  if (poorAir && !alertMqSent) {
    broadcastMessage("ALERTA: aire pobre detectado (MQ=" + String(mqReading) + " >= " + String(mqAlertThreshold) + ")");
    alertMqSent = true;
  } else if (!poorAir) {
    alertMqSent = false;
  }
}

String tzFromOffset(int offsetHours) {
  if (offsetHours == 0) {
    return "GMT";
  }

  if (offsetHours > 0) {
    return "GMT-" + String(offsetHours);
  }

  return "GMT" + String(abs(offsetHours));
}

int promptTimezoneOffset(int defaultOffset) {
  ensureSettingsStore();

  const bool hasStoredOffset = settingsStore.isKey(kStoredTimezoneOffsetKey);
  const int storedOffset = constrain(settingsStore.getInt(kStoredTimezoneOffsetKey, defaultOffset), -12, 14);

  if (skipCredentialPrompt && hasStoredOffset) {
    Serial.println();
    Serial.println("Botón de salto: usando offset guardado en NVS.");
    return storedOffset;
  }

  Serial.println();
  Serial.println("Zona horaria: ingresa el offset UTC en horas (ej: -5, -7, +4).");
  Serial.print("Valor actual ");
  Serial.print(defaultOffset);
  Serial.println(". Presiona Enter para mantenerlo.");

  while (true) {
    if (!skipCredentialPrompt && isSkipButtonPressed()) {
      if (hasStoredOffset) {
        notifyCredentialSkipUse();
        skipCredentialPrompt = true;
        Serial.println();
        Serial.println("Botón de salto: usando offset guardado en NVS.");
        return storedOffset;
      }

      Serial.println();
      Serial.println("Botón de salto presionado pero no hay offset guardado. Ingresa un valor válido.");
    }

    String input = readLineFromSerial("> ", 20000, true);
    input.trim();

    if (skipCredentialPrompt && hasStoredOffset && input.isEmpty()) {
      Serial.println("Usando offset almacenado en NVS.");
      return storedOffset;
    }

    if (input.isEmpty()) {
      Serial.println("Usando offset existente.");
      settingsStore.putInt(kStoredTimezoneOffsetKey, defaultOffset);
      return defaultOffset;
    }

    int offset = input.toInt();
    offset = constrain(offset, -12, 14);
    Serial.print("Offset seleccionado: ");
    Serial.println(offset);

    settingsStore.putInt(kStoredTimezoneOffsetKey, offset);
    return offset;
  }
}

String handleIrrigationCommand(const String &rawLine, bool &updatedConfig,
                               bool &updatedRuntime) {
  String line = rawLine;
  line.trim();
  if (line.isEmpty()) {
    return "";
  }

  String lower = line;
  lower.toLowerCase();
  updatedConfig = false;
  updatedRuntime = false;

  if (lower.startsWith("stage")) {
    int spaceIndex = lower.indexOf(' ');
    if (spaceIndex > 0) {
      String stageToken = lower.substring(spaceIndex + 1);
      stageToken.trim();
      plantStage newStage = stageFromString(stageToken);
      updateStage(newStage);
      updatedConfig = true;
      return "Etapa actualizada a " + stageToken;
    }
    return "Etapas disponibles: plantula (pl), vegetativo (veg), pre-floracion (pre), floracion (flo), final (fin). Usa: stage <etapa>";
  } else if (lower == "status") {
    return formatStatus();
  } else if (lower.startsWith("ml")) {
    int firstSpace = lower.indexOf(' ');
    int secondSpace = lower.indexOf(' ', firstSpace + 1);
    if (firstSpace > 0 && secondSpace > firstSpace) {
      String stageToken = lower.substring(firstSpace + 1, secondSpace);
      String valueToken = lower.substring(secondSpace + 1);
      plantStage stage = stageFromString(stageToken);
      int value = valueToken.toInt();
      if (!setMlPerLiterForStage(stage, value)) {
        return "Valor mL/L fuera de rango (5-200).";
      }
      updatedConfig = true;
      return "mL/L actualizado para etapa " + stageToken + ": " + String(value);
    }
  } else if (lower.startsWith("pot")) {
    int spaceIndex = lower.indexOf(' ');
    if (spaceIndex > 0) {
      float liters = lower.substring(spaceIndex + 1).toFloat();
      if (!setPotVolumeL(liters)) {
        return "Capacidad de maceta fuera de rango (1-50 L).";
      }
      updatedConfig = true;
      return "Volumen de maceta actualizado: " + String(liters);
    }
  } else if (lower.startsWith("flow")) {
    int spaceIndex = lower.indexOf(' ');
    if (spaceIndex > 0) {
      float flow = lower.substring(spaceIndex + 1).toFloat();
      if (!setPumpFlow(flow)) {
        return "Caudal inválido (1-50 mL/s).";
      }
      updatedConfig = true;
      return "Caudal de bomba actualizado: " + String(flow);
    }
  } else if (lower.startsWith("soilmax")) {
    int spaceIndex = lower.indexOf(' ');
    if (spaceIndex > 0) {
      int threshold = lower.substring(spaceIndex + 1).toInt();
      if (!setSoilHighThreshold(threshold)) {
        return "Umbral de humedad alta fuera de rango (50-100%).";
      }
      updatedConfig = true;
      return "Umbral de humedad alta actualizado: " + String(threshold) + "%";
    }
  } else if (lower.startsWith("soil")) {
    int spaceIndex = lower.indexOf(' ');
    if (spaceIndex > 0) {
      int threshold = lower.substring(spaceIndex + 1).toInt();
      if (!setSoilThreshold(threshold)) {
        return "Umbral de suelo fuera de rango (0-50%).";
      }
      updatedConfig = true;
      return "Umbral de suelo actualizado: " + String(threshold) + "%";
    }
  } else if (lower.startsWith("interval")) {
    int spaceIndex = lower.indexOf(' ');
    if (spaceIndex > 0) {
      int days = lower.substring(spaceIndex + 1).toInt();
      if (!setIrrigationIntervalDays(days)) {
        return "Intervalo de riego fuera de rango (1-5 días).";
      }
      updatedConfig = true;
      return "Intervalo mínimo entre riegos actualizado a " + String(days) + " días";
    }
  } else if (lower.startsWith("temp_max")) {
    int spaceIndex = lower.indexOf(' ');
    if (spaceIndex > 0) {
      int val = lower.substring(spaceIndex + 1).toInt();
      if (val <= 0) {
        return "Umbral temp alta inválido (usa C enteros).";
      }
      tempAlertThreshold = constrain(val, 1, 100);
      updatedRuntime = true;
      return "Umbral temp alta: " + String(tempAlertThreshold) + "°C";
    }
  } else if (lower.startsWith("hum_min")) {
    int spaceIndex = lower.indexOf(' ');
    if (spaceIndex > 0) {
      int val = lower.substring(spaceIndex + 1).toInt();
      if (val <= 0) {
        return "Umbral humedad baja inválido (1-100%).";
      }
      rhLowAlertThreshold = constrain(val, 1, 100);
      updatedRuntime = true;
      return "Umbral humedad ambiente baja: " + String(rhLowAlertThreshold) + "%";
    }
  } else if (lower.startsWith("hum_max")) {
    int spaceIndex = lower.indexOf(' ');
    if (spaceIndex > 0) {
      int val = lower.substring(spaceIndex + 1).toInt();
      if (val <= 0) {
        return "Umbral humedad alta inválido (1-100%).";
      }
      rhHighAlertThreshold = constrain(val, 1, 100);
      updatedRuntime = true;
      return "Umbral humedad ambiente alta: " + String(rhHighAlertThreshold) + "%";
    }
  } else if (lower.startsWith("aire_max")) {
    int spaceIndex = lower.indexOf(' ');
    if (spaceIndex > 0) {
      int val = lower.substring(spaceIndex + 1).toInt();
      if (val <= 0) {
        return "Umbral MQ inválido (usa enteros positivos).";
      }
      mqAlertThreshold = max(val, 1);
      updatedRuntime = true;
      return "Umbral MQ: " + String(mqAlertThreshold);
    }
  } else if (lower.startsWith("luz")) {
    int firstSpace = lower.indexOf(' ');
    int secondSpace = lower.indexOf(' ', firstSpace + 1);
    if (firstSpace > 0 && secondSpace > firstSpace) {
      String stageToken = lower.substring(firstSpace + 1, secondSpace);
      String valueToken = lower.substring(secondSpace + 1);
      plantStage stage = stageFromString(stageToken);
      int hours = valueToken.toInt();
      if (stage == PRE_FLORACION || stage == FLORACION || stage == FINAL) {
        return "Pre-floración, floración y final están fijas en 12 h y no se pueden editar.";
      }
      if (!setLightHoursForStage(stage, hours)) {
        return "Horas de luz fuera de rango (12-20 h) para plántula/vegetativo.";
      }
      updatedConfig = true;
      return "Horas de luz actualizadas para etapa " + stageToken + ": " + String(hours) + " h";
    }
  } else if (lower == "reset") {
    configReset();
    updatedConfig = true;
    return "Configuración de riego restablecida a valores por defecto.";
    } else if (lower == "conf") {
      return formatIrrigationConfig();
    }

    return "Comandos: stage <plantula|vegetativo|pre-floracion|floracion|final>, ml <etapa> <valor>, luz <plantula|vegetativo> <horas>, pot <L>, flow <mL/s>, soil <pct>, soilmax <pct>, interval <dias>, temp_max <C>, hum_min <pct>, hum_max <pct>, aire_max <N>, status, reset, conf";
  }

void handleSerialCommands() {
  if (!Serial.available()) {
    return;
  }

  String line = Serial.readStringUntil('\n');
  bool updatedConfig = false;
  bool updatedRuntime = false;
  String response = handleIrrigationCommand(line, updatedConfig, updatedRuntime);

  if (!response.isEmpty()) {
    Serial.println(response);
  }

  if (updatedConfig) {
    configSave();
  }
  if (updatedRuntime) {
    saveRuntimeSettings();
  }

  if (updatedConfig || updatedRuntime) {
    printIrrigationConfig();
  }
}

void broadcastMessage(const String &msg) {
  if (msg.isEmpty()) {
    return;
  }

  if (!lastSerialMessageSent || msg != lastSerialMessage) {
    Serial.println(msg);
    lastSerialMessage = msg;
    lastSerialMessageSent = true;
  }

  if (telegramEnabled && WiFi.status() == WL_CONNECTED && telegramBot != nullptr && !lastTelegramChatId.isEmpty()) {
    if (!lastTelegramMessageSent || msg != lastTelegramMessage || lastTelegramChatId != lastTelegramChatIdSent) {
      telegramBot->sendMessage(lastTelegramChatId, msg, "");
      lastTelegramMessage = msg;
      lastTelegramChatIdSent = lastTelegramChatId;
      lastTelegramMessageSent = true;
    }
  }
}

bool parseOnOff(const String &value) {
  String lower = value;
  lower.toLowerCase();
  return lower == "on" || lower == "1" || lower == "true";
}

bool applyReportFormatToken(const String &value) {
  String lower = value;
  lower.toLowerCase();

  if (lower == "compact") {
    reportFormat = REPORT_COMPACT;
    return true;
  }

  if (lower == "all" || lower == "todo" || lower == "full") {
    reportFormat = REPORT_FULL;
    return true;
  }

  return false;
}

String commandHelp() {
  String help;
  help += "== Uso diario ==\n";
  help += "/estado - Ver estado del invernadero\n";
  help += "/regar [mL] - Riego manual\n";
  help += "/autoriego [on|off] - Riego automático\n";
  help += "/vent [0-100] - Ventilador manual\n";
  help += "/ventauto [on|off] - Ventilador automático\n";
  help += "/reportes [on|off] [min] [compact|all]\n";
  help += "\n== Configuración ==\n";
  help += "/config - Ver configuración\n";
  help += "/etapa [pl|veg|pre|flo|fin]\n";
  help += "/maceta [litros]\n";
  help += "/luz [etapa] [horas]\n";
  help += "/pausariego [dias] - Intervalo entre riegos\n";
  help += "/suelomin [%] - Umbral mínimo suelo\n";
  help += "/suelomax [%] - Umbral máximo suelo\n";
  help += "/calsuelo [SECO] [HUMEDO] - Calibrar sensor\n";
  help += "\n== Alertas ==\n";
  help += "/tempmax [C] /hummin [%] /hummax [%] /airemax [N]\n";
  help += "\n== Bomba ==\n";
  help += "/calibrar - Activar bomba 5s para medir\n";
  help += "/caudal [mL] - Guardar volumen medido\n";
  help += "\n== Admin ==\n";
  help += "/addid [ID] /delid [ID] /ids";
  return help;
}

String handleTelegramCommand(const String &chatId, const String &text, bool &updatedConfig) {
  updatedConfig = false;
  String cmd = text;
  cmd.trim();

  if (cmd.isEmpty()) return "";

  int space = cmd.indexOf(' ');
  String base = space == -1 ? cmd : cmd.substring(0, space);
  String args = space == -1 ? "" : cmd.substring(space + 1);
  base.toLowerCase();

  if (base == "/start") {
    return commandHelp();
  }

  if (!ensureChatAuthorized(chatId)) {
    return "Chat no autorizado (máx 5 IDs).";
  }

  if (base == "/estado") {
    return formatStatus();
  }

  if (base == "/maceta") {
    float liters = args.toFloat();
    if (liters <= 0) return "Uso: /maceta [litros]";
    if (!setPotVolumeL(liters)) return "Capacidad inválida (1-50 L).";
    updatedConfig = true;
    return "Capacidad de maceta actualizada a " + String(liters) + " L";
  }

  if (base == "/etapa") {
    if (args.isEmpty()) return "Uso: /etapa [plantula|vegetativo|pre-floracion|floracion|final]";
    updateStage(stageFromString(args));
    updatedConfig = true;
    return "Etapa cambiada a " + args;
  }

  if (base == "/luz") {
    int spaceIdx = args.indexOf(' ');
    if (spaceIdx == -1) return "Uso: /luz [etapa] [horas]";
    String stageToken = args.substring(0, spaceIdx);
    int hours = args.substring(spaceIdx + 1).toInt();
    plantStage stage = stageFromString(stageToken);
    if (stage == PRE_FLORACION || stage == FLORACION || stage == FINAL) {
      return "Pre-floración, floración y final están fijas en 12 h y no se pueden editar.";
    }
    if (!setLightHoursForStage(stage, hours)) return "Horas de luz inválidas (12-20 h) para plántula/vegetativo.";
    updatedConfig = true;
    return "Horas de luz para " + stageToken + ": " + String(hours) + " h";
  }

  if (base == "/calsuelo") {
    int space2 = args.indexOf(' ');
    if (space2 == -1) return "Uso: /calsuelo [SECO] [HUMEDO]";
    soilDryAdc = constrain(args.substring(0, space2).toInt(), 0, 4095);
    soilWetAdc = constrain(args.substring(space2 + 1).toInt(), 0, 4095);
    if (soilDryAdc <= soilWetAdc) {
      soilDryAdc = min(soilWetAdc + 1, 4095);
    }
    updatedConfig = true;
    return "Calibración suelo actualizada. Seco=" + String(soilDryAdc) + " húmedo=" + String(soilWetAdc);
  }

  if (base == "/suelomax") {
    int pct = args.toInt();
    if (pct <= 0) return "Uso: /suelomax [%]";
    if (!setSoilHighThreshold(pct)) return "Umbral de humedad alta inválido (50-100%).";
    updatedConfig = true;
    return "Umbral de humedad alta fijado en " + String(pct) + "%";
  }

  if (base == "/suelomin") {
    int pct = args.toInt();
    if (pct <= 0) return "Uso: /suelomin [%]";
    if (!setSoilThreshold(pct)) return "Umbral de suelo inválido (0-50%).";
    updatedConfig = true;
    return "Umbral mínimo de humedad fijado en " + String(pct) + "%";
  }

  if (base == "/pausariego") {
    int days = args.toInt();
    if (days <= 0) return "Uso: /pausariego [dias]";
    if (!setIrrigationIntervalDays(days)) return "Intervalo entre riegos inválido (1-5 días).";
    updatedConfig = true;
    return "Intervalo entre riegos fijado en " + String(days) + " días";
  }

  if (base == "/tempmax") {
    int val = args.toInt();
    if (val <= 0) return "Uso: /tempmax [C]";
    tempAlertThreshold = constrain(val, 1, 100);
    updatedConfig = true;
    return "Umbral temp alta: " + String(tempAlertThreshold) + " C";
  }

  if (base == "/hummin") {
    int val = args.toInt();
    if (val <= 0) return "Uso: /hummin [%]";
    rhLowAlertThreshold = constrain(val, 1, 100);
    updatedConfig = true;
    return "Umbral humedad ambiente baja: " + String(rhLowAlertThreshold) + "%";
  }

  if (base == "/hummax") {
    int val = args.toInt();
    if (val <= 0) return "Uso: /hummax [%]";
    rhHighAlertThreshold = constrain(val, 1, 100);
    updatedConfig = true;
    return "Umbral humedad ambiente alta: " + String(rhHighAlertThreshold) + "%";
  }

  if (base == "/airemax") {
    int val = args.toInt();
    if (val <= 0) return "Uso: /airemax [N]";
    mqAlertThreshold = max(val, 1);
    updatedConfig = true;
    return "Umbral MQ: " + String(mqAlertThreshold);
  }

  if (base == "/config") {
    return formatIrrigationConfig();
  }

  if (base == "/calibrar") {
    awaitingCalibrationVolume = true;
    pumpOn();
    delay(5000);
    pumpOff();
    return "Bomba activada 5s. Envía /caudal [mL] con el volumen medido.";
  }

  if (base == "/caudal") {
    float measuredMl = args.toFloat();
    if (measuredMl <= 0) return "Envía el volumen medido en mL.";
    float newFlow = measuredMl / 5.0f;
    if (!setPumpFlow(newFlow)) return "Caudal calculado fuera de rango (1-50 mL/s).";
    updatedConfig = true;
    awaitingCalibrationVolume = false;
    setAutoIrrigationEnabled(false);
    return "Caudal calculado: " + String(newFlow) +
           " mL/s. Envía /autoriego on para activar el riego automático.";
  }

  if (base == "/autoriego") {
    if (args.isEmpty()) return String("Riego automático está ") + (isAutoIrrigationEnabled() ? "ON" : "OFF");
    setAutoIrrigationEnabled(parseOnOff(args));
    return String("Riego automático ") + (isAutoIrrigationEnabled() ? "activado" : "desactivado");
  }

  if (base == "/ventauto") {
    if (args.isEmpty()) return String("Ventilador automático está ") + (fanAuto ? "ON" : "OFF");
    fanAuto = parseOnOff(args);
    updateFanControl(true);
    return String("Ventilador automático ") + (fanAuto ? "ON" : "OFF");
  }

  if (base == "/vent") {
    int pct = args.toInt();
    pct = constrain(pct, 0, 100);
    fanPercent = pct;
    fanAuto = false;
    updateFanControl(true);
    return "Ventilador en manual a " + String(pct) + "%";
  }

  if (base == "/regar") {
    float ml = args.toFloat();
    if (ml <= 0) return "Uso: /regar [mL]";
    ml = min(ml, 1500.0f);
    if (!isPumpCalibrated()) return "Bomba sin calibrar. Ejecuta /calibrar antes de regar.";
    if (!isTankWaterAvailable()) return "Tanque sin agua. Verifica el nivel del tanque.";
    irrigateVolume(ml, readSoilMoisture());
    return "Riego manual por " + String(ml) + " mL";
  }

  if (base == "/reportes") {
    if (args.isEmpty()) {
      String summary = String("Reportes ") + (autoReadingsEnabled ? "ON" : "OFF");
      summary += " cada " + String(autoReadingsIntervalMs / 60000) + " min";
      summary += " (" + String(reportFormat == REPORT_COMPACT ? "Compact" : "All") + ")";
      return summary;
    }

    std::vector<String> parts;
    int startIdx = 0;
    while (startIdx < args.length()) {
      int spaceIdx = args.indexOf(' ', startIdx);
      if (spaceIdx == -1) spaceIdx = args.length();
      String token = args.substring(startIdx, spaceIdx);
      token.trim();
      if (!token.isEmpty()) {
        parts.push_back(token);
      }
      startIdx = spaceIdx + 1;
    }

    if (!parts.empty()) {
      autoReadingsEnabled = parseOnOff(parts[0]);
    }

    if (parts.size() >= 2) {
      if (!applyReportFormatToken(parts[1])) {
        int mins = parts[1].toInt();
        if (mins > 0) autoReadingsIntervalMs = mins * 60000UL;
      }
    }

    if (parts.size() >= 3) {
      applyReportFormatToken(parts[2]);
    }

    saveRuntimeSettings();

    String summary = String("Reportes ") + (autoReadingsEnabled ? "activados" : "desactivados");
    summary += " cada " + String(autoReadingsIntervalMs / 60000) + " min";
    summary += " (" + String(reportFormat == REPORT_COMPACT ? "Compact" : "All") + ")";
    return summary;
  }

  if (base == "/addid") {
    if (authorizedChatIds.size() >= 5) return "Máximo de IDs alcanzado.";
    if (args.isEmpty()) return "Uso: /addid [ID]";
    authorizedChatIds.push_back(args);
    persistAuthorizedChatIds();
    return "ID agregado.";
  }

  if (base == "/delid") {
    if (args.isEmpty()) return "Uso: /delid [ID]";
    for (auto it = authorizedChatIds.begin(); it != authorizedChatIds.end(); ++it) {
      if (*it == args) {
        authorizedChatIds.erase(it);
        persistAuthorizedChatIds();
        return "ID eliminado.";
      }
    }
    return "ID no encontrado.";
  }

  if (base == "/ids") {
    String ids = "IDs autorizados:\n";
    for (size_t i = 0; i < authorizedChatIds.size(); i++) {
      ids += String(i + 1) + ": " + authorizedChatIds[i] + "\n";
    }
    return ids;
  }

  return "Comando no reconocido. Usa /start para ayuda.";
}

int getLightsOffMinutesOfDay(plantStage stage) {
  const int totalMinutes = LIGHTS_ON_HOUR * 60 + LIGHTS_ON_MINUTE + getLightHoursForStage(stage) * 60;
  return totalMinutes % (24 * 60);
}

String formatLightsOffTime() {
  const int offMinutes = getLightsOffMinutesOfDay(getCurrentStage());
  const int offHour = offMinutes / 60;
  const int offMinute = offMinutes % 60;
  char buffer[6];
  snprintf(buffer, sizeof(buffer), "%02d:%02d", offHour, offMinute);
  return String(buffer);
}

void applyLightSchedule() {
  struct tm nowInfo;
  if (!getLocalTime(&nowInfo)) {
    return;
  }

  struct tm startInfo = nowInfo;
  startInfo.tm_hour = LIGHTS_ON_HOUR;
  startInfo.tm_min = LIGHTS_ON_MINUTE;
  startInfo.tm_sec = 0;

  const time_t nowTs = mktime(&nowInfo);
  const time_t startTs = mktime(&startInfo);
  const plantStage currentStage = getCurrentStage();
  const time_t offTs = startTs + getLightHoursForStage(currentStage) * 3600L;

  const bool shouldBeOn = nowTs >= startTs && nowTs < offTs;
  const int desiredLevel = shouldBeOn ? LOW : HIGH;

  if (digitalRead(PIN_RELE2) != desiredLevel) {
    digitalWrite(PIN_RELE2, desiredLevel);
  }

  const bool ledsShouldBeOn = shouldBeOn && stageUsesLeds(currentStage);
  const int desiredLedLevel = ledsShouldBeOn ? HIGH : LOW;

  if (digitalRead(PIN_LED_MOSFET) != desiredLedLevel) {
    digitalWrite(PIN_LED_MOSFET, desiredLedLevel);
  }
}

void pollTelegram() {
  const unsigned long now = millis();
  if (!telegramEnabled || WiFi.status() != WL_CONNECTED || telegramBot == nullptr) {
    return;
  }

  if (now - lastTelegramPollMs < 1500) {
    return;
  }

  lastTelegramPollMs = now;
  int numNewMessages = telegramBot->getUpdates(telegramBot->last_message_received + 1);

  while (numNewMessages) {
    for (int i = 0; i < numNewMessages; i++) {
      String chatId = telegramBot->messages[i].chat_id;
      String text = telegramBot->messages[i].text;
      lastTelegramChatId = chatId;

      bool updated = false;
      String response = handleTelegramCommand(chatId, text, updated);
      telegramBot->sendMessage(chatId, response, "");

      if (updated) {
        configSave();
        saveRuntimeSettings();
      }
    }

    numNewMessages = telegramBot->getUpdates(telegramBot->last_message_received + 1);
  }
}

void sendPeriodicStatusIfNeeded() {
  if (!telegramEnabled || !autoReadingsEnabled || telegramBot == nullptr) {
    return;
  }

  unsigned long now = millis();
  if (now - lastAutoReadingMs < autoReadingsIntervalMs) {
    return;
  }

  lastAutoReadingMs = now;
  String targetChat = !lastTelegramChatId.isEmpty() ? lastTelegramChatId : (authorizedChatIds.empty() ? String("") : authorizedChatIds.front());

  if (targetChat.isEmpty()) {
    return;
  }

  telegramBot->sendMessage(targetChat, formatReportMessage(), "");
}

void loadStoredCredentials() {
  credentialsStore.begin("cred", false);
  storedWifiSsid = credentialsStore.getString("ssid", "");
  storedWifiPassword = credentialsStore.getString("pass", "");
  storedTelegramToken = credentialsStore.getString("token", "");
  loadAuthorizedChatIds();
}

bool hasStoredCredentials() {
  return !storedWifiSsid.isEmpty() && !storedWifiPassword.isEmpty() &&
         !storedTelegramToken.isEmpty();
}

void saveWifiCredentials(const String &ssid, const String &password) {
  credentialsStore.putString("ssid", ssid);
  credentialsStore.putString("pass", password);
  storedWifiSsid = ssid;
  storedWifiPassword = password;
}

void saveTelegramToken(const String &token) {
  credentialsStore.putString("token", token);
  storedTelegramToken = token;
}

void persistAuthorizedChatIds() {
  String serialized;
  for (size_t i = 0; i < authorizedChatIds.size(); i++) {
    serialized += authorizedChatIds[i];
    if (i + 1 < authorizedChatIds.size()) {
      serialized += ',';
    }
  }

  credentialsStore.putString("ids", serialized);
}

void loadAuthorizedChatIds() {
  authorizedChatIds.clear();
  String stored = credentialsStore.getString("ids", "");

  int start = 0;
  while (start < stored.length()) {
    int comma = stored.indexOf(',', start);
    if (comma == -1) comma = stored.length();
    String id = stored.substring(start, comma);
    id.trim();
    if (!id.isEmpty()) {
      authorizedChatIds.push_back(id);
    }
    start = comma + 1;
  }
}

bool isChatAuthorized(const String &chatId) {
  for (const auto &id : authorizedChatIds) {
    if (id == chatId) {
      return true;
    }
  }
  return false;
}

bool ensureChatAuthorized(const String &chatId) {
  if (isChatAuthorized(chatId)) {
    return true;
  }

  if (authorizedChatIds.size() < 4) {
    authorizedChatIds.push_back(chatId);
    persistAuthorizedChatIds();
    return true;
  }

  return false;
}


// =========================================================
//  SOLICITAR CREDENCIALES
// =========================================================
void requestCredentials() {
  const uint32_t promptTimeoutMs = 30000;  // 30 segundos para cada valor

  wifiSsid = promptOrStoredValue("WiFi SSID:", storedWifiSsid, promptTimeoutMs);
  wifiPassword = promptOrStoredValue("WiFi Password:", storedWifiPassword, promptTimeoutMs);
  telegramToken = promptOrStoredValue("Token Telegram:", storedTelegramToken, promptTimeoutMs);

  Serial.println();
  Serial.println("=========== CONFIGURACIÓN INICIAL ===========");
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


// =========================================================
//  INICIALIZACIÓN DE RED Y SERVICIOS
// =========================================================
bool connectToWifi(const String &ssid, const String &password, unsigned long timeoutMs = 30000) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  Serial.print("Conectando a WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start >= timeoutMs) {
      Serial.println();
      Serial.println("Timeout: no se pudo conectar a WiFi.");
      return false;
    }
    delay(500);
    Serial.print('.');
  }

  Serial.println();
  Serial.println("WiFi conectado.");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  saveWifiCredentials(ssid, password);
  return true;
}

void configureTelegramTransport() {
  telegramClient.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  telegramClient.setTimeout(15000);
}

bool ensureTimeReady(int offsetHours) {
  timezoneOffsetHours = offsetHours;
  timezoneInfo = tzFromOffset(timezoneOffsetHours);
  configureTimezone();

  bool timeSynced = syncTimeWithOffset(timezoneOffsetHours);

  if (!timeSynced) {
    Serial.println("NTP no respondió, intentando usar el RTC...");
    timeSynced = setTimeFromRtc();
  }

  if (!timeSynced) {
    Serial.println("Sin hora válida, continuando sin sincronización confirmada.");
    return false;
  }

  if (!syncRtcFromSystemClock()) {
    Serial.println("No se pudo actualizar el RTC con la hora obtenida.");
  }

  return true;
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
        Serial.println("Se continuará sin verificación; si el token es incorrecto el bot no responderá.");
        break;
      }

      Serial.println("Ingresa un token válido o presiona Enter para reutilizarlo.");
      String nuevoToken = readLineFromSerial("Nuevo token (vacío para mantener): ");

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
//  NTP + ZONA HORARIA GMT-5
// =========================================================
void configureTimezone() {
  setenv("TZ", timezoneInfo.c_str(), 1);
  tzset();
}

bool initRtc() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

  if (!rtc.begin()) {
    Serial.println("RTC no encontrado en el bus I2C.");
    return false;
  }

  Serial.println("RTC detectado correctamente.");
  return true;
}

bool syncRtcFromSystemClock() {
  if (!rtcReady) {
    return false;
  }

  time_t now;
  time(&now);

  if (now < 10) {
    return false;
  }

  rtc.adjust(DateTime(now));
  Serial.println("RTC actualizado con la hora del sistema (NTP).");
  return true;
}

bool setTimeFromRtc() {
  if (!rtcReady) {
    Serial.println("RTC no disponible para fijar la hora.");
    return false;
  }

  DateTime rtcNow = rtc.now();

  if (rtcNow.year() < 2020) {
    Serial.println("RTC tiene una fecha inválida, no se usará como respaldo.");
    return false;
  }

  timeval tv{rtcNow.unixtime(), 0};
  settimeofday(&tv, nullptr);
  configureTimezone();

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    Serial.println("Hora configurada desde el RTC.");
    Serial.print("Hora local: ");
    Serial.println(formatDateTime(timeinfo));
    return true;
  }

  Serial.println("No se pudo leer la hora local tras usar el RTC.");
  return false;
}

bool syncTimeWithOffset(int offsetHours, unsigned long maxWaitMs) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No hay conexión WiFi, no se puede sincronizar NTP.");
    return false;
  }

  timezoneInfo = tzFromOffset(offsetHours);
  configureTimezone();
  Serial.println("Sincronizando hora NTP (" + timezoneInfo + ")...");

  configTzTime(timezoneInfo.c_str(), "pool.ntp.org", "time.nist.gov", "time.cloudflare.com");

  struct tm timeinfo;
  unsigned long start = millis();

  while (millis() - start < maxWaitMs) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println();
      Serial.println("Conexión WiFi perdida durante la sincronización NTP.");
      return false;
    }

    if (getLocalTime(&timeinfo)) {
      Serial.println();
      Serial.println("Hora NTP sincronizada.");
      Serial.print("Hora local: ");
      Serial.println(formatDateTime(timeinfo));
      return true;
    }

    Serial.print('.');
    delay(500);
  }

  Serial.println();
  Serial.println("No se pudo sincronizar la hora NTP tras el tiempo de espera.");
  return false;
}


// =========================================================
//  VERIFICAR TOKEN DE TELEGRAM
// =========================================================
bool verifyTelegramToken(uint8_t maxAttempts, uint16_t retryDelayMs) {
  if (telegramBot == nullptr) {
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Sin conexión WiFi, no se puede verificar el token.");
    return false;
  }

  Serial.print("Verificando token Telegram... ");

  // En ocasiones el handshake TLS falla si la hora acaba de sincronizarse.
  for (uint8_t attempt = 0; attempt < maxAttempts; attempt++) {
    if (telegramBot->getMe()) {
      Serial.println("OK");
      Serial.print("Bot detectado: @");
      Serial.println(telegramBot->userName);
      return true;
    }

    delay(retryDelayMs);
    Serial.print('.');
  }

  Serial.println();
  Serial.println("FALLÓ (token inválido o sin conexión)");
  return false;
}


// =========================================================
//  SETUP
// =========================================================
void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  dht.begin();

  configInit();
  configLoad();
  loadRuntimeSettings();
  initIrrigationHardware();
  initFanPwm();
  pinMode(PIN_RELE2, OUTPUT);
  digitalWrite(PIN_RELE2, HIGH);
  pinMode(PIN_LED_MOSFET, OUTPUT);
  digitalWrite(PIN_LED_MOSFET, LOW);
  // Botón activo en LOW con pull-up interno. Se mantiene siempre como entrada
  // (INPUT_PULLUP) y únicamente se lee su estado en LOW para saltar las
  // credenciales; no se cambia a salida ni se escribe al pin.
  pinMode(PIN_CRED_SKIP, INPUT_PULLUP);
  delay(10);

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

  ensureSettingsStore();
  const bool hasStoredOffset = settingsStore.isKey(kStoredTimezoneOffsetKey);
  const int storedOffset = constrain(
      settingsStore.getInt(kStoredTimezoneOffsetKey, timezoneOffsetHours), -12,
      14);

  if (skipCredentialPrompt && hasStoredOffset) {
    Serial.println();
    Serial.println("Botón de salto: usando offset guardado en NVS.");
    timezoneOffsetHours = storedOffset;
  } else {
    timezoneOffsetHours = promptTimezoneOffset(timezoneOffsetHours);
  }
  timezoneInfo = tzFromOffset(timezoneOffsetHours);
  configureTimezone();
  rtcReady = initRtc();

  // -------------------------------------------------------
  //  CONEXIÓN WIFI
  // -------------------------------------------------------
  connectToWifi(wifiSsid, wifiPassword);

  // -------------------------------------------------------
  //  SINCRONIZAR HORA NTP (IMPORTANTE PARA TLS)
  // -------------------------------------------------------
  ensureTimeReady(timezoneOffsetHours);

  // Inicializar el estado de las luces según el horario configurado.
  applyLightSchedule();
  updateFanControl(true);

  // -------------------------------------------------------
  //  CONFIGURAR CLIENTE SEGURO PARA TELEGRAM
  // -------------------------------------------------------
  configureTelegramTransport();

  // -------------------------------------------------------
  //  VERIFICACIÓN ITERATIVA DEL TOKEN
  // -------------------------------------------------------
  initializeTelegramBot();

  // Mostrar configuración solo después de completar el flujo de credenciales.
  printIrrigationConfig();

  Serial.println("Sistema listo.");
}


// =========================================================
//  LOOP
// =========================================================
void loop() {
  handleSerialCommands();
  pollTelegram();
  sendPeriodicStatusIfNeeded();

  const unsigned long now = millis();

  if (now - lastLightCheckMs >= 10000) {
    lastLightCheckMs = now;
    applyLightSchedule();
  }

  if (now - lastFanUpdateMs >= 2000) {
    updateFanControl();
  }

  if (now - lastSoilCheckMs >= 2000) {
    lastSoilCheckMs = now;
    checkSoilAndIrrigate();
    evaluateAlerts();
  }
}
