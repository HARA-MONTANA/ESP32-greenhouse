#pragma once

#include "config.h"

void initIrrigationHardware();
int readSoilMoisture();
int soilPercentFromAdc(int reading);
void updateTankFloat();      // llama desde loop() para debounce del flotador
bool isTankWaterAvailable(); // retorna valor con debounce
void pumpOn();
void pumpOff();
bool isIrrigating();         // true mientras hay riego en curso
void tickIrrigation();       // llama desde loop() para avanzar la maquina de estados
bool checkSoilAndIrrigate();
void irrigate(int initialSoilReading);
void irrigateVolume(float totalMl, int initialSoilReading = -1);
void setAutoIrrigationEnabled(bool enabled);
bool isAutoIrrigationEnabled();
