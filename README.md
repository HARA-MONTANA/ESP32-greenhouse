# ESP32-greenhousev2

Mapeo de pines propuesto para el proyecto:

- **PIN_DHT (4):** DHT22 para temperatura y humedad ambiente.
- **PIN_ONEWIRE (17):** Bus OneWire para 2 × DS18B20.
- **PIN_RELE1 (26):** Relé 1 (bomba).
- **PIN_RELE2 (27):** Relé 2 (luz).
- **PIN_MQ135 (34):** Sensor MQ-135 (entrada analógica).
- **PIN_SUELO (35):** Sensor de humedad de suelo (entrada analógica).
- **PIN_FLOT (33):** Interruptor de flotador para nivel de agua (entrada digital).
- **PIN_FAN_PWM (25):** PWM hacia MOSFET de ventiladores.
- **PIN_SD_CS (5):** Chip Select de la tarjeta SD (SPI).
- **PIN_I2C_SDA (21):** Línea SDA para RTC (I2C).
- **PIN_I2C_SCL (22):** Línea SCL para RTC (I2C).

El firmware está ubicado en la carpeta `ESP32/` con el sketch principal `ESP32.ino` listo para abrirse en el Arduino IDE. Consulta `ESP32/pins.h` para las definiciones que se usan en el código.

Al encender el dispositivo, el firmware solicita por el monitor serie el SSID y contraseña de WiFi, además del token de Telegram, antes de intentar la conexión. Tras conectarse a WiFi usa `WiFiClientSecure` junto con `UniversalTelegramBot` para verificar que el token responde correctamente (llamando a `getMe`). Si el token no es válido, se vuelve a pedir por serie hasta recibir uno funcional.

Dependencias Arduino principales (instalables desde el Gestor de Librerías):

- **UniversalTelegramBot**
- **WiFiClientSecure** (incluida con el core de ESP32)
