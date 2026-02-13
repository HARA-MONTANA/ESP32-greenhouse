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
int getLightHoursForStage(plantStage stage);
float getPotVolumeL();
float getPumpFlow();
bool isPumpCalibrated();
int getSoilThreshold();
int getSoilHighThreshold();
int getIrrigationIntervalDays();
unsigned long getLastIrrigationEpoch();
plantStage getCurrentStage();
void updateStage(plantStage newStage);

bool setMlPerLiterForStage(plantStage stage, int value);
bool setPotVolumeL(float liters);
bool setPumpFlow(float mlPerSecond);
void setPumpCalibrated(bool calibrated);
bool setSoilThreshold(int threshold);
bool setSoilHighThreshold(int threshold);
bool setIrrigationIntervalDays(int days);
void setLastIrrigationEpoch(unsigned long epochSeconds);
bool setLightHoursForStage(plantStage stage, int hours);

bool getAutoIrrigationStored();
void setAutoIrrigationStored(bool enabled);
