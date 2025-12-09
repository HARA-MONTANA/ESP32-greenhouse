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

Preferences credentialsStore;
unsigned long lastSoilCheckMs = 0;
unsigned long lastTelegramPollMs = 0;
unsigned long lastAutoReadingMs = 0;

bool alertsEnabled = true;
bool autoReadingsEnabled = false;
unsigned long autoReadingsIntervalMs = 300000;  // 5 minutos

int soilDryAdc = 3500;
int soilWetAdc = 1200;
int tempAlertThreshold = 35;
int rhAlertThreshold = 85;
int mqAlertThreshold = 300;

bool awaitingCalibrationVolume = false;
bool fanAuto = true;
int fanPercent = 0;

std::vector<String> authorizedChatIds;

bool telegramEnabled = false;

WiFiClientSecure telegramClient;
UniversalTelegramBot *telegramBot = nullptr;
RTC_DS3231 rtc;
bool rtcReady = false;
String lastTelegramChatId;
DHT dht(PIN_DHT, DHT22);
float lastDhtTempC = NAN;
float lastDhtRh = NAN;
unsigned long lastDhtReadMs = 0;
bool lastDhtValid = false;

// Configuración de zona horaria (por defecto UTC-5, sin horario de verano).
// En la especificación POSIX el valor numérico representa las horas al oeste
// de Greenwich, por lo que se utiliza "GMT5" para obtener UTC-5.
String timezoneInfo = "GMT5";
int timezoneOffsetHours = -5;


