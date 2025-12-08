#pragma once

#include <Preferences.h>

// Enumeración de etapas de la planta
enum plantStage {
  PLANTULA = 0,
  VEGETATIVO = 1,
  PRE_FLORACION = 2,
  FLORACION = 3,
  FINAL = 4,
};

void configInit();
void configLoad();
void configSave();
void configReset();
int getMlPerLiterForStage(plantStage stage);
float getPotVolumeL();
float getPumpFlow();
int getSoilThreshold();
int getSoilHighThreshold();
int getIrrigationIntervalDays();
unsigned long getLastIrrigationEpoch();
plantStage getCurrentStage();
void updateStage(plantStage newStage);

void setMlPerLiterForStage(plantStage stage, int value);
void setPotVolumeL(float liters);
void setPumpFlow(float mlPerSecond);
void setSoilThreshold(int threshold);
void setSoilHighThreshold(int threshold);
void setIrrigationIntervalDays(int days);
void setLastIrrigationEpoch(unsigned long epochSeconds);
