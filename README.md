# ESP32-greenhouse

Firmware para automatizar un invernadero con ESP32. Controla riego, iluminación, ventilación y monitoreo ambiental. Se opera por Telegram o por el monitor serie.

## Pines

| Pin | GPIO | Función |
|-----|------|---------|
| PIN_DHT | 25 | Sensor DHT22 (temperatura y humedad) |
| PIN_LED_MORADO | 26 | MOSFET — LED morado auxiliar |
| PIN_CRED_SKIP | 27 | Botón para saltar credenciales (activo LOW) |
| PIN_MQ135 | 34 | Sensor MQ-135 calidad de aire (ADC) |
| PIN_SUELO | 32 | Sensor de humedad de suelo (ADC) |
| PIN_I2C_SDA | 21 | SDA del RTC DS3231 |
| PIN_I2C_SCL | 22 | SCL del RTC DS3231 |
| PIN_SD_CS | 5 | Chip Select SD (reservado) |
| PIN_FAN_PWM | 16 | PWM ventiladores (MOSFET) |
| PIN_RELE1 | 17 | Bomba de agua DC (MOSFET) |
| PIN_RELE2 | 13 | Luz AC (relé) |
| PIN_FLOAT | 33 | Flotador nivel de agua |

## Primer arranque

1. Conectar el ESP32 por USB y abrir el monitor serie a 115200 baud.
2. Ingresar SSID de WiFi, contraseña y token de Telegram cuando se soliciten.
3. (Opcional) Presionar el botón GPIO 27 en cualquier momento durante el arranque para reutilizar credenciales guardadas.
4. El firmware sincroniza la hora por NTP y verifica el token de Telegram.

Las credenciales se guardan en la memoria NVS y se reutilizan en futuros arranques.

## Dependencias

Instalar desde el Gestor de Librerías de Arduino:

- **UniversalTelegramBot**
- **RTClib** (Adafruit)
- **DHT sensor library** (Adafruit)

Incluidas con el core ESP32:

- WiFi, WiFiClientSecure, Wire, Preferences

## Comandos de Telegram

Envía `/start` al bot para ver la ayuda. Todos los valores se guardan en NVS y sobreviven reinicios.

### Uso diario

| Comando | Descripción |
|---------|-------------|
| `/start` | Muestra la ayuda |
| `/estado` | Estado actual del invernadero |
| `/regar [mL]` | Riego manual (máx 1500 mL) |
| `/autoriego [on\|off]` | Activar/desactivar riego automático |
| `/vent [0-100]` | Ventilador en modo manual al % indicado |
| `/ventauto [on\|off]` | Ventilador en modo automático |
| `/reportes [on\|off] [min] [compact\|all]` | Reportes periódicos por Telegram |

### Configuración de la planta

| Comando | Descripción |
|---------|-------------|
| `/config` | Ver toda la configuración |
| `/etapa [pl\|veg\|pre\|flo\|fin]` | Cambiar etapa de crecimiento |
| `/maceta [litros]` | Volumen de la maceta (1-50 L) |
| `/luz [etapa] [horas]` | Horas de luz para plántula o vegetativo (12-20 h) |
| `/pausariego [dias]` | Días mínimos entre riegos automáticos (1-5) |

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
| `/airemax [N]` | Alerta de calidad de aire (sensor MQ) |

### Calibración de la bomba

| Comando | Descripción |
|---------|-------------|
| `/calibrar` | Activa la bomba 5 segundos para medir el volumen |
| `/caudal [mL]` | Ingresa el volumen medido para calcular el caudal |

Pasos: envía `/calibrar`, mide el agua que salió, luego envía `/caudal [mL]`.

### Administración

| Comando | Descripción |
|---------|-------------|
| `/addid [ID]` | Autorizar un chat de Telegram (máx 5) |
| `/delid [ID]` | Eliminar un chat autorizado |
| `/ids` | Ver chats autorizados |

## Etapas de crecimiento

| Código | Etapa | Luz (h) | mL/L |
|--------|-------|---------|------|
| pl | Plántula | 18 (configurable) | 50 |
| veg | Vegetativo | 18 (configurable) | 100 |
| pre | Pre-floración | 12 (fija) | 150 |
| flo | Floración | 12 (fija) | 200 |
| fin | Final | 12 (fija) | 120 |

Las horas de luz solo se pueden modificar en plántula y vegetativo. Los LEDs auxiliares se activan a partir de pre-floración.
