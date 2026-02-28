#include "irrigation.h"

#include <Arduino.h>
#include <time.h>

#include "pins.h"

// Declaradas en ESP32.ino / sdcard.cpp
void broadcastMessage(const String &msg);
String stageToString(plantStage stage);
void logAccion(const char *tipo, const String &detalle);
bool readAmbient(float &tempC, float &rh);
void logAccionConSensores(const char *tipo, const String &detalle,
                           float tempC, float rh, int soilPct, int mqRaw);

namespace {
const int PUMP_ON_LEVEL = HIGH;
const int PUMP_OFF_LEVEL = LOW;
bool autoIrrigationEnabled = false;
bool tankEmptyNotified = false;
bool pumpNotCalibratedNotified = false;

// Proteccion capa 1: limite de riegos por dia (ventana de 24h en RAM)
const int MAX_DAILY_IRRIGATIONS = 4;
unsigned long windowStartMs = 0;
int dailyIrrigationCount = 0;
bool dailyLimitNotified = false;

// Proteccion capa 2: deteccion de falla de hardware en sensor de suelo
// ADC ESP32 es de 12 bits (0-4095). Extremos indican corto o circuito abierto.
const int ADC_FAULT_LOW  = 50;    // cortocircuito: los pines estan en corto
const int ADC_FAULT_HIGH = 4050;  // circuito abierto: sensor desconectado o roto

// Debounce del flotador: requiere 3 lecturas consecutivas HIGH (sin agua)
// para reportar tanque vacio. Evita falsos positivos por vibracion mecanica.
const int FLOAT_DEBOUNCE_COUNT = 3;
int floatLowCount = FLOAT_DEBOUNCE_COUNT;  // inicia asumiendo agua disponible
bool floatDebounced = true;

// Maquina de estados para riego no bloqueante
enum class IrrigationPhase { IDLE, PUMPING, SETTLING };
IrrigationPhase irrigPhase = IrrigationPhase::IDLE;
unsigned long pumpStartMs    = 0;
unsigned long pumpDurMs      = 0;
unsigned long settleStartMs  = 0;
float  irrigTotalMl   = 0;
int    irrigInitPct   = 0;
}  // namespace

void initIrrigationHardware() {
  pinMode(PIN_PUMP, OUTPUT);
  digitalWrite(PIN_PUMP, PUMP_OFF_LEVEL);
  pinMode(PIN_FLOAT, INPUT_PULLUP);
  autoIrrigationEnabled = getAutoIrrigationStored();
}

void pumpOn() { digitalWrite(PIN_PUMP, PUMP_ON_LEVEL); }
void pumpOff() { digitalWrite(PIN_PUMP, PUMP_OFF_LEVEL); }

int readSoilMoisture() { return analogRead(PIN_SUELO); }

// Actualiza el debounce del flotador; llamar desde el loop principal (~2 s).
// El flotador es mecanico: requiere 3 lecturas consecutivas HIGH para declarar
// el tanque vacio, evitando falsas alertas por vibracion.
void updateTankFloat() {
  bool raw = digitalRead(PIN_FLOAT) == LOW;  // LOW = float arriba = agua disponible
  if (raw) {
    floatLowCount = FLOAT_DEBOUNCE_COUNT;
    floatDebounced = true;
  } else {
    if (floatLowCount > 0) floatLowCount--;
    floatDebounced = (floatLowCount > 0);
  }
}

bool isTankWaterAvailable() { return floatDebounced; }

bool isIrrigating() { return irrigPhase != IrrigationPhase::IDLE; }

int soilPercentFromAdc(int reading) {
  const int dryAdc = getSoilDryAdc();
  const int wetAdc = getSoilWetAdc();
  const int minR = min(wetAdc, dryAdc);
  const int maxR = max(wetAdc, dryAdc);
  int clamped = constrain(reading, minR, maxR);
  int percent = map(clamped, wetAdc, dryAdc, 100, 0);
  return constrain(percent, 0, 100);
}

