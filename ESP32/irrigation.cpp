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

enum IrrigationState { IRR_IDLE, IRR_PUMPING, IRR_SETTLING };
IrrigationState irrState = IRR_IDLE;
unsigned long stateStartMs = 0;
unsigned long irrPumpTimeMs = 0;
float irrTotalMl = 0.0f;
int irrInitialAdc = 0;
int irrInitialPercent = 0;
plantStage irrStage = PLANTULA;
const unsigned long SETTLE_TIME_MS = 5000;
}

void initIrrigationHardware() {
  pinMode(R_Agua, OUTPUT);
  digitalWrite(R_Agua, PUMP_OFF_LEVEL);
  pinMode(PIN_FLOAT, INPUT_PULLUP);
}

void pumpOn() { digitalWrite(R_Agua, PUMP_ON_LEVEL); }

void pumpOff() { digitalWrite(R_Agua, PUMP_OFF_LEVEL); }

int readSoilMoisture() { return analogRead(PIN_SUELO); }

bool isTankWaterAvailable() { return digitalRead(PIN_FLOAT) == LOW; }

bool checkSoilAndIrrigate() {
  if (isIrrigating()) {
    return false;
  }

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
  if (irrState != IRR_IDLE) {
    broadcastMessage("Riego en curso, solicitud ignorada.");
    return;
  }

  const float pumpFlow = getPumpFlow();

  if (!isPumpCalibrated()) {
    broadcastMessage("Riego omitido: calibra la bomba para calcular el caudal.");
    return;
  }

  if (pumpFlow <= 0.0f) {
    broadcastMessage("Riego omitido: caudal de bomba inválido.");
    return;
  }

  irrStage = getCurrentStage();
  irrTotalMl = totalMl;
  irrInitialAdc = initialSoilReading >= 0 ? initialSoilReading : readSoilMoisture();
  irrInitialPercent = soilPercentFromAdc(irrInitialAdc);
  irrPumpTimeMs = static_cast<unsigned long>((totalMl / pumpFlow) * 1000.0f);

  pumpOn();
  stateStartMs = millis();
  irrState = IRR_PUMPING;
}

void updateIrrigation() {
  if (irrState == IRR_IDLE) {
    return;
  }

  const unsigned long now = millis();

  switch (irrState) {
    case IRR_PUMPING:
      if (now - stateStartMs >= irrPumpTimeMs) {
        pumpOff();
        stateStartMs = now;
        irrState = IRR_SETTLING;
      }
      break;

    case IRR_SETTLING:
      if (now - stateStartMs >= SETTLE_TIME_MS) {
        const int finalAdc = readSoilMoisture();
        const int finalPercent = soilPercentFromAdc(finalAdc);

        String logMsg = "Riego completado | Etapa: " + stageToString(irrStage);
        logMsg += " | Volumen: " + String(irrTotalMl, 1) + " mL";
        logMsg += " | Bomba ON: " + String(irrPumpTimeMs / 1000.0f, 1) + " s";
        logMsg += " | Humedad: " + String(irrInitialPercent) + "% -> " + String(finalPercent) + "%";
        logMsg += " (ADC " + String(irrInitialAdc) + " -> " + String(finalAdc) + ")";

        broadcastMessage(logMsg);

        if (finalPercent <= irrInitialPercent) {
          broadcastMessage("⚠️ Riego sin incremento de humedad; verifica bomba, mangueras y válvulas.");
        }

        time_t nowEpoch;
        time(&nowEpoch);
        if (nowEpoch > 0) {
          setLastIrrigationEpoch(static_cast<unsigned long>(nowEpoch));
        }

        irrState = IRR_IDLE;
      }
      break;

    default:
      break;
  }
}

bool isIrrigating() { return irrState != IRR_IDLE; }

void setAutoIrrigationEnabled(bool enabled) { autoIrrigationEnabled = enabled; }

bool isAutoIrrigationEnabled() { return autoIrrigationEnabled; }
