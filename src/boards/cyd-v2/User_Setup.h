// Board-specific TFT_eSPI setup for ESP32-2432S028R v2 (CYD - 1 USB Port)
// This file is force-included for cyd-v2 builds via build.sh so CI/web builds
// do not depend on a developer's global TFT_eSPI/User_Setup.h.

#ifndef CYD_V2_TFT_ESPI_USER_SETUP_H
#define CYD_V2_TFT_ESPI_USER_SETUP_H

// IMPORTANT: Mark setup as provided (prevents TFT_eSPI from including its default).
#define USER_SETUP_LOADED

// Display driver selection
#define ILI9341_2_DRIVER

// Display inversion is required for this panel variant
#define TFT_INVERSION_ON

// Color order
#define TFT_RGB_ORDER TFT_BGR

// Display pins (HSPI defaults)
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  -1
#define TFT_BL   21

// Use HSPI for the display
#define USE_HSPI_PORT

// SPI Frequencies
#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000

// Optional: Fonts (keep default feature set reasonably complete)
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SMOOTH_FONT

#endif // CYD_V2_TFT_ESPI_USER_SETUP_H