// =========================================================
//  FUNCIONES DE UTILIDAD
// =========================================================
String readLineFromSerial(const char *prompt, uint32_t timeoutMs = 0) {
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

String promptOrStoredValue(const char *label, const String &storedValue, uint32_t timeoutMs) {
  while (true) {
    Serial.println();
    Serial.print(label);

    if (!storedValue.isEmpty()) {
      Serial.print(" (presiona Enter para usar el valor guardado)");
    }

    Serial.println();
    String value = readLineFromSerial("> ", timeoutMs);

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

  return formatDateTime(timeinfo);
}

String formatIrrigationConfig() {
  String msg;
  msg += "====Configuración del invernadero====\n";
  msg += "Etapa: " + stageToString(getCurrentStage()) + "\n";
  msg += "mL/L etapa actual: " + String(getMlPerLiterForStage(getCurrentStage())) + "\n";
  msg += "Último riego: " + formatLastIrrigation() + "\n";
  msg += "Maceta: " + String(getPotVolumeL(), 1) + " L\n";
  msg += "Bomba: " + String(getPumpFlow(), 1) + " mL/s\n";
  msg += "Umbral para riego: " + String(getSoilThreshold()) + "%\n";
  msg += "Limite de humedad del suelo: " + String(getSoilHighThreshold()) + "%\n";
  msg += "Dias hasta el siguiente riego: " + String(getIrrigationIntervalDays()) + " días";
  return msg;
}

void printIrrigationConfig() { Serial.println(formatIrrigationConfig()); }

int soilPercentFromAdc(int reading) {
  int clampedReading = constrain(reading, soilWetAdc, soilDryAdc);
  int percent = map(clampedReading, soilWetAdc, soilDryAdc, 100, 0);
  return constrain(percent, 0, 100);
}

String formatStatus() {
  struct tm timeinfo;
  String nowStr = "Sin hora";
  if (getLocalTime(&timeinfo)) {
    nowStr = formatDateTime(timeinfo);
  }

  float ambientTemp = NAN;
  float ambientRh = NAN;
  bool ambientOk = readAmbient(ambientTemp, ambientRh);

  const int soilAdc = readSoilMoisture();
  const int soilPercent = soilPercentFromAdc(soilAdc);
  String msg;
  msg += "Estado del invernadero\n";
  msg += "Etapa: " + stageToString(getCurrentStage()) + "\n";
  if (ambientOk) {
    msg += "Temp y humedad: " + String(ambientTemp, 1) + "°C | " + String(ambientRh, 0) + "%\n";
  } else {
    msg += "Temp y humedad: N/D\n";
  }
  msg += "Humedad suelo: " + String(soilPercent) + "% (ADC " + String(soilAdc) + ")\n";
  msg += "Último riego: " + formatLastIrrigation() + "\n";
  msg += "Hora local: " + nowStr + "\n";
  msg += "Riego automático: " + String(isAutoIrrigationEnabled() ? "ON" : "OFF") + "\n";
  msg += "Autolecturas: " + String(autoReadingsEnabled ? "ON" : "OFF") + (autoReadingsEnabled ? " cada " + String(autoReadingsIntervalMs / 60000) + " min" : "") + "\n";
  msg += "Ventilador: " + String(fanAuto ? "AUTO" : "MANUAL") + (fanAuto ? "" : " " + String(fanPercent) + "%") + "\n";
  msg += "Alertas: " + String(alertsEnabled ? "ON" : "OFF") + "\n";
  msg += "Ciclo de luz: N/D";
  return msg;
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
  Serial.println();
  Serial.println("Zona horaria: ingresa el offset UTC en horas (ej: -5, -7, +4).");
  Serial.print("Valor actual ");
  Serial.print(defaultOffset);
  Serial.println(". Presiona Enter para mantenerlo.");

  String input = readLineFromSerial("> ", 20000);
  input.trim();

  if (input.isEmpty()) {
    Serial.println("Usando offset existente.");
    return defaultOffset;
  }

  int offset = input.toInt();
  offset = constrain(offset, -12, 14);
  Serial.print("Offset seleccionado: ");
  Serial.println(offset);
  return offset;
}

String handleIrrigationCommand(const String &rawLine, bool &updated) {
  String line = rawLine;
  line.trim();
  if (line.isEmpty()) {
    return "";
  }

  String lower = line;
  lower.toLowerCase();
  updated = false;

  if (lower.startsWith("stage")) {
    int spaceIndex = lower.indexOf(' ');
    if (spaceIndex > 0) {
      String stageToken = lower.substring(spaceIndex + 1);
      stageToken.trim();
      plantStage newStage = stageFromString(stageToken);
      updateStage(newStage);
      updated = true;
      return "Etapa actualizada a " + stageToken;
    }
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
      setMlPerLiterForStage(stage, value);
      updated = true;
      return "mL/L actualizado para etapa " + stageToken + ": " + String(value);
    }
  } else if (lower.startsWith("pot")) {
    int spaceIndex = lower.indexOf(' ');
    if (spaceIndex > 0) {
      float liters = lower.substring(spaceIndex + 1).toFloat();
      setPotVolumeL(liters);
      updated = true;
      return "Volumen de maceta actualizado: " + String(liters);
    }
  } else if (lower.startsWith("flow")) {
    int spaceIndex = lower.indexOf(' ');
    if (spaceIndex > 0) {
      float flow = lower.substring(spaceIndex + 1).toFloat();
      setPumpFlow(flow);
      updated = true;
      return "Caudal de bomba actualizado: " + String(flow);
    }
  } else if (lower.startsWith("soilmax")) {
    int spaceIndex = lower.indexOf(' ');
    if (spaceIndex > 0) {
      int threshold = constrain(lower.substring(spaceIndex + 1).toInt(), 0, 100);
      setSoilHighThreshold(threshold);
      updated = true;
      return "Umbral de humedad alta actualizado: " + String(threshold) + "%";
    }
  } else if (lower.startsWith("soil")) {
    int spaceIndex = lower.indexOf(' ');
    if (spaceIndex > 0) {
      int threshold = constrain(lower.substring(spaceIndex + 1).toInt(), 0, 100);
      setSoilThreshold(threshold);
      updated = true;
      return "Umbral de suelo actualizado: " + String(threshold) + "%";
    }
  } else if (lower.startsWith("interval")) {
    int spaceIndex = lower.indexOf(' ');
    if (spaceIndex > 0) {
      long daysArg = lower.substring(spaceIndex + 1).toInt();
      int days = (int)max(1L, daysArg);
      setIrrigationIntervalDays(days);
      updated = true;
      return "Intervalo mínimo entre riegos actualizado a " + String(days) + " días";
    }
  } else if (lower == "reset") {
    configReset();
    updated = true;
    return "Configuración de riego restablecida a valores por defecto.";
  } else if (lower == "show") {
    return formatIrrigationConfig();
  }

  return "Comandos: stage <etapa>, ml <etapa> <valor>, pot <L>, flow <mL/s>, soil <pct>, soilmax <pct>, interval <dias>, status, reset, show";
}

void handleSerialCommands() {
  if (!Serial.available()) {
    return;
  }

  String line = Serial.readStringUntil('\n');
  bool updated = false;
  String response = handleIrrigationCommand(line, updated);

  if (!response.isEmpty()) {
    Serial.println(response);
  }

  if (updated) {
    configSave();
    printIrrigationConfig();
  }
}

void broadcastMessage(const String &msg) {
  if (msg.isEmpty()) {
    return;
  }

  Serial.println(msg);

  if (telegramEnabled && WiFi.status() == WL_CONNECTED && telegramBot != nullptr && !lastTelegramChatId.isEmpty()) {
    telegramBot->sendMessage(lastTelegramChatId, msg, "");
  }
}

bool parseOnOff(const String &value) {
  String lower = value;
  lower.toLowerCase();
  return lower == "on" || lower == "1" || lower == "true";
}

String commandHelp() {
  String help;
  help += "Comandos disponibles:\n";
  help += "/start, /status, /maceta [L], /etapa [plantula|vegetativo|pre-floracion|floracion|final]\n";
  help += "/cal_suelo [SECO] [HUMEDO], /alerta_suelo [%], /umbral_suelo [%], /intervalo_riego [dias]\n";
  help += "/alerta_temp_alta [C], /alerta_rh [%], /alerta_mq [N]\n";
  help += "/mostrar_conf_riego\n";
  help += "/calibrar [mL], /riego_auto [on|off], /regar [mL]\n";
  help += "/fanauto [on|off], /fan [0-100]\n";
  help += "/autolecturas [on|off] [min], /alertas [on|off]\n";
  help += "/addid [ID], /delid [ID], /ids";
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

  if (base == "/status") {
    return formatStatus();
  }

  if (base == "/maceta") {
    float liters = args.toFloat();
    if (liters <= 0) return "Uso: /maceta [litros]";
    setPotVolumeL(liters);
    updatedConfig = true;
    return "Capacidad de maceta actualizada a " + String(liters) + " L";
  }

  if (base == "/etapa") {
    if (args.isEmpty()) return "Uso: /etapa [plantula|vegetativo|pre-floracion|floracion|final]";
    updateStage(stageFromString(args));
    updatedConfig = true;
    return "Etapa cambiada a " + args;
  }

  if (base == "/cal_suelo") {
    int space2 = args.indexOf(' ');
    if (space2 == -1) return "Uso: /cal_suelo [SECO] [HUMEDO]";
    soilDryAdc = args.substring(0, space2).toInt();
    soilWetAdc = args.substring(space2 + 1).toInt();
    return "Calibración suelo actualizada. Seco=" + String(soilDryAdc) + " húmedo=" + String(soilWetAdc);
  }

  if (base == "/alerta_suelo") {
    int pct = args.toInt();
    if (pct <= 0) return "Uso: /alerta_suelo [%]";
    setSoilHighThreshold(constrain(pct, 0, 100));
    updatedConfig = true;
    return "Umbral de humedad alta fijado en " + String(pct) + "%";
  }

  if (base == "/umbral_suelo") {
    int pct = args.toInt();
    if (pct <= 0) return "Uso: /umbral_suelo [%]";
    setSoilThreshold(constrain(pct, 0, 100));
    updatedConfig = true;
    return "Umbral mínimo de humedad fijado en " + String(pct) + "%";
  }

  if (base == "/intervalo_riego") {
    int days = args.toInt();
    if (days <= 0) return "Uso: /intervalo_riego [dias]";
    setIrrigationIntervalDays(max(1, days));
    updatedConfig = true;
    return "Intervalo entre riegos fijado en " + String(days) + " días";
  }

  if (base == "/alerta_temp_alta") {
    int val = args.toInt();
    if (val <= 0) return "Uso: /alerta_temp_alta [C]";
    tempAlertThreshold = val;
    return "Umbral temp alta: " + String(val) + " C";
  }

  if (base == "/alerta_rh") {
    int val = args.toInt();
    if (val <= 0) return "Uso: /alerta_rh [%]";
    rhAlertThreshold = val;
    return "Umbral humedad ambiente: " + String(val) + "%";
  }

  if (base == "/alerta_mq") {
    int val = args.toInt();
    if (val <= 0) return "Uso: /alerta_mq [N]";
    mqAlertThreshold = val;
    return "Umbral MQ: " + String(val);
  }

  if (base == "/mostrar_conf_riego") {
    return formatIrrigationConfig();
  }

  if (base == "/calibrar") {
    if (args.isEmpty()) {
      awaitingCalibrationVolume = true;
      digitalWrite(PIN_RELE1, LOW);
      delay(5000);
      digitalWrite(PIN_RELE1, HIGH);
      return "Bomba activada 5s. Envía /calibrar [mL] con el volumen medido.";
    }

    float measuredMl = args.toFloat();
    if (measuredMl <= 0) return "Envía el volumen medido en mL.";
    float newFlow = measuredMl / 5.0f;
    setPumpFlow(newFlow);
    updatedConfig = true;
    awaitingCalibrationVolume = false;
    return "Caudal calculado: " + String(newFlow) + " mL/s";
  }

  if (base == "/riego_auto") {
    if (args.isEmpty()) return String("Riego automático está ") + (isAutoIrrigationEnabled() ? "ON" : "OFF");
    setAutoIrrigationEnabled(parseOnOff(args));
    return String("Riego automático ") + (isAutoIrrigationEnabled() ? "activado" : "desactivado");
  }

  if (base == "/fanauto") {
    fanAuto = parseOnOff(args);
    return String("Control automático de ventilador ") + (fanAuto ? "ON" : "OFF");
  }

  if (base == "/fan") {
    int pct = args.toInt();
    pct = constrain(pct, 0, 100);
    fanPercent = pct;
    fanAuto = false;
    return "Ventilador en manual a " + String(pct) + "%";
  }

  if (base == "/regar") {
    float ml = args.toFloat();
    if (ml <= 0) return "Uso: /regar [mL]";
    ml = min(ml, 1500.0f);
    irrigateVolume(ml, readSoilMoisture());
    return "Riego manual por " + String(ml) + " mL";
  }

  if (base == "/autolecturas") {
    if (args.isEmpty()) return String("Autolecturas ") + (autoReadingsEnabled ? "ON" : "OFF");
    int spaceArg = args.indexOf(' ');
    String flag = spaceArg == -1 ? args : args.substring(0, spaceArg);
    String minutesStr = spaceArg == -1 ? "" : args.substring(spaceArg + 1);
    autoReadingsEnabled = parseOnOff(flag);
    if (!minutesStr.isEmpty()) {
      int mins = minutesStr.toInt();
      if (mins > 0) autoReadingsIntervalMs = mins * 60000UL;
    }
    return String("Autolecturas ") + (autoReadingsEnabled ? "activadas" : "desactivadas");
  }

  if (base == "/alertas") {
    alertsEnabled = parseOnOff(args);
    return String("Alertas ") + (alertsEnabled ? "activadas" : "desactivadas");
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

  telegramBot->sendMessage(targetChat, formatStatus(), "");
}

void loadStoredCredentials() {
  credentialsStore.begin("cred", false);
  storedWifiSsid = credentialsStore.getString("ssid", "");
  storedWifiPassword = credentialsStore.getString("pass", "");
  storedTelegramToken = credentialsStore.getString("token", "");
  loadAuthorizedChatIds();
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

bool syncTimeWithOffset(int offsetHours = -5, unsigned long maxWaitMs = 60000) {
  timezoneInfo = tzFromOffset(offsetHours);
  configureTimezone();
  Serial.println("Sincronizando hora NTP (" + timezoneInfo + ")...");

  configTzTime(timezoneInfo.c_str(), "pool.ntp.org", "time.nist.gov");

  struct tm timeinfo;
  unsigned long start = millis();

  while (millis() - start < maxWaitMs) {
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
bool verifyTelegramToken(uint8_t maxAttempts = 5, uint16_t retryDelayMs = 1000) {
  if (telegramBot == nullptr) {
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
  initIrrigationHardware();
  printIrrigationConfig();

  loadStoredCredentials();
  requestCredentials();

  timezoneOffsetHours = promptTimezoneOffset(timezoneOffsetHours);
  timezoneInfo = tzFromOffset(timezoneOffsetHours);
  configureTimezone();
  rtcReady = initRtc();

  // -------------------------------------------------------
  //  CONEXIÓN WIFI
  // -------------------------------------------------------
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());

  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();
  Serial.println("WiFi conectado.");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  saveWifiCredentials(wifiSsid, wifiPassword);

  // -------------------------------------------------------
  //  SINCRONIZAR HORA NTP (IMPORTANTE PARA TLS)
  // -------------------------------------------------------
  bool timeSynced = syncTimeWithOffset(timezoneOffsetHours);

  if (!timeSynced) {
    Serial.println("NTP no respondió, intentando usar el RTC...");
    timeSynced = setTimeFromRtc();
  }

  if (!timeSynced) {
    Serial.println("Sin hora válida, continuando sin sincronización confirmada.");
  } else if (!syncRtcFromSystemClock()) {
    Serial.println("No se pudo actualizar el RTC con la hora obtenida.");
  }

  // -------------------------------------------------------
  //  CONFIGURAR CLIENTE SEGURO PARA TELEGRAM
  // -------------------------------------------------------
  telegramClient.setCACert(TELEGRAM_CERTIFICATE_ROOT);  
  telegramClient.setTimeout(15000);

  // -------------------------------------------------------
  //  VERIFICACIÓN ITERATIVA DEL TOKEN
  // -------------------------------------------------------
  bool tokenVerificado = false;
  uint8_t intentosToken = 0;
  const uint8_t maxIntentosToken = 2;  // cantidad de veces que se solicitará un token nuevo

  while (!tokenVerificado && intentosToken < maxIntentosToken) {
    delete telegramBot;
    telegramBot = new UniversalTelegramBot(telegramToken, telegramClient);

    tokenVerificado = verifyTelegramToken();

    if (!tokenVerificado) {
      intentosToken++;

      if (intentosToken >= maxIntentosToken) {
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
    telegramEnabled = true;
  }

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

  if (now - lastSoilCheckMs >= 2000) {
    lastSoilCheckMs = now;
    checkSoilAndIrrigate();
  }
}
