#include "config.h"

#include <Arduino.h>

namespace {
Preferences irrigationPrefs;
bool prefsStarted = false;

// Valores por defecto
int mlPl = 50;   // Plántula
int mlVeg = 100; // Vegetativo
int mlPre = 150; // Pre floración
int mlFlo = 200; // Floración
int mlFin = 120; // Final
float potVolumeL = 15.0f;
float pumpFlow = 0.0f;        // mL/s
bool pumpCalibrated = false;
int soilThreshold = 20;       // Porcentaje mínimo antes de regar
int soilHighThreshold = 65;   // Porcentaje máximo permitido
int irrigationIntervalDays = 2;
unsigned long lastIrrigationEpoch = 0;
plantStage currentStage = PLANTULA;

void ensurePrefs() {
  if (!prefsStarted) {
    prefsStarted = irrigationPrefs.begin("irrigation", false);
  }
}

int clampAndStoreMlDefaults(const char *key, int defaultValue) {
  const int stored = irrigationPrefs.isKey(key) ? irrigationPrefs.getInt(key, defaultValue) : defaultValue;
  const int clamped = constrain(stored, 5, 200);
  irrigationPrefs.putInt(key, clamped);
  return clamped;
}

}  // namespace

void configInit() {
  ensurePrefs();
}

void configLoad() {
  ensurePrefs();

  mlPl = clampAndStoreMlDefaults("ml_pl", mlPl);
  mlVeg = clampAndStoreMlDefaults("ml_veg", mlVeg);
  mlPre = clampAndStoreMlDefaults("ml_pre", mlPre);
  mlFlo = clampAndStoreMlDefaults("ml_flo", mlFlo);
  mlFin = clampAndStoreMlDefaults("ml_fin", mlFin);

  if (!irrigationPrefs.isKey("potL")) {
    irrigationPrefs.putFloat("potL", potVolumeL);
  }
  potVolumeL = constrain(irrigationPrefs.getFloat("potL", potVolumeL), 1.0f, 50.0f);
  irrigationPrefs.putFloat("potL", potVolumeL);

  if (!irrigationPrefs.isKey("flow")) {
    irrigationPrefs.putFloat("flow", pumpFlow);
  }
  pumpFlow = irrigationPrefs.getFloat("flow", pumpFlow);

  if (irrigationPrefs.isKey("flowCal")) {
    pumpCalibrated = irrigationPrefs.getBool("flowCal", pumpCalibrated);
  } else {
    pumpCalibrated = irrigationPrefs.isKey("flow") && pumpFlow > 0.0f;
  }

  if (pumpFlow > 0.0f && (pumpFlow < 1.0f || pumpFlow > 50.0f)) {
    pumpFlow = 0.0f;
    pumpCalibrated = false;
    irrigationPrefs.putFloat("flow", pumpFlow);
    irrigationPrefs.putBool("flowCal", pumpCalibrated);
  }

  if (!pumpCalibrated) {
    pumpFlow = 0.0f;
    irrigationPrefs.putFloat("flow", pumpFlow);
  }

  if (!irrigationPrefs.isKey("soilTh")) {
    irrigationPrefs.putInt("soilTh", soilThreshold);
  }
  soilThreshold = constrain(irrigationPrefs.getInt("soilTh", soilThreshold), 0, 50);
  irrigationPrefs.putInt("soilTh", soilThreshold);

  if (!irrigationPrefs.isKey("soilHigh")) {
    irrigationPrefs.putInt("soilHigh", soilHighThreshold);
  }
  soilHighThreshold = constrain(irrigationPrefs.getInt("soilHigh", soilHighThreshold), 50, 100);
  irrigationPrefs.putInt("soilHigh", soilHighThreshold);

  if (!irrigationPrefs.isKey("intDays")) {
    irrigationPrefs.putInt("intDays", irrigationIntervalDays);
  }
  irrigationIntervalDays = constrain(irrigationPrefs.getInt("intDays", irrigationIntervalDays), 1, 5);
  irrigationPrefs.putInt("intDays", irrigationIntervalDays);

  lastIrrigationEpoch = irrigationPrefs.getULong("lastIr", lastIrrigationEpoch);

  if (!irrigationPrefs.isKey("stage")) {
    irrigationPrefs.putInt("stage", static_cast<int>(currentStage));
  }
  currentStage = static_cast<plantStage>(irrigationPrefs.getInt("stage", static_cast<int>(currentStage)));
}

