#include "irrigation.h"

#include <Arduino.h>

#include "pins.h"

// Declarada en ESP32.ino para reenviar a Serial, Telnet y Telegram
void broadcastMessage(const String &msg);

namespace {
const int R_Agua = PIN_RELE1;
bool autoIrrigationEnabled = true;
}

void initIrrigationHardware() {
  pinMode(R_Agua, OUTPUT);
  digitalWrite(R_Agua, HIGH);  // Relé inactivo en HIGH
}

int readSoilMoisture() { return analogRead(PIN_SUELO); }

bool checkSoilAndIrrigate() {
  const int soilReading = readSoilMoisture();

  if (!autoIrrigationEnabled || soilReading >= getSoilThreshold()) {
    return false;
  }

  irrigate(soilReading);
  return true;
}

void irrigate(int initialSoilReading) {
  plantStage stage = getCurrentStage();
  const int mlPerLiter = getMlPerLiterForStage(stage);
  const float potL = getPotVolumeL();
  const float pumpFlow = getPumpFlow();

  const float totalMl = mlPerLiter * potL;
  irrigateVolume(totalMl, initialSoilReading);
}

void irrigateVolume(float totalMl, int initialSoilReading) {
  plantStage stage = getCurrentStage();
  const float pumpFlow = getPumpFlow();
  float pumpTimeMs = 0.0f;

  if (pumpFlow > 0.0f) {
    pumpTimeMs = (totalMl / pumpFlow) * 1000.0f;
  }

  digitalWrite(R_Agua, LOW);
  delay(static_cast<unsigned long>(pumpTimeMs));
  digitalWrite(R_Agua, HIGH);

  String logMsg = "Riego etapa: " + String(static_cast<int>(stage));
  logMsg += ", ml_totales: " + String(totalMl, 2);
  logMsg += ", tiempo_ms: " + String(pumpTimeMs, 0);
  logMsg += ", humedad_inicial: " + String(initialSoilReading);

  broadcastMessage(logMsg);
}

void setAutoIrrigationEnabled(bool enabled) { autoIrrigationEnabled = enabled; }

bool isAutoIrrigationEnabled() { return autoIrrigationEnabled; }
