# ESP32-greenhouse

Firmware para automatizar un invernadero con ESP32. Controla riego, iluminación, ventilación y monitoreo ambiental. Se opera por Telegram, por el monitor serie o desde el dashboard web integrado.

## Pines

| Pin | GPIO | Función |
|-----|------|---------|
| PIN_DHT | 25 | Sensor DHT22 (temperatura y humedad) |
| PIN_LED_MORADO | 26 | MOSFET — LED morado auxiliar (PWM) |
| PIN_CRED_SKIP | 27 | Botón para saltar prompts de credenciales (activo LOW) |
| PIN_MQ135 | 34 | Sensor MQ-135 calidad de aire (ADC) |
| PIN_SUELO | 32 | Sensor de humedad de suelo (ADC) |
| PIN_I2C_SDA | 21 | SDA del RTC DS3231 |
| PIN_I2C_SCL | 22 | SCL del RTC DS3231 |
| PIN_SD_CS | 5 | Chip Select SD |
| PIN_FAN_PWM | 16 | PWM ventiladores (MOSFET, 25 kHz) |
| PIN_RELE1 | 17 | Bomba de agua DC (MOSFET) |
| PIN_RELE2 | 13 | Luz AC (relé) |
| PIN_FLOAT | 33 | Flotador nivel de agua |

## Primer arranque

1. Conectar el ESP32 por USB y abrir el monitor serie a **115200 baud**.
2. Ingresar SSID de WiFi, contraseña y token de Telegram cuando se soliciten.
   - Presionar Enter sin escribir nada reutiliza el valor guardado en NVS.
   - Presionar el botón GPIO 27 durante el arranque salta **todos** los prompts y usa los valores guardados.
