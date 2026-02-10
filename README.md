# ESP32-greenhouse

Mapeo de pines utilizado actualmente (ver `ESP32/pins.h`):

- **PIN_DHT (25):** DHT22 para temperatura y humedad ambiente.
- **PIN_LED_MOSFET (26):** MOSFET para tiras LED auxiliares.
- **PIN_MQ135 (34):** Sensor MQ-135 (entrada analógica).
- **PIN_SUELO (32):** Sensor de humedad de suelo (entrada analógica, ADC1).
- **PIN_I2C_SDA (21):** Línea SDA para RTC (I2C).
- **PIN_I2C_SCL (22):** Línea SCL para RTC (I2C).
- **PIN_SD_CS (5):** Chip Select de la tarjeta SD (SPI remapeado).
- **PIN_FAN_PWM (16):** PWM hacia MOSFET de ventiladores.
- **PIN_RELE1 (17):** MOSFET para bomba de agua DC.
- **PIN_RELE2 (13):** Relé para luz de corriente alterna.
- **PIN_FLOAT (4):** Interruptor de flotador para nivel de agua (entrada digital).

El firmware está ubicado en la carpeta `ESP32/` con el sketch principal `ESP32.ino` listo para abrirse en el Arduino IDE. Consulta `ESP32/pins.h` para las definiciones que se usan en el código.

Al encender el dispositivo, el firmware solicita por el monitor serie el SSID y contraseña de WiFi, además del token de Telegram, antes de intentar la conexión. Tras conectarse a WiFi usa `WiFiClientSecure` junto con `UniversalTelegramBot` para verificar que el token responde correctamente (llamando a `getMe`). Si el token no es válido, se vuelve a pedir por serie hasta recibir uno funcional.

Dependencias Arduino principales (instalables desde el Gestor de Librerías):

- **UniversalTelegramBot**
- **WiFiClientSecure** (incluida con el core de ESP32)

## Comandos de Telegram

El bot expone los siguientes comandos con el formato `/comando [obligatorio] [opcional]`. Los valores configurados se guardan en la NVS para conservarse tras reinicios (incluyendo umbrales de alertas, calibración del sensor de suelo y caudal de la bomba).

**Comandos de uso común**

- `/start` - Muestra el resumen de ayuda con todos los comandos.
- `/estado` - Devuelve el estado actual (temperatura, humedad relativa, suelo, agua, etapa, etc.).
- `/riego_auto [on|off]` - Consulta o cambia el estado del riego automático.
- `/regar [mL]` - Ejecuta un riego manual con el volumen indicado (hasta 1500 mL).
- `/vent_auto [on|off]` - Activa o desactiva el control automático del ventilador.
- `/vent [0-100]` - Ajusta el ventilador en modo manual al porcentaje indicado.
- `/reportes [on|off] [min]` - Activa/desactiva y, opcionalmente, define el intervalo de envíos automáticos de estado en minutos.

**Comandos de configuración**

- `/ajustes` - Muestra la configuración completa de riego (maceta, etapa, umbrales, intervalos, etc.).
- `/maceta [litros]` - Define el volumen de la maceta (1-50 L).
- `/etapa [plantula|vegetativo|pre-floracion|floracion|final]` - Cambia la etapa de cultivo para ajustar riegos y alertas.
- `/horas_luz [plantula|vegetativo] [horas]` - Ajusta las horas de luz para plántula o vegetativo (1-24 h). Las demás etapas quedan fijas en 12 h.
- `/suelo_cal [SECO] [HUMEDO]` - Calibra el sensor de suelo con lecturas ADC para seco y húmedo.
- `/suelo_min [%]` - Ajusta el umbral mínimo de humedad en suelo (0-50%).
- `/suelo_max [%]` - Ajusta el umbral de humedad alta en suelo (50-100%).
- `/pausa_riego [dias]` - Fija el intervalo mínimo entre riegos automáticos (1-5 días).
- `/temp_max [C]` - Configura el umbral de alerta por temperatura alta.
- `/hum_min [%]` - Configura la alerta de humedad relativa baja (activa solo en plántula y vegetativo).
- `/hum_max [%]` - Configura la alerta de humedad relativa alta (activa en pre-floración, floración y final).
- `/aire_max [N]` - Configura el umbral del sensor MQ para alertar aire pobre.
- `/calibrar` - Activa la bomba durante 5 segundos para medir manualmente el volumen entregado.
- `/caudal [mL]` - Guarda el caudal en mL/s usando el volumen medido tras `/calibrar`.

**Administración de IDs**

- `/addid [ID]` - Autoriza un nuevo ID de chat para recibir mensajes (máximo 5).
- `/delid [ID]` - Elimina un ID autorizado.
- `/ids` - Lista los IDs de chat autorizados actualmente.
