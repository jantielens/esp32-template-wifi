#ifndef BOARD_OVERRIDES_JC3248W535_H
#define BOARD_OVERRIDES_JC3248W535_H

// ============================================================================
// Guition JC3248W535 Board Configuration Overrides
// ============================================================================
// Manufacturer sample reference: /sample
// - Panel: AXS15231B, QSPI, 320x480
// - Touch: AXS15231B touch, I2C
//
// Notes:
// - The sample uses LVGL rotation (90°) and keeps the panel in portrait.
// - The sample defines a DC pin, but QSPI IO uses dc_gpio_num=-1.

// ============================================================================
// Display Configuration
// ============================================================================
#define HAS_DISPLAY true

// ============================================================================
// Driver Selection (HAL)
// ============================================================================
// Display backend: Arduino_GFX (AXS15231B over QSPI)
// Touch backend:   AXS15231B (I2C)
#define DISPLAY_DRIVER DISPLAY_DRIVER_ARDUINO_GFX
#define TOUCH_DRIVER   TOUCH_DRIVER_AXS15231B

// Physical panel resolution (portrait)
#define DISPLAY_WIDTH  320
#define DISPLAY_HEIGHT 480

// Software rotation via LVGL (1 = 90° landscape)
#define DISPLAY_ROTATION 1

// LVGL buffer size - larger for 320x480 display
// Increase to reduce the number of flush chunks LVGL emits per frame.
// This buffer is allocated from PSRAM in DisplayManager.
#define LVGL_BUFFER_SIZE (DISPLAY_WIDTH * 80)

// QSPI pins (from sample/esp_bsp.h)
#define LCD_QSPI_HOST SPI2_HOST
#define LCD_QSPI_CS   45
#define LCD_QSPI_PCLK 47
#define LCD_QSPI_D0   21
#define LCD_QSPI_D1   48
#define LCD_QSPI_D2   40
#define LCD_QSPI_D3   39
#define LCD_QSPI_RST  -1

// Optional tear effect pin (from sample)
#define LCD_QSPI_TE   38

// Backlight
#define HAS_BACKLIGHT true
#define LCD_BL_PIN 1
#define TFT_BACKLIGHT_PWM_CHANNEL 1

// ============================================================================
// Touch Configuration
// ============================================================================
#define HAS_TOUCH true

// Touch is I2C (from sample/esp_bsp.h)
#define TOUCH_I2C_PORT 0    // I2C_NUM_0
#define TOUCH_I2C_SCL  8
#define TOUCH_I2C_SDA  4
#define TOUCH_I2C_FREQ_HZ 400000

#define TOUCH_RST -1
#define TOUCH_INT -1

// Touch calibration (from sample: dispcfg.h)
#define TOUCH_CAL_X_MIN 12
#define TOUCH_CAL_X_MAX 310
#define TOUCH_CAL_Y_MIN 14
#define TOUCH_CAL_Y_MAX 461

// ============================================================================
// Image API
// ============================================================================
#define HAS_IMAGE_API true

#endif // BOARD_OVERRIDES_JC3248W535_H
