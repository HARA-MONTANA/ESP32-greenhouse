#include "irrigation.h"

#include <Arduino.h>
#include <time.h>

#include "pins.h"

// Declarada en ESP32.ino para reenviar a Serial y Telegram
void broadcastMessage(const String &msg);
String stageToString(plantStage stage);

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

  if (!isPumpCalibrated()) {
    broadcastMessage("Riego omitido: bomba sin calibrar.");
    return false;
  }

  if (!autoIrrigationEnabled) {
    return false;
  }

  const int highThreshold = getSoilHighThreshold();
  if (highThreshold > 0 && soilPercent >= highThreshold) {
    broadcastMessage("Riego omitido: humedad alta detectada en suelo (" + String(soilPercent) + "% >= " + String(highThreshold) +
                     "%).");
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

  const float totalMl = mlPerLiter * potL;
  irrigateVolume(totalMl, initialSoilReading);
}

void irrigateVolume(float totalMl, int initialSoilReading) {
  plantStage stage = getCurrentStage();
  const float pumpFlow = getPumpFlow();

  if (!isPumpCalibrated()) {
    broadcastMessage("Riego omitido: calibra la bomba para calcular el caudal.");
    return;
  }

  const int initialAdc = initialSoilReading >= 0 ? initialSoilReading : readSoilMoisture();
  const int initialPercent = soilPercentFromAdc(initialAdc);
  float pumpTimeMs = 0.0f;

  if (pumpFlow > 0.0f) {
    pumpTimeMs = (totalMl / pumpFlow) * 1000.0f;
  }

  const float pumpTimeSeconds = pumpTimeMs / 1000.0f;

  digitalWrite(R_Agua, LOW);
  delay(static_cast<unsigned long>(pumpTimeMs));
  digitalWrite(R_Agua, HIGH);

  const int finalReading = readSoilMoisture();
  const int finalPercent = soilPercentFromAdc(finalReading);

  String logMsg = "Riego completado | Etapa: " + stageToString(stage);
  logMsg += " | Volumen: " + String(totalMl, 1) + " mL";
  logMsg += " | Duración: " + String(pumpTimeSeconds, 1) + " s";
  logMsg += " | Humedad: " + String(initialPercent) + "% → " + String(finalPercent) + "% (ADC " + String(initialAdc) +
            " → " + String(finalReading) + ")";

  broadcastMessage(logMsg);

  if (finalPercent <= initialPercent) {
    broadcastMessage("⚠️ Riego sin incremento de humedad; verifica bomba, mangueras y válvulas.");
  }

  time_t now;
  time(&now);
  if (now > 0) {
    setLastIrrigationEpoch(static_cast<unsigned long>(now));
  }
}

void setAutoIrrigationEnabled(bool enabled) { autoIrrigationEnabled = enabled; }

bool isAutoIrrigationEnabled() { return autoIrrigationEnabled; }
