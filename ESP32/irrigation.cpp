#include "irrigation.h"

#include <Arduino.h>
#include <time.h>

#include "pins.h"

// Declaradas en ESP32.ino / sdcard.cpp
void broadcastMessage(const String &msg);
String stageToString(plantStage stage);
void logAccion(const char *tipo, const String &detalle);
bool readAmbient(float &tempC, float &rh);
void logAccionConSensores(const char *tipo, const String &detalle,
                           float tempC, float rh, int soilPct, int mqRaw);

namespace {
const int PUMP_ON_LEVEL = HIGH;
const int PUMP_OFF_LEVEL = LOW;
bool autoIrrigationEnabled = false;
bool tankEmptyNotified = false;
bool pumpNotCalibratedNotified = false;

// Proteccion capa 1: limite de riegos por dia (ventana de 24h en RAM)
const int MAX_DAILY_IRRIGATIONS = 4;
unsigned long windowStartMs = 0;
int dailyIrrigationCount = 0;
bool dailyLimitNotified = false;

// Proteccion capa 2: riegos consecutivos sin efecto
int noImprovementStreak = 0;
}  // namespace

void initIrrigationHardware() {
  pinMode(PIN_PUMP, OUTPUT);
  digitalWrite(PIN_PUMP, PUMP_OFF_LEVEL);
  pinMode(PIN_FLOAT, INPUT_PULLUP);
  autoIrrigationEnabled = getAutoIrrigationStored();
}

void pumpOn() { digitalWrite(PIN_PUMP, PUMP_ON_LEVEL); }
void pumpOff() { digitalWrite(PIN_PUMP, PUMP_OFF_LEVEL); }

int readSoilMoisture() { return analogRead(PIN_SUELO); }

bool isTankWaterAvailable() { return digitalRead(PIN_FLOAT) == LOW; }

int soilPercentFromAdc(int reading) {
  const int dryAdc = getSoilDryAdc();
  const int wetAdc = getSoilWetAdc();
  const int minR = min(wetAdc, dryAdc);
  const int maxR = max(wetAdc, dryAdc);
  int clamped = constrain(reading, minR, maxR);
  int percent = map(clamped, wetAdc, dryAdc, 100, 0);
  return constrain(percent, 0, 100);
}

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

  if (!autoIrrigationEnabled) return false;

  if (!isTankWaterAvailable()) {
    if (!tankEmptyNotified) {
      broadcastMessage("Riego omitido: tanque sin agua.");
      tankEmptyNotified = true;
    }
    return false;
  }
  tankEmptyNotified = false;

  const int highThreshold = getSoilHighThreshold();
  if (highThreshold > 0 && soilPercent >= highThreshold) return false;
  if (soilPercent >= getSoilThreshold()) return false;

  // Cooldown minimo de 15 minutos entre riegos para no ciclar la bomba
  time_t now;
  time(&now);
  const unsigned long lastEpoch = getLastIrrigationEpoch();
  if (now > 0 && lastEpoch > 0 && difftime(now, static_cast<time_t>(lastEpoch)) < 900.0) {
    return false;
  }

  // Proteccion capa 1: maximo MAX_DAILY_IRRIGATIONS riegos automaticos por dia
  unsigned long nowMs = millis();
  if (windowStartMs == 0 || nowMs - windowStartMs >= 86400000UL) {
    windowStartMs = nowMs;
    dailyIrrigationCount = 0;
    dailyLimitNotified = false;
  }
  if (dailyIrrigationCount >= MAX_DAILY_IRRIGATIONS) {
    if (!dailyLimitNotified) {
      broadcastMessage("ALERTA: Limite diario de " + String(MAX_DAILY_IRRIGATIONS) +
                       " riegos alcanzado. Verifica el sensor de suelo.");
      dailyLimitNotified = true;
    }
    return false;
  }

  dailyIrrigationCount++;
  irrigate(soilReading);
  return true;
}

void irrigate(int initialSoilReading) {
  const float totalMl = getMlPerLiterForStage(getCurrentStage()) * getPotVolumeL();
  irrigateVolume(totalMl, initialSoilReading);
}

void irrigateVolume(float totalMl, int initialSoilReading) {
  const float flow = getPumpFlow();

  if (!isPumpCalibrated() || flow <= 0.0f) {
    broadcastMessage("Riego omitido: bomba sin calibrar.");
    return;
  }

  const int initialAdc = initialSoilReading >= 0 ? initialSoilReading : readSoilMoisture();
  const int initialPercent = soilPercentFromAdc(initialAdc);
  const unsigned long pumpTimeMs = static_cast<unsigned long>((totalMl / flow) * 1000.0f);

  pumpOn();
  delay(pumpTimeMs);
  pumpOff();

  delay(5000);

  const int finalAdc = readSoilMoisture();
  const int finalPercent = soilPercentFromAdc(finalAdc);

  String msg = "Riego completado | Etapa: " + stageToString(getCurrentStage());
  msg += " | " + String(totalMl, 1) + " mL";
  msg += " | Bomba: " + String(pumpTimeMs / 1000.0f, 1) + " s";
  msg += " | Suelo: " + String(initialPercent) + "% -> " + String(finalPercent) + "%";
  broadcastMessage(msg);

  // Log a SD card con lecturas de ambiente del momento del riego
  String det = "etapa " + stageToString(getCurrentStage())
             + "; " + String(totalMl, 1) + " mL"
             + "; bomba " + String(pumpTimeMs / 1000.0f, 1) + "s"
             + "; suelo " + String(initialPercent) + "%->" + String(finalPercent) + "%";
  float irrTemp = NAN, irrRh = NAN;
  readAmbient(irrTemp, irrRh);
  // soilPct: usar finalPercent (estado tras el riego); mqRaw: no relevante para riego
  logAccionConSensores("RIEGO", det, irrTemp, irrRh, finalPercent, -1);

  // Proteccion capa 2: detectar sensor roto o bomba sin efecto
  if (finalPercent < initialPercent + 5) {
    noImprovementStreak++;
    broadcastMessage("Riego sin efecto (" + String(noImprovementStreak) +
                     " consecutivo/s). Suelo: " + String(initialPercent) +
                     "% -> " + String(finalPercent) + "%. Verifica sensor y bomba.");
    if (noImprovementStreak >= 2) {
      broadcastMessage("ALERTA: Auto riego desactivado por falla repetida. "
                       "Revisa sensor de suelo y bomba antes de reactivar.");
      setAutoIrrigationEnabled(false);
      noImprovementStreak = 0;
    }
  } else {
    noImprovementStreak = 0;
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