3. Ingresar la URL del webhook de Google Drive (opcional). Enter la omite; ver [Logs en Google Drive](#logs-en-google-drive).
4. Ingresar el offset UTC de la zona horaria (ej. `-3`, `-5`). Enter mantiene el valor guardado.
5. El firmware sincroniza la hora por NTP y actualiza el RTC DS3231.
6. Una vez conectado, el dashboard web queda disponible en `http://<IP>/`.

Las credenciales (WiFi, token, URL de Google Drive) se guardan en NVS y se reutilizan en futuros arranques.

## Dashboard web

El firmware sirve una SPA (Single Page Application) en el puerto **80**, accesible desde cualquier navegador en la misma red. La comunicación es por **WebSocket** (`/ws`) con actualizaciones cada ~2 segundos. Los controles envían los mismos comandos que Telegram/Serial, por lo que todo cambio se refleja de inmediato en el hardware.

En la cabecera se muestra la IP del ESP32, el reloj en tiempo real sincronizado con el RTC, un indicador del estado de la conexión WebSocket (verde = conectado), y un campo para el nombre del bot de Telegram que genera un enlace directo al chat.

### Pestaña Dashboard

**Gauges de sensores** — cuadrícula de 6 medidores circulares con animación y cambio de color según umbrales:

| Gauge | Rango | Alerta visual |
|-------|-------|---------------|
| Temperatura | 0 – 50 °C | Rojo al superar el umbral configurado |
| Humedad | 0 – 100 % | Rojo si cae bajo el mínimo o supera el máximo |
| Humedad suelo | 0 – 100 % | Indicativo respecto a umbrales de riego |
| Calidad de aire MQ | 0 – 4095 raw | Rojo al superar el umbral configurado |
| Fan RPM | 0 – máx | Sin alerta de color |
| Suelo RAW (ADC) | 0 – 4095 | Sin alerta de color (útil para calibración) |

Cada gauge tiene un botón de expansión que muestra un **sparkline** con la historia de los últimos 10 minutos.

**Cámara IP** — reproduce un stream MJPEG ingresando la URL de cualquier cámara en la red local (ej. ESP32-CAM).

**Última actividad** — lista de los últimos eventos del sistema con badge de color por tipo (`RIEGO`, `LUZ_ON`, `LUZ_OFF`, `ALERTA_ON`, `ALERTA_OFF`) y botón de refresco.

**Controles:**

| Card | Controles disponibles |
|------|-----------------------|
| LED Morado | Botones ON/OFF + slider de intensidad (1–100 %) |
| Ventiladores | Slider de velocidad manual (0–100 %) + toggle Auto/Manual + gráfico de RPM en tiempo real |
| Riego | Toggle Auto-riego + campo mL para riego manual + indicador de nivel del tanque |

### Pestaña Logs

Permite navegar y visualizar los archivos CSV de la tarjeta SD directamente desde el navegador:

- **Selector de mes** — lista los directorios disponibles en `/logs/`.
- **Selector de archivo** — lista los CSV del mes seleccionado.
- **Vista de tabla** — muestra el CSV con las filas coloreadas por tipo de evento (riego en cyan, luz encendida en verde, alertas en rojo/verde).
- **Botón Descargar** — descarga el CSV seleccionado al dispositivo.

### Pestaña Configuración

Secciones colapsables, cada una con un botón de guardar independiente. Los cambios se aplican inmediatamente vía WebSocket y se persisten en NVS.

| Sección | Qué se configura |
|---------|-----------------|
| **Telegram** | Token del bot y nombre para el enlace directo (el token se muestra oculto) |
| **Planta** | Etapa de crecimiento, volumen de maceta, mL/L por etapa, horas de luz por etapa, intensidad del LED |
| **Alertas** | Temperatura máxima, humedad mínima (plántula/vegetativo), humedad máxima (pre-flor en adelante), umbral MQ de calidad de aire |
| **Suelo y Bomba** | Umbral seco y húmedo para riego automático; calibración ADC del sensor (bloqueada por defecto, desbloquear con botón) |
| **Bomba** | Calibración de caudal: ejecuta la bomba 5 s y registra el volumen recolectado |
| **Sistema** | Offset de zona horaria (UTC±h) |
| **Redes WiFi** | Lista de redes guardadas (máx 5) con opción de agregar, editar y eliminar; el ESP32 las prueba automáticamente en orden si la red principal falla |

### API HTTP

| Ruta | Método | Descripción |
|------|--------|-------------|
| `/` | GET | Sirve el dashboard |
| `/api/config` | GET | Configuración actual como JSON |
| `/api/logs` | GET | Índice de meses y archivos de log en SD |
| `/api/logfile?path=...` | GET | Muestra un CSV de log (`&dl=1` para descarga directa) |
| `/api/telegram` | POST | Actualiza token y/o nombre del bot (`{"token":"...","name":"..."}`) |
| `/api/wifi` | GET | Lista las redes WiFi guardadas |
| `/api/wifi/add` | POST | Agrega una red WiFi guardada |

## Logs en SD

La SD almacena dos tipos de registros en `/logs/MM/YYYY-MM-DD.csv`:

- **SENSOR** — lectura periódica cada 5 minutos, alineada al reloj (`:00`, `:05`, `:10`...).
- **Acciones** — `RIEGO`, `LUZ_ON`, `LUZ_OFF`, `ALERTA_ON`, `ALERTA_OFF`, `CMD`, `INICIO`.

Columnas del CSV: `fecha_hora, tipo, temp_c, hr_pct, suelo_pct, mq_raw, detalle`.

## Logs en Google Drive

Cuando está configurado, cada entrada que se escribe en la SD también se envía a Google Drive, replicando la misma estructura de carpetas y formato de CSV:

```
Mi unidad/
└── logs/
    ├── 02-2026/
    │   ├── 2026-02-25.csv
    │   └── 2026-02-27.csv
    └── 03-2026/
        └── 2026-03-01.csv
```

El mecanismo usa un **Google Apps Script** desplegado como aplicación web. El ESP32 envía un POST HTTPS con los datos del log; el script escribe la fila en el archivo CSV correspondiente dentro de Google Drive usando la API de Drive. No se requiere OAuth2 en el ESP32.

### Configuración inicial (una sola vez)

**1. Crear el Apps Script**

1. Abre [script.google.com](https://script.google.com) y haz clic en **Nuevo proyecto**.
   - Alternativamente: en [drive.google.com](https://drive.google.com) → **Nuevo → Más → Google Apps Script**.
   - El script no usa ninguna hoja de cálculo; crea y gestiona directamente una carpeta `logs/` con archivos CSV en tu Drive.
2. Borra el contenido del editor y pega el contenido del archivo `ESP32/apps_script.js` incluido en este repositorio.
3. Guarda con **Ctrl+S** (puedes darle cualquier nombre al proyecto).

**2. Desplegar como aplicación web**

1. Botón **Desplegar → Nueva implementación**.
2. Haz clic en el engranaje ⚙ junto a "Tipo" y elige **Aplicación web**.
3. Configura:
   - **Ejecutar como:** Yo *(tu cuenta de Google)*
   - **Acceso:** Cualquier usuario
4. Haz clic en **Desplegar** y autoriza los permisos de Drive cuando se soliciten.
5. Copia la **URL de la aplicación web** que aparece al finalizar. Tiene el formato:
   ```
   https://script.google.com/macros/s/XXXXXXXXXXXXXXXXXX/exec
   ```

**3. Configurar en el ESP32**

**Opción A — durante el arranque (monitor serie):**
```
Google Drive Apps Script URL (opcional):
  Pega la URL del webhook para guardar logs en Drive.
  Enter para omitir.
> https://script.google.com/macros/s/XXXXXXXX/exec
```

**Opción B — en cualquier momento (Telegram o Serial):**
```
/gdrive https://script.google.com/macros/s/XXXXXXXX/exec
```

La URL se guarda en NVS. Desde ese momento cada log (sensor y acción) se envía a Drive además de escribirse en la SD.

### Notas

- Si el ESP32 no tiene WiFi al momento de generar un log, la entrada se omite en Drive (pero siempre se guarda en la SD).
- Para ver el estado actual: `/gdrive`
- Para desactivar: `/gdrive off`
- Si redespliegas el Apps Script (nueva versión), debes actualizar la URL en el ESP32 con `/gdrive <nueva-url>`.
- El archivo CSV se crea automáticamente si no existe, incluyendo la cabecera en la primera fila.

## Dependencias

Instalar desde el Gestor de Librerías de Arduino:

- **UniversalTelegramBot**
- **RTClib** (Adafruit)
- **DHT sensor library** (Adafruit)
- **ESPAsyncWebServer**
- **AsyncTCP**
- **ArduinoJson**

Incluidas con el core ESP32:

- WiFi, WiFiClientSecure, Wire, Preferences

## Comandos

Los comandos funcionan igual en **Telegram** (con `/` delante), **Serial** y **WebSocket**. Todos los valores configurables se guardan en NVS y sobreviven reinicios.

### Uso diario

| Comando | Descripción |
|---------|-------------|
| `/start` | Muestra la ayuda |
| `/estado` | Estado actual del invernadero |
| `/regar [mL]` | Riego manual (máx 1500 mL) |
| `/autoriego [on\|off]` | Activar/desactivar riego automático |
| `/vent [0-100]` | Ventilador en modo manual al % indicado |
| `/ventauto [on\|off]` | Ventilador en modo automático |
| `/reportes [on\|off] [min]` | Reportes periódicos por Telegram (intervalo en minutos) |

### Configuración de la planta

| Comando | Descripción |
|---------|-------------|
| `/config` | Ver toda la configuración |
| `/etapa [pl\|veg\|pre\|flo\|fin]` | Cambiar etapa de crecimiento |
| `/maceta [litros]` | Volumen de la maceta (1-50 L) |
| `/ml [etapa] [valor]` | mL por litro de maceta para una etapa |
| `/luz [etapa] [horas]` | Horas de luz para plántula o vegetativo (12-20 h) |
| `/led [on\|off\|0-100]` | LED morado: on/off manual o brillo (respeta el horario) |
| `/pausariego [dias]` | Días mínimos entre riegos automáticos (1-5) |
| `/timezone [offset]` | Zona horaria como offset UTC en horas (ej. `-3`) |

### Sensor de suelo

| Comando | Descripción |
|---------|-------------|
| `/suelomin [%]` | Umbral mínimo de humedad para regar (0-50%) |
| `/suelomax [%]` | Umbral máximo de humedad del suelo (50-100%) |
| `/calsuelo [SECO] [HUMEDO]` | Calibrar sensor con valores ADC seco y húmedo |

### Alertas

| Comando | Descripción |
|---------|-------------|
| `/tempmax [C]` | Alerta de temperatura alta |
| `/hummin [%]` | Alerta de humedad ambiente baja (plántula y vegetativo) |
| `/hummax [%]` | Alerta de humedad ambiente alta (floración y final) |
| `/airemax [N]` | Alerta de calidad de aire (valor raw del sensor MQ) |

### Calibración de la bomba

| Comando | Descripción |
|---------|-------------|
| `/calibrar` | Activa la bomba 5 segundos para medir el volumen |
| `/caudal [mL]` | Ingresa el volumen medido para calcular el caudal (mL/s) |

Pasos: envía `/calibrar`, mide el agua que salió, luego envía `/caudal [mL]`.

### Administración

| Comando | Descripción |
|---------|-------------|
| `/addid [ID]` | Autorizar un chat de Telegram (máx 5) |
| `/delid [ID]` | Eliminar un chat autorizado |
| `/ids` | Ver chats autorizados |
| `/reset` | Restablecer configuración de planta a valores por defecto |
| `/gdrive [url\|off]` | Configurar o desactivar el webhook de Google Drive |

## Etapas de crecimiento

| Código | Etapa | Luz (h) | mL/L (por defecto) | LED morado |
|--------|-------|---------|------|------------|
| pl | Plántula | 18 (configurable) | 50 | Solo si `/led on` |
| veg | Vegetativo | 18 (configurable) | 100 | Solo si `/led on` |
| pre | Pre-floración | 12 (fija) | 150 | Automático |
| flo | Floración | 12 (fija) | 200 | Automático |
| fin | Final | 12 (fija) | 120 | Automático |

Las horas de luz solo se pueden modificar en plántula y vegetativo. El LED morado se activa automáticamente en pre-floración, floración y final; en las etapas anteriores requiere activación manual con `/led on`.

Las luces se encienden a las **06:00** y se apagan según las horas configuradas para la etapa activa.

## Ventilación automática

En modo automático el ventilador sube proporcionalmente cuando la temperatura supera 24 °C (llega a 100% al alcanzar el umbral de alerta) o cuando el sensor MQ supera el umbral configurado. Se puede forzar un porcentaje fijo con `/vent [0-100]` y volver al modo automático con `/ventauto on`.
