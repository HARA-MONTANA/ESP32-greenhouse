#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <TelegramCertificate.h>
#include <time.h>

// =========================================================
//  VARIABLES GLOBALES
// =========================================================
String wifiSsid;
String wifiPassword;
String telegramToken;

WiFiClientSecure telegramClient;
UniversalTelegramBot *telegramBot = nullptr;


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
  Serial.println();
  Serial.println("=========== CONFIGURACIÓN INICIAL ===========");

  do {
    wifiSsid = readLineFromSerial("WiFi SSID: ");
  } while (wifiSsid.isEmpty());

  do {
    wifiPassword = readLineFromSerial("WiFi Password: ");
  } while (wifiPassword.isEmpty());

  do {
    telegramToken = readLineFromSerial("Token Telegram: ");
  } while (telegramToken.isEmpty());

  Serial.println("==============================================");
  Serial.println("Credenciales recibidas.");
}


// =========================================================
//  NTP + ZONA HORARIA GMT-5
// =========================================================
void syncTimeGMT5() {
  Serial.println("Sincronizando hora NTP (GMT-5)...");

  const long gmtOffset_sec = -5 * 3600;  // UTC-5
  const int daylightOffset_sec = 0;

  configTime(gmtOffset_sec, daylightOffset_sec, 
             "pool.ntp.org", 
             "time.nist.gov");

  time_t now = time(nullptr);
  while (now < 24 * 3600) {
    delay(500);
    Serial.print('.');
    now = time(nullptr);
  }

  Serial.println();
  Serial.println("Hora NTP sincronizada.");

  struct tm timeinfo;
  getLocalTime(&timeinfo);

  Serial.print("Hora local: ");
  Serial.println(asctime(&timeinfo));
}


// =========================================================
//  VERIFICAR TOKEN DE TELEGRAM
// =========================================================
bool verifyTelegramToken() {
  if (telegramBot == nullptr) {
    return false;
  }

  Serial.print("Verificando token Telegram... ");

  if (telegramBot->getMe()) {
    Serial.println("OK");
    Serial.print("Bot detectado: @");
    Serial.println(telegramBot->userName);
    return true;
  }

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
  syncTimeGMT5();

  // -------------------------------------------------------
  //  CONFIGURAR CLIENTE SEGURO PARA TELEGRAM
  // -------------------------------------------------------
  telegramClient.setCACert(TELEGRAM_CERTIFICATE_ROOT);  
  telegramClient.setTimeout(15000);

  // -------------------------------------------------------
  //  VERIFICACIÓN ITERATIVA DEL TOKEN
  // -------------------------------------------------------
  bool tokenValido = false;

  do {
    delete telegramBot;
    telegramBot = new UniversalTelegramBot(telegramToken, telegramClient);

    tokenValido = verifyTelegramToken();

    if (!tokenValido) {
      Serial.println("Ingresa un token válido.");
      telegramToken = readLineFromSerial("Nuevo token: ");
    }

  } while (!tokenValido);

  Serial.println("Token verificado correctamente.");
  Serial.println("Sistema listo.");
}


// =========================================================
//  LOOP
// =========================================================
void loop() {
  // Aquí irá la lógica del invernadero, sensores, actuadores, etc.
  delay(1000);
}
