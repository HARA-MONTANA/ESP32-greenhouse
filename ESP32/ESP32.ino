#include "pins.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

String wifiSsid;
String wifiPassword;
String telegramToken;

WiFiClientSecure telegramClient;
UniversalTelegramBot *telegramBot = nullptr;

String readLineFromSerial(const char *prompt) {
  Serial.print(prompt);
  Serial.flush();

  while (!Serial.available()) {
    delay(50);
  }

  String line = Serial.readStringUntil('\n');
  line.trim();
  return line;
}

void requestCredentials() {
  Serial.println("================ CONFIGURACIÓN INICIAL ================");
  Serial.println("Ingresa las credenciales cuando se soliciten y presiona Enter.");
  Serial.println();

  do {
    wifiSsid = readLineFromSerial("WiFi SSID: ");
  } while (wifiSsid.isEmpty());

  do {
    wifiPassword = readLineFromSerial("WiFi password: ");
  } while (wifiPassword.isEmpty());

  do {
    telegramToken = readLineFromSerial("Token de Telegram: ");
  } while (telegramToken.isEmpty());

  Serial.println();
  Serial.println("Credenciales recibidas. Intentando conectar a WiFi...");
}

bool verifyTelegramToken() {
  if (telegramBot == nullptr) {
    return false;
  }

  Serial.print("Verificando token de Telegram...");
  const bool ok = telegramBot->getMe();

  if (ok) {
    Serial.print(" OK. Bot detectado: @");
    Serial.println(telegramBot->userName);
  } else {
    Serial.println(" error. No hay respuesta del bot.");
  }

  return ok;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  requestCredentials();

  // Ejemplo de conexión WiFi (puede ajustarse según sea necesario).
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();
  Serial.print("WiFi conectado. IP: ");
  Serial.println(WiFi.localIP());

  // Configurar cliente seguro y verificar token de Telegram.
  telegramClient.setInsecure();

  bool tokenValido = false;
  do {
    delete telegramBot;
    telegramBot = new UniversalTelegramBot(telegramToken, telegramClient);

    tokenValido = verifyTelegramToken();
    if (!tokenValido) {
      Serial.println("Token inválido o sin respuesta. Ingresa uno nuevo.");
      telegramToken = readLineFromSerial("Token de Telegram: ");
    }
  } while (!tokenValido);

  // TODO: add initialization for sensors, relays, and peripherals.
}

void loop() {
  // TODO: implement main control logic.
}
