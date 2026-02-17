# Plan de Simplificación - ESP32 Greenhouse

## Problemas actuales

1. **ESP32.ino monolítico** (1,775 líneas) - mezcla todo: WiFi, Telegram, sensores, comandos, alertas
2. **100+ variables globales** dispersas sin organización
3. **Comandos duplicados** - Serial y Telegram tienen parsers separados con lógica repetida
4. **3 namespaces NVS** (cred, runtime, irrigation) - complejidad innecesaria
5. **Sistema de credenciales sobrecomplicado** - botón skip + prompts con timeout + lógica de fallback
6. **Código muerto** - `runPulsedIrrigation` nunca se usa, formato de reporte dual
7. **Broadcast con deduplicación** - tracking de último mensaje serial/telegram innecesario
8. **`soilPercentFromAdc`** definido en ESP32.ino pero declarado en irrigation.h - confuso
9. **Horas de luz fijas** (pre/flo/fin = 12h) se guardan/cargan de NVS sin necesidad
10. **Timezone prompt** con lógica de botón - sobrecomplicado

## Cambios propuestos

### 1. Comando unificado
- Eliminar `handleTelegramCommand` separado
- Un solo parser `handleCommand(args)` que funcione para Serial y Telegram
- Serial: `estado` → llama handleCommand
- Telegram: `/estado` → strip `/`, llama handleCommand

### 2. Simplificar credenciales
- Si hay credenciales en NVS → usarlas automáticamente (sin botón, sin prompt)
- Si no hay → pedir por Serial (simple, sin timeout complejo)
- Eliminar PIN_CRED_SKIP y toda la lógica del botón skip

### 3. Eliminar código muerto
- Eliminar `runPulsedIrrigation` y `PulseSummary`
- Eliminar formato compact de reportes (`formatCompactReport`, `stageToCode`, `formatShortLastIrrigation`)
- Solo un formato de estado: `formatStatus()`
- Eliminar `ReportFormat` enum y variables asociadas

### 4. Consolidar NVS (3→2 namespaces)
- "cred" → WiFi + Telegram (se queda igual)
- "config" → unificar "irrigation" + "runtime" en un solo namespace

### 5. Mover `soilPercentFromAdc` a irrigation.cpp
- Donde pertenece lógicamente

### 6. Simplificar broadcast
- Eliminar deduplicación (lastSerialMessage, lastTelegramMessage, etc.)
- Simplemente enviar a Serial + todos los chats autorizados

### 7. Simplificar timezone
- Si hay offset guardado en NVS → usarlo
- Si no → usar default (-5) y permitir cambiarlo por comando
- Agregar comando `/timezone [offset]` en vez del prompt interactivo en setup

### 8. Limpiar variables globales
- Reducir booleanos de alerta a un esquema más simple
- Agrupar variables relacionadas

## Resultado esperado
- ~600-800 líneas menos de código
- Un solo flujo de comandos
- Menos puntos de fallo (menos lógica de reintentos, menos prompts interactivos)
- Setup más rápido (sin prompts si hay credenciales guardadas)
- Código más fácil de leer y mantener
