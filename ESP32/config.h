#pragma once

#include <Preferences.h>

// Etapas de la planta
enum plantStage {
  PLANTULA = 0,
  VEGETATIVO = 1,
  PRE_FLORACION = 2,
  FLORACION = 3,
  FINAL = 4,
};

// Inicializar y cargar toda la configuración desde NVS
void configInit();
void configSave();
void configReset();

// --- Getters de riego/planta ---
plantStage getCurrentStage();
int getMlPerLiterForStage(plantStage stage);
int getLightHoursForStage(plantStage stage);
float getPotVolumeL();
float getPumpFlow();
bool isPumpCalibrated();
int getSoilThreshold();
int getSoilHighThreshold();
int getIrrigationIntervalDays();
unsigned long getLastIrrigationEpoch();
bool getAutoIrrigationStored();

// --- Getters de runtime (antes en namespace separado) ---
int getTempAlertThreshold();
int getRhLowAlertThreshold();
int getRhHighAlertThreshold();
int getMqAlertThreshold();
int getSoilDryAdc();
int getSoilWetAdc();
bool getAutoReadingsEnabled();
unsigned long getAutoReadingsIntervalMs();
int getTimezoneOffsetHours();

// --- Setters de riego/planta ---
void updateStage(plantStage newStage);
bool setMlPerLiterForStage(plantStage stage, int value);
bool setPotVolumeL(float liters);
bool setPumpFlow(float mlPerSecond);
void setPumpCalibrated(bool calibrated);
bool setSoilThreshold(int threshold);
bool setSoilHighThreshold(int threshold);
bool setIrrigationIntervalDays(int days);
void setLastIrrigationEpoch(unsigned long epochSeconds);
void setAutoIrrigationStored(bool enabled);
bool setLightHoursForStage(plantStage stage, int hours);

// --- Setters de runtime ---
void setTempAlertThreshold(int val);
void setRhLowAlertThreshold(int val);
void setRhHighAlertThreshold(int val);
void setMqAlertThreshold(int val);
void setSoilCalibration(int dryAdc, int wetAdc);
void setAutoReadings(bool enabled, unsigned long intervalMs);
void setTimezoneOffsetHours(int offset);

// --- LED morado ---
int  getLedIntensity();
bool setLedIntensity(int pct);  // rango 1-100
