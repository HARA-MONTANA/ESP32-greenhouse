// Pin assignments for ESP32 greenhouse v2

// --- Sensores ---
#define PIN_DHT       25
#define PIN_LED_MORADO 26   // MOSFET — LED morado
#define PIN_CRED_SKIP 27    // Botón para usar credenciales almacenadas (activo en LOW)
#define PIN_MQ135     34   // ADC1 input-only
#define PIN_SUELO     32   // ADC1

// --- I2C RTC ---
#define PIN_I2C_SDA   21
#define PIN_I2C_SCL   22

// --- SD card (SPI remapeado) ---
#define PIN_SD_CS     5
// #define PIN_SD_SCK    14
// #define PIN_SD_MISO   19
// #define PIN_SD_MOSI   23

// --- Actuadores ---
#define PIN_FAN_PWM   16   // MOSFET (fans)
#define PIN_RELE1     17   // MOSFET bomba DC
#define PIN_RELE2     13   // Luz AC (relé)
#define PIN_FLOAT     33   // Float switch
