#ifndef BOARD_OVERRIDES_ESP32C3_WAVESHARE_169_ST7789V2_H
#define BOARD_OVERRIDES_ESP32C3_WAVESHARE_169_ST7789V2_H

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
// Display (Waveshare 1.69" ST7789V2) - ESP32-C3 Pin Mapping
// ============================================================================
// The ESP32-C3 does not have the same default SPI pins as the classic ESP32.

#define HAS_DISPLAY true

// ============================================================================
// Driver Selection (HAL)
// ============================================================================
// Display backend: ST7789V2 native SPI driver
// Touch backend:   none
#define DISPLAY_DRIVER DISPLAY_DRIVER_ST7789V2
#define HAS_TOUCH false

// Display dimensions (physical panel is 240x280 in portrait)
#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 280

// Display rotation: 0=portrait(0°), 1=landscape(90°), 2=portrait(180°), 3=landscape(270°)
// Note: Using LVGL software rotation, panel stays in portrait mode
#define DISPLAY_ROTATION 1  // Landscape mode (90° clockwise via LVGL)

// SPI pins (common ESP32-C3 Super Mini header/pinout: SCK=GPIO4, MOSI=GPIO6, CS=GPIO7)
#define LCD_SCK_PIN 4
#define LCD_MOSI_PIN 6
#define LCD_CS_PIN 7
#define LCD_DC_PIN 3
#define LCD_RST_PIN 20
#define LCD_BL_PIN 1

// Backlight control
#define HAS_BACKLIGHT true
#define TFT_BL LCD_BL_PIN
#define TFT_BACKLIGHT_ON HIGH

// ============================================================================
// Image API Configuration
// ============================================================================
#define HAS_IMAGE_API true
// CYD has more RAM than ESP32-C3, can handle larger images
#define IMAGE_API_MAX_SIZE_BYTES (150 * 1024)  // 150KB max for full image upload
#define IMAGE_API_DECODE_HEADROOM_BYTES (50 * 1024)  // 50KB headroom for decoding

// LVGL buffer size (lines to buffer - larger = faster but more RAM)
// 20 lines × 240 pixels × 2 bytes = 9.6KB per buffer (double buffered = 19.2KB total)
#define LVGL_BUFFER_SIZE (DISPLAY_WIDTH * 20)

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

#endif // BOARD_OVERRIDES_ESP32C3_WAVESHARE_169_ST7789V2_H