void configSave() {
  ensurePrefs();

  irrigationPrefs.putInt("ml_pl", mlPl);
  irrigationPrefs.putInt("ml_veg", mlVeg);
  irrigationPrefs.putInt("ml_pre", mlPre);
  irrigationPrefs.putInt("ml_flo", mlFlo);
  irrigationPrefs.putInt("ml_fin", mlFin);
  irrigationPrefs.putFloat("potL", potVolumeL);
  irrigationPrefs.putFloat("flow", pumpFlow);
  irrigationPrefs.putBool("flowCal", pumpCalibrated);
  irrigationPrefs.putInt("soilTh", soilThreshold);
  irrigationPrefs.putInt("soilHigh", soilHighThreshold);
  irrigationPrefs.putInt("intDays", irrigationIntervalDays);
  irrigationPrefs.putULong("lastIr", lastIrrigationEpoch);
  irrigationPrefs.putInt("stage", static_cast<int>(currentStage));
}

void configReset() {
  mlPl = 50;
  mlVeg = 100;
  mlPre = 150;
  mlFlo = 200;
  mlFin = 120;
  potVolumeL = 15.0f;
  pumpFlow = 0.0f;
  pumpCalibrated = false;
  soilThreshold = 20;
  soilHighThreshold = 65;
  irrigationIntervalDays = 2;
  lastIrrigationEpoch = 0;
  currentStage = PLANTULA;
  configSave();
}

int getMlPerLiterForStage(plantStage stage) {
  switch (stage) {
    case PLANTULA:
      return mlPl;
    case VEGETATIVO:
      return mlVeg;
    case PRE_FLORACION:
      return mlPre;
    case FLORACION:
      return mlFlo;
    case FINAL:
      return mlFin;
    default:
      return mlVeg;
  }
}

float getPotVolumeL() { return potVolumeL; }

float getPumpFlow() { return pumpFlow; }

bool isPumpCalibrated() { return pumpCalibrated; }

int getSoilThreshold() { return soilThreshold; }

int getSoilHighThreshold() { return soilHighThreshold; }

int getIrrigationIntervalDays() { return irrigationIntervalDays; }

unsigned long getLastIrrigationEpoch() { return lastIrrigationEpoch; }

plantStage getCurrentStage() { return currentStage; }

void updateStage(plantStage newStage) {
  currentStage = newStage;
  ensurePrefs();
  irrigationPrefs.putInt("stage", static_cast<int>(currentStage));
}

bool setMlPerLiterForStage(plantStage stage, int value) {
  if (value < 5 || value > 200) {
    return false;
  }
  ensurePrefs();
  switch (stage) {
    case PLANTULA:
      mlPl = value;
      irrigationPrefs.putInt("ml_pl", mlPl);
      break;
    case VEGETATIVO:
      mlVeg = value;
      irrigationPrefs.putInt("ml_veg", mlVeg);
      break;
    case PRE_FLORACION:
      mlPre = value;
      irrigationPrefs.putInt("ml_pre", mlPre);
      break;
    case FLORACION:
      mlFlo = value;
      irrigationPrefs.putInt("ml_flo", mlFlo);
      break;
    case FINAL:
      mlFin = value;
      irrigationPrefs.putInt("ml_fin", mlFin);
      break;
    default:
      return false;
  }

  return true;
}

bool setPotVolumeL(float liters) {
  if (liters < 1.0f || liters > 50.0f) {
    return false;
  }

  potVolumeL = liters;
  ensurePrefs();
  irrigationPrefs.putFloat("potL", potVolumeL);
  return true;
}

bool setPumpFlow(float mlPerSecond) {
  if (mlPerSecond < 1.0f || mlPerSecond > 50.0f) {
    return false;
  }

  pumpFlow = mlPerSecond;
  setPumpCalibrated(true);
  ensurePrefs();
  irrigationPrefs.putFloat("flow", pumpFlow);
  return true;
}

void setPumpCalibrated(bool calibrated) {
  pumpCalibrated = calibrated;
  ensurePrefs();
  irrigationPrefs.putBool("flowCal", pumpCalibrated);
}

bool setSoilThreshold(int threshold) {
  if (threshold < 0 || threshold > 50) {
    return false;
  }

  soilThreshold = threshold;
  ensurePrefs();
  irrigationPrefs.putInt("soilTh", soilThreshold);
  return true;
}

bool setSoilHighThreshold(int threshold) {
  if (threshold < 50 || threshold > 100) {
    return false;
  }

  soilHighThreshold = threshold;
  ensurePrefs();
  irrigationPrefs.putInt("soilHigh", soilHighThreshold);
  return true;
}

bool setIrrigationIntervalDays(int days) {
  if (days < 1 || days > 5) {
    return false;
  }

  irrigationIntervalDays = days;
  ensurePrefs();
  irrigationPrefs.putInt("intDays", irrigationIntervalDays);
  return true;
}

void setLastIrrigationEpoch(unsigned long epochSeconds) {
  lastIrrigationEpoch = epochSeconds;
  ensurePrefs();
  irrigationPrefs.putULong("lastIr", lastIrrigationEpoch);
}
