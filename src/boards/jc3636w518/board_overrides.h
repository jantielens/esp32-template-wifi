#ifndef BOARD_OVERRIDES_H
#define BOARD_OVERRIDES_H

// ============================================================================
// Board Overrides: jc3636w518 (ESP32-S3 + ST77916 QSPI 360x360 + CST816S touch)
// Mirrors the known-good setup from sample/jc3636w518-macropad.
// ============================================================================

// ---------------------------------------------------------------------------
// Capabilities
// ---------------------------------------------------------------------------
#define HAS_DISPLAY true

// LVGL: place built-in CPU/FPS perf monitor at bottom-center (round display)
#define LV_USE_PERF_MONITOR_POS LV_ALIGN_BOTTOM_MID
#define HAS_BACKLIGHT true
#define HAS_TOUCH true

// Enable Image Upload API on this board
#define HAS_IMAGE_API true

// PSRAM board: allow larger full-image uploads than the global default.
// High-entropy JPEGs at higher quality can exceed 100KB.
#define IMAGE_API_MAX_SIZE_BYTES (300 * 1024)

// ---------------------------------------------------------------------------
// Driver Selection (HAL)
// ---------------------------------------------------------------------------
#define DISPLAY_DRIVER DISPLAY_DRIVER_ESP_PANEL
#define TOUCH_DRIVER TOUCH_DRIVER_CST816S_ESP_PANEL

// ---------------------------------------------------------------------------
// Display geometry
// ---------------------------------------------------------------------------
#define DISPLAY_WIDTH 360
#define DISPLAY_HEIGHT 360
#define DISPLAY_ROTATION 0

// Match the sample: prefer PSRAM for LVGL draw buffer (fallback handled in DisplayManager).
#define LVGL_BUFFER_PREFER_INTERNAL false
#define LVGL_BUFFER_SIZE (DISPLAY_WIDTH * 16)  // 16 rows (matches sample default)

// ---------------------------------------------------------------------------
// Backlight (LEDC)
// ---------------------------------------------------------------------------
#define LCD_BL_PIN 15

// ---------------------------------------------------------------------------
// QSPI panel pins (ST77916)
// ---------------------------------------------------------------------------
#define TFT_RST 47
#define TFT_CS 10
#define TFT_SCK 9
#define TFT_SDA0 11
#define TFT_SDA1 12
#define TFT_SDA2 13
#define TFT_SDA3 14

// QSPI clock (matches sample)
#define TFT_SPI_FREQ_HZ (50 * 1000 * 1000)

// ---------------------------------------------------------------------------
// Touch pins (CST816S over I2C)
// ---------------------------------------------------------------------------
#define TOUCH_I2C_SCL 8
#define TOUCH_I2C_SDA 7
#define TOUCH_INT 41
#define TOUCH_RST 40

#endif // BOARD_OVERRIDES_H
