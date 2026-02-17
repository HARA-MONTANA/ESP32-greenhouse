#pragma once

#include "config.h"

void initIrrigationHardware();
int readSoilMoisture();
int soilPercentFromAdc(int reading);
bool isTankWaterAvailable();
void pumpOn();
void pumpOff();
bool checkSoilAndIrrigate();
void irrigate(int initialSoilReading);
void irrigateVolume(float totalMl, int initialSoilReading = -1);
void setAutoIrrigationEnabled(bool enabled);
bool isAutoIrrigationEnabled();
