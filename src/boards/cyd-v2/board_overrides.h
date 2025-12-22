#ifndef BOARD_OVERRIDES_CYD2USB_V2_H
#define BOARD_OVERRIDES_CYD2USB_V2_H

// ============================================================================
// ESP32-2432S028R v2 (CYD - 1 USB Port) Board Configuration Overrides
// ============================================================================
// This file overrides default settings in src/app/board_config.h for the
// ESP32-2432S028R v2 "Cheap Yellow Display" with single USB port.
//
// Hardware: ESP32 + 2.8" ILI9341 TFT (320x240) + XPT2046 Touch
// Display: ILI9341 driver with TFT_INVERSION_ON + gamma correction
//
// Reference: https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display

// ============================================================================
// Display Configuration
// ============================================================================
#define HAS_DISPLAY true

// ============================================================================
// Driver Selection (HAL)
// ============================================================================
// Display backend: TFT_eSPI (ILI9341 over SPI)
// Touch backend:   XPT2046 (SPI)
#define DISPLAY_DRIVER DISPLAY_DRIVER_TFT_ESPI
#define TOUCH_DRIVER   TOUCH_DRIVER_XPT2046

// ============================================================================
// Display Controller Config (TFT_eSPI)
// ============================================================================
// Must match library's User_Setup.h or use build flags
// For v2 (1 USB port): ILI9341_2_DRIVER with TFT_INVERSION_ON
#define DISPLAY_DRIVER_ILI9341_2
#define DISPLAY_INVERSION_ON true

// Gamma Correction Fix Required for v2
// Both 1-USB and 2-USB variants need gamma correction
// See: https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/blob/main/cyd.md
#define DISPLAY_NEEDS_GAMMA_FIX true

// Display Pins (HSPI)
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  -1   // No reset pin
#define TFT_BL   21   // Backlight

// Display Properties
#define DISPLAY_WIDTH  320
#define DISPLAY_HEIGHT 240
#define DISPLAY_ROTATION 1  // Landscape (0=portrait, 1=landscape, 2=portrait_flip, 3=landscape_flip)

// SPI Frequency
#define TFT_SPI_FREQUENCY 55000000  // 55MHz

// Color Order
#define DISPLAY_COLOR_ORDER_BGR true  // BGR color order (not RGB)

// Backlight Control
#define HAS_BACKLIGHT true
#define TFT_BACKLIGHT_ON HIGH
#define TFT_BACKLIGHT_PWM_CHANNEL 0  // LEDC channel for PWM

// TFT_eSPI Touch Controller Pins (required for TFT_eSPI touch extensions)
#define TOUCH_CS 33     // Touch chip select
#define TOUCH_SCLK 25   // Touch SPI clock
#define TOUCH_MISO 39   // Touch SPI MISO
#define TOUCH_MOSI 32   // Touch SPI MOSI
#define TOUCH_IRQ 36    // Touch interrupt (optional)

// ============================================================================
// Touch Screen Configuration (XPT2046)
// ============================================================================
// Touch uses separate VSPI bus
#define HAS_TOUCH true

// XPT2046 pins (VSPI bus - separate from display)
#define XPT2046_IRQ  36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK  25
#define XPT2046_CS   33

// Calibration values (from macsbug.wordpress.com)
#define TOUCH_CAL_X_MIN 300
#define TOUCH_CAL_X_MAX 3900
#define TOUCH_CAL_Y_MIN 200
#define TOUCH_CAL_Y_MAX 3700

// ============================================================================
// Additional Hardware on CYD
// ============================================================================
// RGB LED (active low)
// #define HAS_RGB_LED true
// #define RGB_LED_RED   4
// #define RGB_LED_GREEN 16
// #define RGB_LED_BLUE  17

// SD Card (VSPI)
// #define HAS_SD_CARD true
// #define SD_CS   5
// #define SD_MISO 19
// #define SD_MOSI 23
// #define SD_SCLK 18

// Light Sensor
// #define HAS_LDR true
// #define LDR_PIN 34

// ============================================================================
// Image API Configuration
// ============================================================================
#define HAS_IMAGE_API true
// Compromise cap: accepts worst-case 320x240 JPEGs while reducing allocation pressure
#define IMAGE_API_MAX_SIZE_BYTES (80 * 1024)  // 80KB max for full image upload
#define IMAGE_API_DECODE_HEADROOM_BYTES (50 * 1024)  // 50KB headroom for decoding

#endif // BOARD_OVERRIDES_CYD2USB_V2_H
