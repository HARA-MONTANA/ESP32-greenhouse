# ESP32-greenhousev2

Mapeo de pines propuesto para el proyecto:

- **PIN_DHT (4):** DHT22 para temperatura y humedad ambiente.
- **PIN_ONEWIRE (17):** Bus OneWire para 2 × DS18B20.
- **PIN_RELE1 (26):** Relé 1 (bomba).
- **PIN_RELE2 (27):** Relé 2 (luz).
- **PIN_MQ135 (34):** Sensor MQ-135 (entrada analógica).
- **PIN_SUELO (35):** Sensor de humedad de suelo (entrada analógica).
- **PIN_FLOAT (33):** Interruptor de flotador para nivel de agua (entrada digital).
- **PIN_FAN_PWM (25):** PWM hacia MOSFET de ventiladores.
- **PIN_SD_CS (5):** Chip Select de la tarjeta SD (SPI).
- **PIN_I2C_SDA (21):** Línea SDA para RTC (I2C).
- **PIN_I2C_SCL (22):** Línea SCL para RTC (I2C).

El firmware está ubicado en la carpeta `ESP32/` con el sketch principal `ESP32.ino` listo para abrirse en el Arduino IDE. Consulta `ESP32/pins.h` para las definiciones que se usan en el código.

Al encender el dispositivo, el firmware solicita por el monitor serie el SSID y contraseña de WiFi, además del token de Telegram, antes de intentar la conexión. Tras conectarse a WiFi usa `WiFiClientSecure` junto con `UniversalTelegramBot` para verificar que el token responde correctamente (llamando a `getMe`). Si el token no es válido, se vuelve a pedir por serie hasta recibir uno funcional.

Dependencias Arduino principales (instalables desde el Gestor de Librerías):

- **UniversalTelegramBot**
- **WiFiClientSecure** (incluida con el core de ESP32)

## Comandos de Telegram

El bot expone los siguientes comandos con el formato `/comando [obligatorio] {opcional}`. Los valores configurados se guardan en la
NVS para conservarse tras reinicios (incluyendo los umbrales de alertas y la calibración seco/húmedo del sensor de suelo).

- `/start` - Muestra el resumen de ayuda con todos los comandos.
- `/status` - Devuelve el estado actual (temperatura, humedad relativa, suelo, agua, etapa, etc.).
- `/maceta [litros]` - Define el volumen de la maceta (1-50 L).
- `/etapa [plantula|vegetativo|pre-floracion|floracion|final]` - Cambia la etapa de cultivo para ajustar riegos y alertas.
- `/cal_suelo [SECO] [HUMEDO]` - Calibra el sensor de suelo con lecturas ADC para seco y húmedo.
- `/alerta_suelo [porcentaje]` - Ajusta el umbral de humedad alta en suelo (50-100%).
- `/umbral_suelo [porcentaje]` - Ajusta el umbral mínimo de humedad en suelo (0-50%).
- `/intervalo_riego [dias]` - Fija el intervalo mínimo entre riegos automáticos (1-5 días).
- `/alerta_temp_alta [C]` - Configura el umbral de alerta por temperatura alta.
- `/alerta_rh_baja [%]` - Configura la alerta de humedad relativa baja (activa solo en plántula y vegetativo).
- `/alerta_rh_alta [%]` - Configura la alerta de humedad relativa alta (activa en pre-floración, floración y final).
- `/alerta_mq [ADC]` - Configura el umbral del sensor MQ para alertar aire pobre.
- `/mostrar_conf_riego` - Muestra la configuración completa de riego.
- `/calibrar {mL}` - Activa la bomba 5 s y, si se envía un valor, establece el caudal en mL/s según el volumen medido.
- `/riego_auto [on|off]` - Consulta o cambia el estado del riego automático.
- `/regar [mL]` - Ejecuta un riego manual con el volumen indicado.
- `/fanauto [on|off]` - Activa o desactiva el control automático del ventilador.
- `/fan [0-100]` - Ajusta el ventilador en modo manual al porcentaje indicado.
- `/autolecturas [on|off] {min}` - Activa/desactiva y ajusta el intervalo de envíos automáticos de estado.
- `/addid [ID]` - Autoriza un nuevo ID de chat para recibir mensajes.
- `/delid [ID]` - Elimina un ID autorizado.
- `/ids` - Lista los IDs de chat autorizados actualmente.
