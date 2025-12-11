#pragma once

#include "config.h"

void initIrrigationHardware();
int readSoilMoisture();
bool isTankWaterAvailable();
int soilPercentFromAdc(int reading);
bool checkSoilAndIrrigate();
void irrigate(int initialSoilReading);
void irrigateVolume(float totalMl, int initialSoilReading = -1);
void setAutoIrrigationEnabled(bool enabled);
bool isAutoIrrigationEnabled();
