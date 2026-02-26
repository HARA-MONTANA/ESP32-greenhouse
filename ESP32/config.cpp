#include "config.h"

#include <Arduino.h>

namespace {
Preferences prefs;
bool prefsStarted = false;

// --- Valores de riego/planta ---
int mlPl = 50;
int mlVeg = 100;
int mlPre = 150;
int mlFlo = 200;
int mlFin = 120;
int lightHoursPl = 18;
int lightHoursVeg = 18;
float potVolumeL = 15.0f;
float pumpFlow = 0.0f;
bool pumpCalibrated = false;
int soilThreshold = 20;
int soilHighThreshold = 40;
int irrigationIntervalDays = 2;
unsigned long lastIrrigationEpoch = 0;
bool autoIrrigationStored = false;
plantStage currentStage = PLANTULA;

// --- Valores de runtime (antes en namespace "runtime") ---
int soilDryAdc = 2150;
int soilWetAdc = 500;
int tempAlertThreshold = 30;
int rhLowAlertThreshold = 40;
int rhHighAlertThreshold = 75;
int mqAlertThreshold = 500;
bool autoReadingsEnabled = false;
unsigned long autoReadingsIntervalMs = 300000;
int timezoneOffsetHours = -5;
int ledIntensity = 100;

void ensurePrefs() {
  if (!prefsStarted) {
    prefsStarted = prefs.begin("config", false);
  }
}

int clampMl(const char *key, int defaultValue) {
  int val = prefs.isKey(key) ? prefs.getInt(key, defaultValue) : defaultValue;
  return constrain(val, 5, 200);
}

int clampLightHours(const char *key, int defaultValue) {
  int val = prefs.isKey(key) ? prefs.getInt(key, defaultValue) : defaultValue;
  return constrain(val, 12, 20);
}

}  // namespace

void configInit() {
  ensurePrefs();

  // Riego
  mlPl = clampMl("ml_pl", mlPl);
  mlVeg = clampMl("ml_veg", mlVeg);
  mlPre = clampMl("ml_pre", mlPre);
  mlFlo = clampMl("ml_flo", mlFlo);
  mlFin = clampMl("ml_fin", mlFin);

  lightHoursPl = clampLightHours("lh_pl", lightHoursPl);
  lightHoursVeg = clampLightHours("lh_veg", lightHoursVeg);
  // Pre/flo/fin siempre 12h, no se guardan

  potVolumeL = constrain(prefs.getFloat("potL", potVolumeL), 1.0f, 50.0f);

  pumpFlow = prefs.getFloat("flow", pumpFlow);
  pumpCalibrated = prefs.getBool("flowCal", false);
  if (pumpFlow > 0.0f && (pumpFlow < 1.0f || pumpFlow > 50.0f)) {
    pumpFlow = 0.0f;
    pumpCalibrated = false;
  }
  if (!pumpCalibrated) {
    pumpFlow = 0.0f;
  }

  soilThreshold = constrain(prefs.getInt("soilTh", soilThreshold), 0, 50);
  soilHighThreshold = constrain(prefs.getInt("soilHigh", soilHighThreshold), 10, 100);
  irrigationIntervalDays = constrain(prefs.getInt("intDays", irrigationIntervalDays), 1, 5);
  lastIrrigationEpoch = prefs.getULong("lastIr", lastIrrigationEpoch);
  autoIrrigationStored = prefs.getBool("autoIr", autoIrrigationStored);
  currentStage = static_cast<plantStage>(prefs.getInt("stage", static_cast<int>(currentStage)));

  // Runtime
  soilDryAdc = constrain(prefs.getInt("soilDry", soilDryAdc), 0, 4095);
  soilWetAdc = constrain(prefs.getInt("soilWet", soilWetAdc), 0, 4095);
  if (soilDryAdc == soilWetAdc) {
    soilDryAdc = min(soilWetAdc + 1, 4095);
  }

  tempAlertThreshold = constrain(prefs.getInt("tempHi", tempAlertThreshold), 1, 100);
  rhLowAlertThreshold = constrain(prefs.getInt("rhLow", rhLowAlertThreshold), 1, 100);
  rhHighAlertThreshold = constrain(prefs.getInt("rhHigh", rhHighAlertThreshold), 1, 100);
  mqAlertThreshold = max(prefs.getInt("mqTh", mqAlertThreshold), 1);

  autoReadingsEnabled = prefs.getBool("autoRpt", autoReadingsEnabled);
  autoReadingsIntervalMs = max((unsigned long)prefs.getUInt("autoInt", (uint32_t)autoReadingsIntervalMs), 60000UL);

  timezoneOffsetHours = constrain(prefs.getInt("tzOff", timezoneOffsetHours), -12, 14);
  ledIntensity = constrain(prefs.getInt("led_int", ledIntensity), 1, 100);
}