bool checkSoilAndIrrigate() {
  if (isIrrigating()) return false;  // ya hay un riego en curso

  const int soilReading = readSoilMoisture();
  const int soilPercent = soilPercentFromAdc(soilReading);

  if (!isPumpCalibrated()) {
    if (!pumpNotCalibratedNotified) {
      broadcastMessage("Riego omitido: bomba sin calibrar.");
      pumpNotCalibratedNotified = true;
    }
    return false;
  }
  pumpNotCalibratedNotified = false;

  if (!autoIrrigationEnabled) return false;

  // Proteccion capa 2: valores ADC en extremos indican falla de hardware
  if (soilReading <= ADC_FAULT_LOW || soilReading >= ADC_FAULT_HIGH) {
    const char* tipo = soilReading <= ADC_FAULT_LOW ? "cortocircuito" : "circuito abierto";
    broadcastMessage("ALERTA: Sensor de suelo dañado (" + String(tipo) +
                     ", ADC=" + String(soilReading) + "). Auto riego desactivado.");
    setAutoIrrigationEnabled(false);
    return false;
  }

  if (!isTankWaterAvailable()) {
    if (!tankEmptyNotified) {
      broadcastMessage("Riego omitido: tanque sin agua.");
      tankEmptyNotified = true;
    }
    return false;
  }
  tankEmptyNotified = false;

  const int highThreshold = getSoilHighThreshold();
  if (highThreshold > 0 && soilPercent >= highThreshold) return false;
  if (soilPercent >= getSoilThreshold()) return false;

  // Cooldown minimo de 15 minutos entre riegos para no ciclar la bomba
  time_t now;
  time(&now);
  const unsigned long lastEpoch = getLastIrrigationEpoch();
  if (now > 0 && lastEpoch > 0 && difftime(now, static_cast<time_t>(lastEpoch)) < 900.0) {
    return false;
  }

  // Proteccion capa 1: maximo MAX_DAILY_IRRIGATIONS riegos automaticos por dia
  unsigned long nowMs = millis();
  if (windowStartMs == 0 || nowMs - windowStartMs >= 86400000UL) {
    windowStartMs = nowMs;
    dailyIrrigationCount = 0;
    dailyLimitNotified = false;
  }
  if (dailyIrrigationCount >= MAX_DAILY_IRRIGATIONS) {
    if (!dailyLimitNotified) {
      broadcastMessage("ALERTA: Limite diario de " + String(MAX_DAILY_IRRIGATIONS) +
                       " riegos alcanzado. Verifica el sensor de suelo.");
      dailyLimitNotified = true;
    }
    return false;
  }

  dailyIrrigationCount++;
  irrigate(soilReading);
  return true;
}

void irrigate(int initialSoilReading) {
  const float totalMl = getMlPerLiterForStage(getCurrentStage()) * getPotVolumeL();
  irrigateVolume(totalMl, initialSoilReading);
}

// Inicia el riego de forma NO BLOQUEANTE. El ciclo completo (bombeo + asentamiento
// + log) lo lleva a cabo tickIrrigation(), que debe llamarse desde loop().
void irrigateVolume(float totalMl, int initialSoilReading) {
  const float flow = getPumpFlow();

  if (!isPumpCalibrated() || flow <= 0.0f) {
    broadcastMessage("Riego omitido: bomba sin calibrar.");
    return;
  }

  if (isIrrigating()) {
    broadcastMessage("Riego ya en curso; intenta de nuevo al terminar.");
    return;
  }

  const int initialAdc = initialSoilReading >= 0 ? initialSoilReading : readSoilMoisture();
  irrigInitPct  = soilPercentFromAdc(initialAdc);
  irrigTotalMl  = totalMl;
  pumpDurMs     = static_cast<unsigned long>((totalMl / flow) * 1000.0f);

  pumpOn();
  pumpStartMs = millis();
  irrigPhase  = IrrigationPhase::PUMPING;
}

// Avanza la maquina de estados de riego. Llamar desde loop() en cada iteracion.
// Maneja: fin de bombeo -> asentamiento (5 s) -> log y actualizacion de timestamp.
void tickIrrigation() {
  if (irrigPhase == IrrigationPhase::IDLE) return;

  unsigned long now = millis();

  if (irrigPhase == IrrigationPhase::PUMPING) {
    if (now - pumpStartMs >= pumpDurMs) {
      pumpOff();
      settleStartMs = now;
      irrigPhase = IrrigationPhase::SETTLING;
    }
    return;
  }

  if (irrigPhase == IrrigationPhase::SETTLING) {
    if (now - settleStartMs < 5000) return;

    const int finalAdc  = readSoilMoisture();
    const int finalPct  = soilPercentFromAdc(finalAdc);
    plantStage stage    = getCurrentStage();

    String msg = "Riego completado | Etapa: " + stageToString(stage);
    msg += " | " + String(irrigTotalMl, 1) + " mL";
    msg += " | Bomba: " + String(pumpDurMs / 1000.0f, 1) + " s";
    msg += " | Suelo: " + String(irrigInitPct) + "% -> " + String(finalPct) + "%";
    broadcastMessage(msg);

    String det = "etapa " + stageToString(stage)
               + "; " + String(irrigTotalMl, 1) + " mL"
               + "; bomba " + String(pumpDurMs / 1000.0f, 1) + "s"
               + "; suelo " + String(irrigInitPct) + "%->" + String(finalPct) + "%";
    float irrTemp = NAN, irrRh = NAN;
    readAmbient(irrTemp, irrRh);
    logAccionConSensores("RIEGO", det, irrTemp, irrRh, finalPct, -1);

    if (finalPct <= irrigInitPct) {
      broadcastMessage("Riego sin incremento de humedad; verifica bomba y mangueras.");
    }

    time_t t;
    time(&t);
    if (t > 0) setLastIrrigationEpoch(static_cast<unsigned long>(t));

    irrigPhase = IrrigationPhase::IDLE;
  }
}

void setAutoIrrigationEnabled(bool enabled) {
  autoIrrigationEnabled = enabled;
  setAutoIrrigationStored(enabled);
}

bool isAutoIrrigationEnabled() { return autoIrrigationEnabled; }
