#pragma once

// ==========================================================================
// Board Overrides: esp32c3-withsensors
// ==========================================================================

// Enable MQTT (required for HA discovery in this sample)
#define HAS_MQTT true

// Enable BME280 sensor sample
#define HAS_SENSOR_BME280 true

// Enable LD2410 OUT pin presence sample
#define HAS_SENSOR_LD2410_OUT true

// Sensor I2C pins (ESP32-C3 Super Mini defaults)
// Set to -1 to use Wire defaults if needed.
#define SENSOR_I2C_SDA 8
#define SENSOR_I2C_SCL 9

// LD2410 OUT pin (presence)
#define LD2410_OUT_PIN 4

// Optional: BME280 address (0x76 or 0x77)
// #define BME280_I2C_ADDR 0x76