void configSave() {
  ensurePrefs();

  // Riego
  prefs.putInt("ml_pl", mlPl);
  prefs.putInt("ml_veg", mlVeg);
  prefs.putInt("ml_pre", mlPre);
  prefs.putInt("ml_flo", mlFlo);
  prefs.putInt("ml_fin", mlFin);
  prefs.putInt("lh_pl", lightHoursPl);
  prefs.putInt("lh_veg", lightHoursVeg);
  prefs.putFloat("potL", potVolumeL);
  prefs.putFloat("flow", pumpFlow);
  prefs.putBool("flowCal", pumpCalibrated);
  prefs.putInt("soilTh", soilThreshold);
  prefs.putInt("soilHigh", soilHighThreshold);
  prefs.putInt("intDays", irrigationIntervalDays);
  prefs.putULong("lastIr", lastIrrigationEpoch);
  prefs.putBool("autoIr", autoIrrigationStored);
  prefs.putInt("stage", static_cast<int>(currentStage));

  // Runtime
  prefs.putInt("soilDry", soilDryAdc);
  prefs.putInt("soilWet", soilWetAdc);
  prefs.putInt("tempHi", tempAlertThreshold);
  prefs.putInt("rhLow", rhLowAlertThreshold);
  prefs.putInt("rhHigh", rhHighAlertThreshold);
  prefs.putInt("mqTh", mqAlertThreshold);
  prefs.putBool("autoRpt", autoReadingsEnabled);
  prefs.putUInt("autoInt", autoReadingsIntervalMs);
  prefs.putInt("tzOff", timezoneOffsetHours);
  prefs.putInt("led_int", ledIntensity);
}

void configReset() {
  mlPl = 50;
  mlVeg = 100;
  mlPre = 150;
  mlFlo = 200;
  mlFin = 120;
  lightHoursPl = 18;
  lightHoursVeg = 18;
  potVolumeL = 15.0f;
  pumpFlow = 0.0f;
  pumpCalibrated = false;
  soilThreshold = 20;
  soilHighThreshold = 40;
  irrigationIntervalDays = 2;
  lastIrrigationEpoch = 0;
  autoIrrigationStored = false;
  currentStage = PLANTULA;
  soilDryAdc = 2150;
  soilWetAdc = 500;
  tempAlertThreshold = 30;
  rhLowAlertThreshold = 40;
  rhHighAlertThreshold = 75;
  mqAlertThreshold = 500;
  autoReadingsEnabled = false;
  autoReadingsIntervalMs = 300000;
  timezoneOffsetHours = -5;
  ledIntensity = 100;
  configSave();
}

// --- Getters riego/planta ---

plantStage getCurrentStage() { return currentStage; }

int getMlPerLiterForStage(plantStage stage) {
  switch (stage) {
    case PLANTULA:      return mlPl;
    case VEGETATIVO:    return mlVeg;
    case PRE_FLORACION: return mlPre;
    case FLORACION:     return mlFlo;
    case FINAL:         return mlFin;
    default:            return mlVeg;
  }
}

int getLightHoursForStage(plantStage stage) {
  switch (stage) {
    case PLANTULA:   return lightHoursPl;
    case VEGETATIVO: return lightHoursVeg;
    default:         return 12;  // Pre/flo/fin siempre 12h
  }
}

float getPotVolumeL() { return potVolumeL; }
float getPumpFlow() { return pumpFlow; }
bool isPumpCalibrated() { return pumpCalibrated; }
int getSoilThreshold() { return soilThreshold; }
int getSoilHighThreshold() { return soilHighThreshold; }
int getIrrigationIntervalDays() { return irrigationIntervalDays; }
unsigned long getLastIrrigationEpoch() { return lastIrrigationEpoch; }
bool getAutoIrrigationStored() { return autoIrrigationStored; }

// --- Getters runtime ---

int getTempAlertThreshold() { return tempAlertThreshold; }
int getRhLowAlertThreshold() { return rhLowAlertThreshold; }
int getRhHighAlertThreshold() { return rhHighAlertThreshold; }
int getMqAlertThreshold() { return mqAlertThreshold; }
int getSoilDryAdc() { return soilDryAdc; }
int getSoilWetAdc() { return soilWetAdc; }
bool getAutoReadingsEnabled() { return autoReadingsEnabled; }
unsigned long getAutoReadingsIntervalMs() { return autoReadingsIntervalMs; }
int getTimezoneOffsetHours() { return timezoneOffsetHours; }

// --- Setters riego/planta ---

void updateStage(plantStage newStage) {
  currentStage = newStage;
  ensurePrefs();
  prefs.putInt("stage", static_cast<int>(currentStage));
}

