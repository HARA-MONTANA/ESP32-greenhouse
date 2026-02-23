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
   - Presionar el botón GPIO 27 durante el arranque salta todos los prompts y usa las credenciales guardadas.
3. Ingresar el offset UTC de la zona horaria (ej. `-3`, `-5`). Enter mantiene el valor guardado.
4. El firmware sincroniza la hora por NTP y actualiza el RTC DS3231.
5. Una vez conectado, el dashboard web queda disponible en `http://<IP>/`.

Las credenciales (WiFi, token) se guardan en NVS y se reutilizan en futuros arranques.

## Dashboard web

El firmware sirve un dashboard en el puerto **80** con tres pestañas:

- **Dashboard** — gauges en tiempo real de temperatura, humedad, suelo y calidad del aire; control de LED, ventiladores, riego y bomba; vista de cámara IP (MJPEG); últimos eventos.
- **Logs** — navegación y descarga de archivos CSV de la SD por mes.
- **Config** — configuración completa de planta, alertas, suelo, bomba, Telegram y zona horaria.

La comunicación con el ESP32 es por **WebSocket** (`/ws`). Los controles del dashboard envían los mismos comandos que Telegram/Serial.

### API HTTP

| Ruta | Método | Descripción |
|------|--------|-------------|
| `/` | GET | Sirve el dashboard |
| `/api/config` | GET | Configuración actual como JSON |
| `/api/logs` | GET | Índice de meses y archivos de log en SD |
| `/api/logfile?path=...` | GET | Descarga o muestra un CSV de log (`&dl=1` para descarga) |
| `/api/telegram` | POST | Actualiza token y/o nombre del bot (`{"token":"...","name":"..."}`) |

## Logs en SD

La SD almacena dos tipos de registros en `/logs/MM/YYYY-MM-DD.csv`:

- **Sensores** — lectura periódica cada 5 minutos, alineada al reloj (`:00`, `:05`, `:10`...).
- **Acciones** — riego, luz ON/OFF, alertas, comandos y arranque del sistema.

Columnas del CSV: `timestamp, tipo, detalle, temp_c, rh_pct, suelo_pct, mq_raw`.

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
