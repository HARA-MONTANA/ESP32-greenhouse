// Pin assignments for ESP32 greenhouse v2
// DHT22 (ambient temperature / humidity)
#define PIN_DHT       4
// OneWire bus for two DS18B20 sensors
// #define PIN_ONEWIRE   17

// Relays
#define PIN_RELE1     26  // Relay 1 (pump)
#define PIN_RELE2     27  // Relay 2 (light)

// Analog sensors
#define PIN_MQ135     34  // MQ-135 (analog input)
#define PIN_SUELO     35  // Soil moisture (analog input)
#define PIN_FLOAT     33  // Float switch (water level, digital)

// PWM for fans
#define PIN_FAN_PWM   25  // PWM to MOSFET (fans)

// SD card (SPI)
#define PIN_SD_CS     5   // SD chip select

// I2C for RTC
#define PIN_I2C_SDA   21
#define PIN_I2C_SCL   22