bool setMlPerLiterForStage(plantStage stage, int value) {
  if (value < 5 || value > 200) return false;
  ensurePrefs();
  switch (stage) {
    case PLANTULA:      mlPl = value;  prefs.putInt("ml_pl", mlPl); break;
    case VEGETATIVO:    mlVeg = value; prefs.putInt("ml_veg", mlVeg); break;
    case PRE_FLORACION: mlPre = value; prefs.putInt("ml_pre", mlPre); break;
    case FLORACION:     mlFlo = value; prefs.putInt("ml_flo", mlFlo); break;
    case FINAL:         mlFin = value; prefs.putInt("ml_fin", mlFin); break;
    default: return false;
  }
  return true;
}

bool setPotVolumeL(float liters) {
  if (liters < 1.0f || liters > 50.0f) return false;
  potVolumeL = liters;
  ensurePrefs();
  prefs.putFloat("potL", potVolumeL);
  return true;
}

bool setPumpFlow(float mlPerSecond) {
  if (mlPerSecond < 1.0f || mlPerSecond > 50.0f) return false;
  pumpFlow = mlPerSecond;
  setPumpCalibrated(true);
  ensurePrefs();
  prefs.putFloat("flow", pumpFlow);
  return true;
}

void setPumpCalibrated(bool calibrated) {
  pumpCalibrated = calibrated;
  ensurePrefs();
  prefs.putBool("flowCal", pumpCalibrated);
}

bool setSoilThreshold(int threshold) {
  if (threshold < 0 || threshold > 50) return false;
  soilThreshold = threshold;
  ensurePrefs();
  prefs.putInt("soilTh", soilThreshold);
  return true;
}

bool setSoilHighThreshold(int threshold) {
  if (threshold < 10 || threshold > 100) return false;
  soilHighThreshold = threshold;
  ensurePrefs();
  prefs.putInt("soilHigh", soilHighThreshold);
  return true;
}

bool setIrrigationIntervalDays(int days) {
  if (days < 1 || days > 5) return false;
  irrigationIntervalDays = days;
  ensurePrefs();
  prefs.putInt("intDays", irrigationIntervalDays);
  return true;
}

void setLastIrrigationEpoch(unsigned long epochSeconds) {
  lastIrrigationEpoch = epochSeconds;
  ensurePrefs();
  prefs.putULong("lastIr", lastIrrigationEpoch);
}

void setAutoIrrigationStored(bool enabled) {
  autoIrrigationStored = enabled;
  ensurePrefs();
  prefs.putBool("autoIr", autoIrrigationStored);
}

bool setLightHoursForStage(plantStage stage, int hours) {
  if (hours < 12 || hours > 20) return false;
  ensurePrefs();
  switch (stage) {
    case PLANTULA:   lightHoursPl = hours;  prefs.putInt("lh_pl", lightHoursPl); break;
    case VEGETATIVO: lightHoursVeg = hours; prefs.putInt("lh_veg", lightHoursVeg); break;
    default: return false;  // Pre/flo/fin no se pueden editar
  }
  return true;
}

// --- Setters runtime ---

void setTempAlertThreshold(int val) {
  tempAlertThreshold = constrain(val, 1, 100);
  ensurePrefs();
  prefs.putInt("tempHi", tempAlertThreshold);
}

void setRhLowAlertThreshold(int val) {
  rhLowAlertThreshold = constrain(val, 1, 100);
  ensurePrefs();
  prefs.putInt("rhLow", rhLowAlertThreshold);
}

void setRhHighAlertThreshold(int val) {
  rhHighAlertThreshold = constrain(val, 1, 100);
  ensurePrefs();
  prefs.putInt("rhHigh", rhHighAlertThreshold);
}

void setMqAlertThreshold(int val) {
  mqAlertThreshold = max(val, 1);
  ensurePrefs();
  prefs.putInt("mqTh", mqAlertThreshold);
}

void setSoilCalibration(int dryAdc, int wetAdc) {
  soilDryAdc = constrain(dryAdc, 0, 4095);
  soilWetAdc = constrain(wetAdc, 0, 4095);
  if (soilDryAdc <= soilWetAdc) {
    soilDryAdc = min(soilWetAdc + 1, 4095);
  }
  ensurePrefs();
  prefs.putInt("soilDry", soilDryAdc);
  prefs.putInt("soilWet", soilWetAdc);
}

void setAutoReadings(bool enabled, unsigned long intervalMs) {
  autoReadingsEnabled = enabled;
  autoReadingsIntervalMs = max(intervalMs, 60000UL);
  ensurePrefs();
  prefs.putBool("autoRpt", autoReadingsEnabled);
  prefs.putUInt("autoInt", autoReadingsIntervalMs);
}

void setTimezoneOffsetHours(int offset) {
  timezoneOffsetHours = constrain(offset, -12, 14);
  ensurePrefs();
  prefs.putInt("tzOff", timezoneOffsetHours);
}

// --- LED morado ---

int getLedIntensity() { return ledIntensity; }

bool setLedIntensity(int pct) {
  if (pct < 1 || pct > 100) return false;
  ledIntensity = pct;
  ensurePrefs();
  prefs.putInt("led_int", ledIntensity);
  return true;
}
