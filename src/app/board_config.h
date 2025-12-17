#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// ============================================================================
// Board Configuration - Two-Phase Include Pattern
// ============================================================================
// This file provides default configuration for all boards using a two-phase
// include pattern:
//
// Phase 1: Load board-specific overrides first (if they exist)
// Phase 2: Define defaults using #ifndef guards (only if not already defined)
//
// To customize for a specific board, create: src/boards/[board-name]/board_overrides.h
// The build system will automatically detect and include board-specific overrides.
//
// Example board-specific override:
//   src/boards/esp32c3/board_overrides.h

// ============================================================================
// Phase 1: Load Board-Specific Overrides
// ============================================================================
// build.sh defines BOARD_HAS_OVERRIDE when a board override directory exists
// and adds that directory to the include path. Board-specific settings are
// loaded first so they can override the defaults below.

#ifdef BOARD_HAS_OVERRIDE
#include "board_overrides.h"
#endif

// ============================================================================
// Project Branding
// ============================================================================
// Project display name for UI elements (passed from build system)
// Default fallback if not provided by build flags
#ifndef PROJECT_DISPLAY_NAME
#define PROJECT_DISPLAY_NAME "ESP32 Device"
#endif

// ============================================================================
// Phase 2: Default Hardware Capabilities
// ============================================================================
// These defaults are only applied if not already defined by board overrides.

// Built-in LED
#ifndef HAS_BUILTIN_LED
#define HAS_BUILTIN_LED false
#endif

// MQTT / Home Assistant integration
#ifndef HAS_MQTT
#define HAS_MQTT true
#endif

#ifndef LED_PIN
#define LED_PIN 2  // Common GPIO for ESP32 boards
#endif

#ifndef LED_ACTIVE_HIGH
#define LED_ACTIVE_HIGH true  // true = HIGH turns LED on, false = LOW turns LED on
#endif

// ============================================================================
// Default WiFi Configuration
// ============================================================================

#ifndef WIFI_MAX_ATTEMPTS
#define WIFI_MAX_ATTEMPTS 3
#endif

// ============================================================================
// Additional Default Configuration Settings
// ============================================================================
// Add new hardware features here using #ifndef guards to allow board-specific
// overrides.
//
// Usage Pattern in Application Code:
//   1. Define capabilities in board_overrides.h: #define HAS_BUTTON true
//   2. Use conditional compilation in app.ino:
//
//      #if HAS_BUTTON
//        pinMode(BUTTON_PIN, INPUT_PULLUP);
//        // Button-specific code only compiled when HAS_BUTTON is true
//      #endif
//
// Examples:
//
// Button:
// #ifndef HAS_BUTTON
// #define HAS_BUTTON false
// #endif
//
// #ifndef BUTTON_PIN
// #define BUTTON_PIN 0
// #endif
//
// Battery Monitor:
// #ifndef HAS_BATTERY_MONITOR
// #define HAS_BATTERY_MONITOR false
// #endif
//
// #ifndef BATTERY_ADC_PIN
// #define BATTERY_ADC_PIN 34
// #endif
//
// Display:
// #ifndef HAS_DISPLAY
// #define HAS_DISPLAY false
// #endif

// ============================================================================
// Display Configuration
// ============================================================================
#ifndef HAS_DISPLAY
#define HAS_DISPLAY false
#endif

// Display driver selection
// Available drivers:
//   DISPLAY_DRIVER_TFT_ESPI (1) - Bodmer's TFT_eSPI (supports ILI9341, ST7789, etc.)
//   DISPLAY_DRIVER_LOVYANGFX (2) - LovyanGFX (future support)
#define DISPLAY_DRIVER_TFT_ESPI 1
#define DISPLAY_DRIVER_LOVYANGFX 2

#ifndef DISPLAY_DRIVER
#define DISPLAY_DRIVER DISPLAY_DRIVER_TFT_ESPI  // Default to TFT_eSPI
#endif

// ============================================================================
// LVGL Configuration
// ============================================================================
#ifndef LVGL_BUFFER_SIZE
#define LVGL_BUFFER_SIZE (DISPLAY_WIDTH * 10)  // 10 lines buffer
#endif

#ifndef LVGL_TICK_PERIOD_MS
#define LVGL_TICK_PERIOD_MS 5
#endif

// ============================================================================
// Touch Configuration
// ============================================================================
#ifndef HAS_TOUCH
#define HAS_TOUCH false
#endif

// Touch driver selection
// Available drivers:
//   TOUCH_DRIVER_XPT2046 (1) - XPT2046 resistive touch (via TFT_eSPI)
//   TOUCH_DRIVER_FT6236 (2) - FT6236 capacitive touch (future support)
#define TOUCH_DRIVER_XPT2046 1
#define TOUCH_DRIVER_FT6236 2

#ifndef TOUCH_DRIVER
#define TOUCH_DRIVER TOUCH_DRIVER_XPT2046  // Default to XPT2046
#endif

#endif // BOARD_CONFIG_H

