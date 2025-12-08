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
float potVolumeL = 10.0f;
float pumpFlow = 50.0f;       // mL/s
int soilThreshold = 2000;     // Valor ADC
plantStage currentStage = PLANTULA;

void ensurePrefs() {
  if (!prefsStarted) {
    prefsStarted = irrigationPrefs.begin("irrigation", false);
  }
}

void storeMlDefaultsIfMissing(const char *key, int &target, int defaultValue) {
  if (!irrigationPrefs.isKey(key)) {
    target = defaultValue;
    irrigationPrefs.putInt(key, target);
  } else {
    target = irrigationPrefs.getInt(key, defaultValue);
  }
}

}  // namespace

void configInit() {
  ensurePrefs();
}

void configLoad() {
  ensurePrefs();

  storeMlDefaultsIfMissing("ml_pl", mlPl, mlPl);
  storeMlDefaultsIfMissing("ml_veg", mlVeg, mlVeg);
  storeMlDefaultsIfMissing("ml_pre", mlPre, mlPre);
  storeMlDefaultsIfMissing("ml_flo", mlFlo, mlFlo);
  storeMlDefaultsIfMissing("ml_fin", mlFin, mlFin);

  if (!irrigationPrefs.isKey("potL")) {
    irrigationPrefs.putFloat("potL", potVolumeL);
  }
  potVolumeL = irrigationPrefs.getFloat("potL", potVolumeL);

  if (!irrigationPrefs.isKey("flow")) {
    irrigationPrefs.putFloat("flow", pumpFlow);
  }
  pumpFlow = irrigationPrefs.getFloat("flow", pumpFlow);

  if (!irrigationPrefs.isKey("soilTh")) {
    irrigationPrefs.putInt("soilTh", soilThreshold);
  }
  soilThreshold = irrigationPrefs.getInt("soilTh", soilThreshold);

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
  irrigationPrefs.putInt("soilTh", soilThreshold);
  irrigationPrefs.putInt("stage", static_cast<int>(currentStage));
}

void configReset() {
  mlPl = 50;
  mlVeg = 100;
  mlPre = 150;
  mlFlo = 200;
  mlFin = 120;
  potVolumeL = 10.0f;
  pumpFlow = 50.0f;
  soilThreshold = 2000;
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

int getSoilThreshold() { return soilThreshold; }

plantStage getCurrentStage() { return currentStage; }

void updateStage(plantStage newStage) {
  currentStage = newStage;
  ensurePrefs();
  irrigationPrefs.putInt("stage", static_cast<int>(currentStage));
}

void setMlPerLiterForStage(plantStage stage, int value) {
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
      break;
  }
}

void setPotVolumeL(float liters) {
  potVolumeL = liters;
  ensurePrefs();
  irrigationPrefs.putFloat("potL", potVolumeL);
}

void setPumpFlow(float mlPerSecond) {
  pumpFlow = mlPerSecond;
  ensurePrefs();
  irrigationPrefs.putFloat("flow", pumpFlow);
}

void setSoilThreshold(int threshold) {
  soilThreshold = threshold;
  ensurePrefs();
  irrigationPrefs.putInt("soilTh", soilThreshold);
}
