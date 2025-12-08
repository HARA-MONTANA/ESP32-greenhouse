#include "irrigation.h"

#include <Arduino.h>
#include <time.h>

#include "pins.h"

// Declarada en ESP32.ino para reenviar a Serial y Telegram
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
  const int soilPercent = soilPercentFromAdc(soilReading);

  if (!autoIrrigationEnabled) {
    return false;
  }

  const int highThreshold = getSoilHighThreshold();
  if (highThreshold > 0 && soilPercent >= highThreshold) {
    broadcastMessage("Humedad alta detectada, se evita riego automático.");
    return false;
  }

  if (soilPercent >= getSoilThreshold()) {
    return false;
  }

  const int intervalDays = max(getIrrigationIntervalDays(), 0);
  const unsigned long intervalSeconds = static_cast<unsigned long>(intervalDays) * 86400UL;
  time_t now;
  time(&now);
  const unsigned long lastEpoch = getLastIrrigationEpoch();

  if (intervalSeconds > 0 && now > 0 && lastEpoch > 0) {
    if (difftime(now, static_cast<time_t>(lastEpoch)) < static_cast<double>(intervalSeconds)) {
      return false;
    }
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
  const int initialPercent = initialSoilReading >= 0 ? soilPercentFromAdc(initialSoilReading) : soilPercentFromAdc(readSoilMoisture());
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

  const int finalReading = readSoilMoisture();
  const int finalPercent = soilPercentFromAdc(finalReading);
  if (finalPercent <= initialPercent) {
    broadcastMessage("ERROR: la humedad del suelo no aumentó tras el riego. Verificar bomba y tuberías.");
  }

  time_t now;
  time(&now);
  if (now > 0) {
    setLastIrrigationEpoch(static_cast<unsigned long>(now));
  }
}

void setAutoIrrigationEnabled(bool enabled) { autoIrrigationEnabled = enabled; }

bool isAutoIrrigationEnabled() { return autoIrrigationEnabled; }
