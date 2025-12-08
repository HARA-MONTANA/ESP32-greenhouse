#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <Wire.h>
#include <RTClib.h>
#include <sys/time.h>
#include <time.h>

#include "pins.h"

// =========================================================
//  VARIABLES GLOBALES
// =========================================================
String wifiSsid;
String wifiPassword;
String telegramToken;

WiFiClientSecure telegramClient;
UniversalTelegramBot *telegramBot = nullptr;
RTC_DS3231 rtc;
bool rtcReady = false;

const char *TZ_INFO = "GMT-5";


// =========================================================
//  FUNCIONES DE UTILIDAD
// =========================================================
String readLineFromSerial(const char *prompt) {
  Serial.print(prompt);
  Serial.flush();

  while (!Serial.available()) {
    delay(20);
  }

  String line = Serial.readStringUntil('\n');
  line.trim();
  return line;
}


// =========================================================
//  SOLICITAR CREDENCIALES
// =========================================================
void requestCredentials() {
  do {
    Serial.print("WiFi SSID: ");
    wifiSsid = readLineFromSerial("");
    if (wifiSsid.isEmpty()) {
      Serial.println("El SSID no puede estar vacío.");
    } else {
      Serial.print("Ingresaste: ");
      Serial.println(wifiSsid);
      Serial.println();
    }
  } while (wifiSsid.isEmpty());

  do {
    Serial.print("WiFi Password: ");
    wifiPassword = readLineFromSerial("");
    if (wifiPassword.isEmpty()) {
      Serial.println("La contraseña no puede estar vacía.");
    } else {
      Serial.print("Ingresaste: ");
      Serial.println(wifiPassword);
      Serial.println();
    }
  } while (wifiPassword.isEmpty());

  do {
    Serial.print("Token Telegram: ");
    telegramToken = readLineFromSerial("");
    if (telegramToken.isEmpty()) {
      Serial.println("El token no puede estar vacío.");
    } else {
      Serial.print("Ingresaste: ");
      Serial.println(telegramToken);
      Serial.println();
    }
  } while (telegramToken.isEmpty());

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
  setenv("TZ", TZ_INFO, 1);
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
    Serial.println(asctime(&timeinfo));
    return true;
  }

  Serial.println("No se pudo leer la hora local tras usar el RTC.");
  return false;
}

bool syncTimeGMT5(unsigned long maxWaitMs = 60000) {
  Serial.println("Sincronizando hora NTP (GMT-5)...");

  // Ajuste horario fijo UTC-5 sin horario de verano
  configTzTime(TZ_INFO, "pool.ntp.org", "time.nist.gov");

  struct tm timeinfo;
  unsigned long start = millis();

  while (millis() - start < maxWaitMs) {
    if (getLocalTime(&timeinfo)) {
      Serial.println();
      Serial.println("Hora NTP sincronizada.");
      Serial.print("Hora local: ");
      Serial.println(asctime(&timeinfo));
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

  configureTimezone();
  rtcReady = initRtc();

  requestCredentials();

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

  // -------------------------------------------------------
  //  SINCRONIZAR HORA NTP (IMPORTANTE PARA TLS)
  // -------------------------------------------------------
  bool timeSynced = syncTimeGMT5();

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
    Serial.println("Token verificado correctamente.");
  }

  Serial.println("Sistema listo.");
}


// =========================================================
//  LOOP
// =========================================================
void loop() {
  // Aquí irá la lógica del invernadero, sensores, actuadores, etc.
  delay(1000);
}
