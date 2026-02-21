#pragma once
#include <Arduino.h>

// true si la SD fue inicializada correctamente
extern bool sdReady;

// Inicializa la SD; debe llamarse después de sincronizar NTP
bool   sdInit();

// Registra una lectura periódica de sensores
void   logSensors(float tempC, float rh, int soilPct, int mqRaw, bool valid);

// Registra un evento o acción (tipo: "RIEGO","LUZ_ON","LUZ_OFF","VENT",
//                                     "ALERTA_ON","ALERTA_OFF","CMD","INICIO")
void   logAccion(const char *tipo, const String &detalle);

// Construye un JSON con el índice de meses y archivos disponibles en /logs
// Retorna false si la SD no está disponible
bool   sdBuildLogIndex(String &outJson);

// Valida que rawPath sea un path seguro dentro de /logs
// Retorna el path validado, o String vacío si inválido o no existe
String sdValidateLogPath(const String &rawPath);
