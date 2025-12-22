// Pin assignments for ESP32 greenhouse v2

// OneWire bus for two DS18B20 sensors
// #define PIN_ONEWIRE   16   // 2x DS18B20 en el mismo bus

// --- Sensores lejos de la planta (fila superior) ---
#define PIN_DHT       4

// ADC1 (seguros con WiFi)
#define PIN_MQ135     34   // input-only
#define PIN_SUELO     32   // ADC1

// I2C RTC
#define PIN_I2C_SDA   21
#define PIN_I2C_SCL   22

// --- SD card (SPI remapeado) ---
#define PIN_SD_CS     5
// #define PIN_SD_SCK    14
// #define PIN_SD_MISO   19
// #define PIN_SD_MOSI   23

// --- Cerca de la planta (fila inferior) ---
#define PIN_FAN_PWM   25   // PWM MOSFET (fans)
#define PIN_RELE1     17   // MOSFET bomba DC
#define PIN_RELE2     13   // luz AC (relé)
#define PIN_FLOAT     18   // Float switch
