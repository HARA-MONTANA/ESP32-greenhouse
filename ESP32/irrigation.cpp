#include "irrigation.h"

#include <Arduino.h>
#include <time.h>

#include "pins.h"

// Declarada en ESP32.ino para reenviar a Serial y Telegram
void broadcastMessage(const String &msg);
String stageToString(plantStage stage);

namespace {
const int R_Agua = PIN_RELE1;
const int PUMP_ON_LEVEL = HIGH;
const int PUMP_OFF_LEVEL = LOW;
bool autoIrrigationEnabled = false;
bool tankEmptyNotified = false;
bool pumpNotCalibratedNotified = false;

struct PulseSummary {
  int finalAdc;
  int finalPercent;
  float deliveredMl;
  unsigned long totalOnTimeMs;
  unsigned long totalDurationMs;
  unsigned int pulseCount;
  unsigned long absorptionMs;
};

PulseSummary runPulsedIrrigation(float totalMl, float pumpFlow, int targetPercent, int highThreshold, int initialAdc) {
  const float estimatedPumpTimeMs = (totalMl / pumpFlow) * 1000.0f;
  const unsigned long basePulseOnMs = constrain(static_cast<unsigned long>(estimatedPumpTimeMs / 6.0f), 500UL, 3000UL);
  const unsigned long absorptionMs = max(basePulseOnMs * 3UL, 5000UL);

  PulseSummary summary{};
  summary.finalAdc = initialAdc;
  summary.finalPercent = soilPercentFromAdc(initialAdc);
  summary.absorptionMs = absorptionMs;

  const unsigned long startMs = millis();

  while (summary.deliveredMl < totalMl && summary.finalPercent < targetPercent && summary.finalPercent < highThreshold) {
    const float remainingMl = totalMl - summary.deliveredMl;
    const unsigned long remainingOnMs = static_cast<unsigned long>((remainingMl / pumpFlow) * 1000.0f);
    unsigned long pulseOnMs = min(basePulseOnMs, remainingOnMs);
    pulseOnMs = max(pulseOnMs, 300UL);

    digitalWrite(R_Agua, PUMP_ON_LEVEL);
    delay(pulseOnMs);
    digitalWrite(R_Agua, PUMP_OFF_LEVEL);

    summary.pulseCount++;
    summary.totalOnTimeMs += pulseOnMs;
    summary.deliveredMl += pumpFlow * (pulseOnMs / 1000.0f);

    delay(absorptionMs);

    summary.finalAdc = readSoilMoisture();
    summary.finalPercent = soilPercentFromAdc(summary.finalAdc);
  }

  summary.totalDurationMs = millis() - startMs;
  return summary;
}
}

void initIrrigationHardware() {
  pinMode(R_Agua, OUTPUT);
  digitalWrite(R_Agua, PUMP_OFF_LEVEL);
  pinMode(PIN_FLOAT, INPUT_PULLUP);
  autoIrrigationEnabled = getAutoIrrigationStored();
}

void pumpOn() { digitalWrite(R_Agua, PUMP_ON_LEVEL); }

void pumpOff() { digitalWrite(R_Agua, PUMP_OFF_LEVEL); }

int readSoilMoisture() { return analogRead(PIN_SUELO); }

bool isTankWaterAvailable() { return digitalRead(PIN_FLOAT) == LOW; }

bool checkSoilAndIrrigate() {
  const int soilReading = readSoilMoisture();
  const int soilPercent = soilPercentFromAdc(soilReading);

  if (!isPumpCalibrated()) {
    if (!pumpNotCalibratedNotified) {
      broadcastMessage("Riego omitido: bomba sin calibrar.");
      pumpNotCalibratedNotified = true;
    }
    return false;
  }
  pumpNotCalibratedNotified = false;

  if (!autoIrrigationEnabled) {
    return false;
  }

  if (!isTankWaterAvailable()) {
    if (!tankEmptyNotified) {
      broadcastMessage("Riego omitido: tanque sin agua.");
      tankEmptyNotified = true;
    }
    return false;
  }
  tankEmptyNotified = false;

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

  if (pumpFlow <= 0.0f) {
    broadcastMessage("Riego omitido: caudal de bomba inválido.");
    return;
  }

  const int initialAdc = initialSoilReading >= 0 ? initialSoilReading : readSoilMoisture();
  const int initialPercent = soilPercentFromAdc(initialAdc);
  const int targetPercent = getSoilThreshold();
  const int highThreshold = getSoilHighThreshold();

  const PulseSummary summary = runPulsedIrrigation(totalMl, pumpFlow, targetPercent, highThreshold, initialAdc);

  const int finalReading = summary.finalAdc;
  const int finalPercent = summary.finalPercent;

  String logMsg = "Riego completado | Etapa: " + stageToString(stage);
  logMsg += " | Volumen solicitado: " + String(totalMl, 1) + " mL";
  logMsg += " | Volumen entregado: " + String(summary.deliveredMl, 1) + " mL";
  logMsg += " | Pulsos: " + String(summary.pulseCount);
  logMsg += " | ON acumulado: " + String(summary.totalOnTimeMs / 1000.0f, 1) + " s";
  logMsg += " | Tiempo total: " + String(summary.totalDurationMs / 1000.0f, 1) + " s";
  logMsg += " | Absorción entre pulsos: " + String(summary.absorptionMs / 1000.0f, 1) + " s";
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

void setAutoIrrigationEnabled(bool enabled) {
  autoIrrigationEnabled = enabled;
  setAutoIrrigationStored(enabled);
}

bool isAutoIrrigationEnabled() { return autoIrrigationEnabled; }
