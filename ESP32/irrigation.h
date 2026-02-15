#pragma once

#include "config.h"

void initIrrigationHardware();
int readSoilMoisture();
bool isTankWaterAvailable();
void pumpOn();
void pumpOff();
int soilPercentFromAdc(int reading);
bool checkSoilAndIrrigate();
void irrigate(int initialSoilReading);
void irrigateVolume(float totalMl, int initialSoilReading = -1);
void updateIrrigation();
bool isIrrigating();
void setAutoIrrigationEnabled(bool enabled);
bool isAutoIrrigationEnabled();
