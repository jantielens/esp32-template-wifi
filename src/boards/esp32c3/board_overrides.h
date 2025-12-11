#ifndef BOARD_OVERRIDES_ESP32C3_H
#define BOARD_OVERRIDES_ESP32C3_H

// ============================================================================
// ESP32-C3 Super Mini Board Configuration Overrides
// ============================================================================
// This file overrides default settings in src/app/board_config.h
// for the ESP32-C3 Super Mini board.
//
// Only define hardware-specific constants that differ from defaults.
// Use conditional compilation in app.ino for board-specific logic.
//
// Usage Pattern:
//   1. Define board capabilities here (HAS_xxx, PIN numbers, etc.)
//   2. Use #if HAS_xxx in app.ino to conditionally compile board-specific code
//   3. Compiler removes unused code automatically (zero overhead)

// ============================================================================
// Hardware Configuration
// ============================================================================

// Built-in LED on ESP32-C3 Super Mini is on GPIO8 (not GPIO2 like ESP32)
#define HAS_BUILTIN_LED true
#define LED_PIN 8
#define LED_ACTIVE_HIGH true

// ============================================================================
// Example: Additional Board-Specific Hardware
// ============================================================================
// Uncomment and customize as needed for your board:
//
// #define HAS_BUTTON true
// #define BUTTON_PIN 9
// #define BUTTON_ACTIVE_LOW true
//
// #define HAS_BATTERY_MONITOR true
// #define BATTERY_ADC_PIN 4
// #define BATTERY_VOLTAGE_DIVIDER 2.0
//
// #define HAS_DISPLAY true
// #define DISPLAY_DRIVER_ST7789
// #define DISPLAY_WIDTH 240
// #define DISPLAY_HEIGHT 135

#endif // BOARD_OVERRIDES_ESP32C3_H
