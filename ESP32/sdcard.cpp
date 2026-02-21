#include "sdcard.h"
#include "pins.h"
#include <SD.h>
#include <time.h>

bool sdReady = false;

// ─── helpers internos ───────────────────────────────────────────────────────

static void getLocalDt(struct tm &out) {
  time_t now = time(nullptr);
  localtime_r(&now, &out);
}

// Construye la ruta del directorio mensual: "/logs/MM"
static void monthDir(const struct tm &dt, char *buf, size_t len) {
  snprintf(buf, len, "/logs/%02d", dt.tm_mon + 1);
}

// Construye la ruta del archivo diario: "/logs/MM/YYYY-MM-DD.csv"
static void dayFile(const struct tm &dt, char *buf, size_t len) {
  snprintf(buf, len, "/logs/%02d/%04d-%02d-%02d.csv",
           dt.tm_mon + 1, 1900 + dt.tm_year, dt.tm_mon + 1, dt.tm_mday);
}

// Cabecera CSV en español
static const char CSV_HEADER[] =
  "fecha_hora,tipo,temp_c,hr_pct,suelo_pct,mq_raw,detalle";

// Escribe una fila en el archivo de log del día
// Los campos numéricos vienen como strings vacíos ("") cuando no aplican
static void writeLog(const char *tipo,
                     float tempC, float rh,
                     int soilPct, int mqRaw,
                     bool hasNumeric,
                     const String &detalle) {
  if (!sdReady) return;

  struct tm dt;
  getLocalDt(dt);

  // Asegurar directorio del mes
  char mDir[16];
  monthDir(dt, mDir, sizeof(mDir));
  if (!SD.exists(mDir)) {
    if (!SD.mkdir(mDir)) {
      Serial.printf("[SD] No se pudo crear %s\n", mDir);
      return;
    }
  }

  char fPath[40];
  dayFile(dt, fPath, sizeof(fPath));

  File f = SD.open(fPath, FILE_APPEND);
  if (!f) {
    Serial.printf("[SD] Error abriendo %s\n", fPath);
    return;
  }

  // Escribir cabecera si es archivo nuevo
  if (f.size() == 0) {
    f.println(CSV_HEADER);
  }

  // Construir timestamp
  char ts[20];
  snprintf(ts, sizeof(ts), "%04d-%02d-%02d %02d:%02d:%02d",
           1900 + dt.tm_year, dt.tm_mon + 1, dt.tm_mday,
           dt.tm_hour, dt.tm_min, dt.tm_sec);

  // Campos numéricos (vacíos si no aplica al tipo de evento)
  if (hasNumeric) {
    char numFields[48];
    if (!isnan(tempC) && !isnan(rh)) {
      snprintf(numFields, sizeof(numFields), "%.1f,%.1f,%d,%d",
               tempC, rh, soilPct, mqRaw);
    } else {
      snprintf(numFields, sizeof(numFields), ",,%d,%d", soilPct, mqRaw);
    }
    f.printf("%s,%s,%s,%s\n", ts, tipo, numFields, detalle.c_str());
  } else {
    f.printf("%s,%s,,,,,\"%s\"\n", ts, tipo, detalle.c_str());
  }

  f.close();
}

// ─── API pública ─────────────────────────────────────────────────────────────

bool sdInit() {
  if (!SD.begin(PIN_SD_CS)) {
    Serial.println("[SD] Tarjeta SD no detectada o fallo al iniciar");
    return false;
  }
  sdReady = true;

  // Crear directorio raíz de logs si no existe
  if (!SD.exists("/logs")) {
    SD.mkdir("/logs");
  }

  Serial.println("[SD] Tarjeta SD lista");
  return true;
}

void logSensors(float tempC, float rh, int soilPct, int mqRaw, bool valid) {
  if (!sdReady) return;
  if (valid) {
    writeLog("SENSOR", tempC, rh, soilPct, mqRaw, true, "");
  } else {
    // Sin lectura DHT válida: guardamos suelo y MQ igual
    writeLog("SENSOR", NAN, NAN, soilPct, mqRaw, true, "DHT invalido");
  }
}

void logAccion(const char *tipo, const String &detalle) {
  if (!sdReady) return;
  writeLog(tipo, NAN, NAN, -1, -1, false, detalle);
}

bool sdBuildLogIndex(String &outJson) {
  if (!sdReady) return false;

  File root = SD.open("/logs");
  if (!root || !root.isDirectory()) return false;

  // Recopilar meses ordenados
  // Usamos arrays simples para evitar std::vector (no disponible en todos los entornos)
  const int MAX_MONTHS = 12;
  const int MAX_FILES  = 62;  // máx días por mes × 2 por seguridad
  char months[MAX_MONTHS][4] = {};
  int  monthCount = 0;

  // Primera pasada: listar meses
  File mEntry = root.openNextFile();
  while (mEntry && monthCount < MAX_MONTHS) {
    if (mEntry.isDirectory()) {
      strncpy(months[monthCount], mEntry.name(), 3);
      months[monthCount][3] = '\0';
      monthCount++;
    }
    mEntry.close();
    mEntry = root.openNextFile();
  }
  root.close();

  // Construir JSON manualmente (sin ArduinoJson para no depender de heap extra)
  outJson = "{\"months\":[";
  for (int i = 0; i < monthCount; i++) {
    outJson += "\"";
    outJson += months[i];
    outJson += "\"";
    if (i < monthCount - 1) outJson += ",";
  }
  outJson += "],\"files\":{";

  for (int i = 0; i < monthCount; i++) {
    char mPath[12];
    snprintf(mPath, sizeof(mPath), "/logs/%s", months[i]);

    outJson += "\"";
    outJson += months[i];
    outJson += "\":[";

    File mDir = SD.open(mPath);
    if (mDir && mDir.isDirectory()) {
      int fileCount = 0;
      File fEntry = mDir.openNextFile();
      while (fEntry && fileCount < MAX_FILES) {
        if (!fEntry.isDirectory()) {
          if (fileCount > 0) outJson += ",";
          outJson += "\"";
          outJson += fEntry.name();
          outJson += "\"";
          fileCount++;
        }
        fEntry.close();
        fEntry = mDir.openNextFile();
      }
      mDir.close();
    }

    outJson += "]";
    if (i < monthCount - 1) outJson += ",";
  }

  outJson += "}}";
  return true;
}

String sdValidateLogPath(const String &rawPath) {
  // Debe empezar con /logs/ y no contener ..
  if (!rawPath.startsWith("/logs/")) return "";
  if (rawPath.indexOf("..") >= 0)    return "";
  // Verificar que existe en SD
  if (!sdReady)                       return "";
  if (!SD.exists(rawPath.c_str()))    return "";
  return rawPath;
}
