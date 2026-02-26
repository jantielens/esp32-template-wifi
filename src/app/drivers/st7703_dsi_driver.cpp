/*
 * ST7703 MIPI-DSI Display Driver Implementation (Direct ESP-IDF)
 * 
 * Calls ESP-IDF MIPI-DSI APIs directly instead of wrapping Arduino_GFX.
 * This allows setting `disable_lp = true` in the DPI panel config, keeping
 * the D-PHY in High-Speed mode during blanking intervals and eliminating the
 * cyan/idle-color flashes caused by HS↔LP transitions on the ST7703 panel.
 * 
 * Replaces what Arduino_GFX's Arduino_ESP32DSIPanel + Arduino_DSI_Display did:
 *   - DSI bus, DBI IO, DPI panel creation → direct esp_lcd_* calls
 *   - draw16bitRGBBitmap()               → memcpy + esp_cache_msync
 *   - Panel reset                        → manual GPIO toggle
 * 
 * DSI timing parameters (from verified hardware testing on Waveshare P4 board):
 * - DSI: 2-lane, 480 Mbps per lane
 * - DPI clock: 38 MHz
 * - HSYNC: pulse_width=20, back_porch=50, front_porch=50
 * - VSYNC: pulse_width=4, back_porch=20, front_porch=20
 * 
 * Init sequence: Adapted from Waveshare BSP esp_lcd_st7703.c (PoC-validated)
 */

#include "st7703_dsi_driver.h"
#include "../log_manager.h"
#include <string.h>  // memcpy

// MIPI DSI PHY power supply (LDO channel 3 on ESP32-P4)
#define MIPI_DSI_PHY_PWR_LDO_CHAN       3
#define MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV 2500

// ============================================================================
// ST7703 vendor init commands (from Waveshare BSP, validated on hardware)
// ============================================================================
// Format: st7703_init_cmd_t { command, data[], data_len, delay_ms }
static const st7703_init_cmd_t st7703_init_operations[] = {
    {0xB9, (uint8_t[]){0xF1, 0x12, 0x83}, 3, 0},
    {0xB1, (uint8_t[]){0x00, 0x00, 0x00, 0xDA, 0x80}, 5, 0},
    {0xB2, (uint8_t[]){0x3C, 0x12, 0x30}, 3, 0},
    {0xB3, (uint8_t[]){0x10, 0x10, 0x28, 0x28, 0x03, 0xFF, 0x00, 0x00, 0x00, 0x00}, 10, 0},
    {0xB4, (uint8_t[]){0x80}, 1, 0},
    {0xB5, (uint8_t[]){0x0A, 0x0A}, 2, 0},
    {0xB6, (uint8_t[]){0x97, 0x97}, 2, 0},
    {0xB8, (uint8_t[]){0x26, 0x22, 0xF0, 0x13}, 4, 0},
    {0xBA, (uint8_t[]){0x31, 0x81, 0x0F, 0xF9, 0x0E, 0x06, 0x20, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x44, 0x25, 0x00, 0x90, 0x0A, 0x00,
                       0x00, 0x01, 0x4F, 0x01, 0x00, 0x00, 0x37}, 27, 0},
    {0xBC, (uint8_t[]){0x47}, 1, 0},
    {0xBF, (uint8_t[]){0x02, 0x11, 0x00}, 3, 0},
    {0xC0, (uint8_t[]){0x73, 0x73, 0x50, 0x50, 0x00, 0x00, 0x12, 0x70, 0x00}, 9, 0},
    {0xC1, (uint8_t[]){0x25, 0x00, 0x32, 0x32, 0x77, 0xE4, 0xFF, 0xFF, 0xCC, 0xCC,
                       0x77, 0x77}, 12, 0},
    {0xC6, (uint8_t[]){0x82, 0x00, 0xBF, 0xFF, 0x00, 0xFF}, 6, 0},
    {0xC7, (uint8_t[]){0xB8, 0x00, 0x0A, 0x10, 0x01, 0x09}, 6, 0},
    {0xC8, (uint8_t[]){0x10, 0x40, 0x1E, 0x02}, 4, 0},
    {0xCC, (uint8_t[]){0x0B}, 1, 0},
    {0xE0, (uint8_t[]){0x00, 0x0B, 0x10, 0x2C, 0x3D, 0x3F, 0x42, 0x3A, 0x07, 0x0D,
                       0x0F, 0x13, 0x15, 0x13, 0x14, 0x0F, 0x16,
                       0x00, 0x0B, 0x10, 0x2C, 0x3D, 0x3F, 0x42, 0x3A, 0x07, 0x0D,
                       0x0F, 0x13, 0x15, 0x13, 0x14, 0x0F, 0x16}, 34, 0},
    {0xE3, (uint8_t[]){0x07, 0x07, 0x0B, 0x0B, 0x0B, 0x0B, 0x00, 0x00, 0x00, 0x00,
                       0xFF, 0x00, 0xC0, 0x10}, 14, 0},
    {0xE9, (uint8_t[]){0xC8, 0x10, 0x0A, 0x00, 0x00, 0x80, 0x81, 0x12, 0x31, 0x23,
                       0x4F, 0x86, 0xA0, 0x00, 0x47, 0x08, 0x00, 0x00, 0x0C, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x98, 0x02,
                       0x8B, 0xAF, 0x46, 0x02, 0x88, 0x88, 0x88, 0x88, 0x88, 0x98,
                       0x13, 0x8B, 0xAF, 0x57, 0x13, 0x88, 0x88, 0x88, 0x88, 0x88,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00}, 63, 0},
    {0xEA, (uint8_t[]){0x97, 0x0C, 0x09, 0x09, 0x09, 0x78, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x9F, 0x31, 0x8B, 0xA8, 0x31, 0x75, 0x88, 0x88,
                       0x88, 0x88, 0x88, 0x9F, 0x20, 0x8B, 0xA8, 0x20, 0x64, 0x88,
                       0x88, 0x88, 0x88, 0x88, 0x23, 0x00, 0x00, 0x02, 0x71, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x40, 0x80, 0x81, 0x00, 0x00, 0x00,
                       0x00}, 61, 0},
    {0xEF, (uint8_t[]){0xFF, 0xFF, 0x01}, 3, 0},
    {0x11, (uint8_t[]){0x00}, 0, 250},   // Sleep Out + 250 ms delay
    {0x29, (uint8_t[]){0x00}, 0, 50},    // Display On + 50 ms delay
};

// DSI timing parameters come from board_overrides.h → board_config.h defaults.
// No local fallback defines needed — board_config.h provides them.

// ============================================================================
// ST7703_DSI_Driver HAL Implementation
// ============================================================================

ST7703_DSI_Driver::ST7703_DSI_Driver()
    : framebuffer(nullptr), panel_handle(nullptr), currentBrightness(100),
      displayWidth(DISPLAY_WIDTH), displayHeight(DISPLAY_HEIGHT), displayRotation(DISPLAY_ROTATION),
      backlightOn(false), flushX(0), flushY(0), flushW(0), flushH(0) {
}

ST7703_DSI_Driver::~ST7703_DSI_Driver() {
    // Display is never torn down during normal operation.
    // If needed: esp_lcd_panel_del(panel_handle) + bus/LDO cleanup.
}

void ST7703_DSI_Driver::init() {
    // ----------------------------------------------------------------
    // Attach backlight PWM BEFORE the LCD panel starts.
    // Same approach as ST7701 RGB driver — configuring the LEDC PWM
    // before DSI init avoids GPIO glitches during panel scan start.
    // ----------------------------------------------------------------
    #ifdef LCD_BL_PIN
    #if HAS_BACKLIGHT
    // PWM brightness control
    #if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttachChannel(LCD_BL_PIN, TFT_BACKLIGHT_PWM_FREQ, 8, TFT_BACKLIGHT_PWM_CHANNEL);
    ledcWrite(LCD_BL_PIN, 0);  // Start OFF
    LOGI("ST7703", "Backlight PWM: GPIO%d, %dHz, 8-bit, ch%d (OFF)",
         LCD_BL_PIN, TFT_BACKLIGHT_PWM_FREQ, TFT_BACKLIGHT_PWM_CHANNEL);
    #else
    ledcSetup(TFT_BACKLIGHT_PWM_CHANNEL, TFT_BACKLIGHT_PWM_FREQ, 8);
    ledcAttachPin(LCD_BL_PIN, TFT_BACKLIGHT_PWM_CHANNEL);
    ledcWrite(TFT_BACKLIGHT_PWM_CHANNEL, 0);  // Start OFF
    LOGI("ST7703", "Backlight PWM: GPIO%d, %dHz, 8-bit, ch%d (OFF)",
         LCD_BL_PIN, TFT_BACKLIGHT_PWM_FREQ, TFT_BACKLIGHT_PWM_CHANNEL);
    #endif
    #else
    // Simple on/off — no PWM
    pinMode(LCD_BL_PIN, OUTPUT);
    #ifdef TFT_BACKLIGHT_ON
    digitalWrite(LCD_BL_PIN, !TFT_BACKLIGHT_ON);  // Start OFF
    #else
    digitalWrite(LCD_BL_PIN, LOW);  // Start OFF
    #endif
    LOGI("ST7703", "Backlight digital: GPIO%d (OFF)", LCD_BL_PIN);
    #endif
    #endif
    
    // ----------------------------------------------------------------
    // Panel hardware reset (previously done inside Arduino_DSI_Display::begin)
    // ----------------------------------------------------------------
    #ifdef LCD_RST_PIN
    pinMode(LCD_RST_PIN, OUTPUT);
    digitalWrite(LCD_RST_PIN, HIGH);
    delay(5);
    digitalWrite(LCD_RST_PIN, LOW);
    delay(10);
    digitalWrite(LCD_RST_PIN, HIGH);
    delay(120);
    LOGI("ST7703", "Panel reset: GPIO%d (HIGH-LOW-HIGH, 120ms)", LCD_RST_PIN);
    #endif
    
    // ----------------------------------------------------------------
    // Direct ESP-IDF MIPI-DSI initialization
    // Replaces Arduino_GFX Arduino_ESP32DSIPanel + Arduino_DSI_Display
    // ----------------------------------------------------------------
    
    LOGI("ST7703", "Initializing MIPI-DSI via direct ESP-IDF calls");
    LOGI("ST7703", "DSI timing: DPI_CLK=%ldHz, lane_rate=%d Mbps",
         (long)ST7703_DPI_CLK_HZ, ST7703_LANE_BIT_RATE);
    LOGI("ST7703", "DSI HSYNC: pw=%d, bp=%d, fp=%d",
         ST7703_HSYNC_PULSE_WIDTH, ST7703_HSYNC_BACK_PORCH, ST7703_HSYNC_FRONT_PORCH);
    LOGI("ST7703", "DSI VSYNC: pw=%d, bp=%d, fp=%d",
         ST7703_VSYNC_PULSE_WIDTH, ST7703_VSYNC_BACK_PORCH, ST7703_VSYNC_FRONT_PORCH);
    
    // 1. Power on MIPI DSI PHY via internal LDO (LDO_VO3 → VDD_MIPI_DPHY)
    esp_ldo_channel_handle_t ldo_mipi_phy = NULL;
    esp_ldo_channel_config_t ldo_config = {
        .chan_id = MIPI_DSI_PHY_PWR_LDO_CHAN,
        .voltage_mv = MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
    };
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_config, &ldo_mipi_phy));
    LOGI("ST7703", "MIPI DSI PHY powered on (LDO ch%d, %dmV)",
         MIPI_DSI_PHY_PWR_LDO_CHAN, MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV);
    
    // 2. Create DSI bus (2-lane, configured bit rate)
    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = 2,
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = ST7703_LANE_BIT_RATE,
    };
    esp_lcd_dsi_bus_handle_t mipi_dsi_bus = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));
    
    // 3. Create DBI IO for DSI command channel (vendor init commands)
    esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    esp_lcd_panel_io_handle_t io_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &io_handle));
    
    // 4. Create DPI panel — THE KEY FIX: disable_lp = true
    //    With disable_lp=false (Arduino_GFX default), the DSI host enters LP mode
    //    during every blanking interval, causing hundreds of HS↔LP D-PHY transitions
    //    per frame.  The ST7703 panel flashes cyan during these transitions.
    //    Setting disable_lp=true keeps D-PHY in HS mode continuously.
    esp_lcd_dpi_panel_config_t dpi_config = {
        .virtual_channel = 0,
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = (uint32_t)(ST7703_DPI_CLK_HZ / 1000000),
        .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565,
        .num_fbs = 1,
        .video_timing = {
            .h_size = displayWidth,
            .v_size = displayHeight,
            .hsync_pulse_width = ST7703_HSYNC_PULSE_WIDTH,
            .hsync_back_porch = ST7703_HSYNC_BACK_PORCH,
            .hsync_front_porch = ST7703_HSYNC_FRONT_PORCH,
            .vsync_pulse_width = ST7703_VSYNC_PULSE_WIDTH,
            .vsync_back_porch = ST7703_VSYNC_BACK_PORCH,
            .vsync_front_porch = ST7703_VSYNC_FRONT_PORCH,
        },
        .flags = {
            .use_dma2d = true,
            .disable_lp = true,   // Keep D-PHY in HS mode during blanking
        },
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_dpi(mipi_dsi_bus, &dpi_config, &panel_handle));
    
    // 5. Send ST7703 vendor init commands via DSI command mode
    const size_t num_cmds = sizeof(st7703_init_operations) / sizeof(st7703_init_operations[0]);
    for (size_t i = 0; i < num_cmds; i++) {
        ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(
            io_handle,
            st7703_init_operations[i].cmd,
            st7703_init_operations[i].data,
            st7703_init_operations[i].data_bytes
        ));
        if (st7703_init_operations[i].delay_ms > 0) {
            delay(st7703_init_operations[i].delay_ms);
        }
    }
    LOGI("ST7703", "Sent %d vendor init commands", (int)num_cmds);
    
    // 6. Initialize DPI panel — starts continuous DMA refresh from PSRAM
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    
    // 7. Get PSRAM framebuffer pointer (allocated by ESP-IDF during panel creation)
    void* fb = nullptr;
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(panel_handle, 1, &fb));
    framebuffer = (uint16_t*)fb;
    
    LOGI("ST7703", "DSI initialized: %dx%d, disable_lp=true, single FB @ %p",
         displayWidth, displayHeight, framebuffer);
    
    delay(50);  // Brief delay before enabling backlight
    setBacklightBrightness(currentBrightness);
    
    LOGI("ST7703", "Display ready: %dx%d @ rotation %d", 
         width(), height(), displayRotation);
}

void ST7703_DSI_Driver::setRotation(uint8_t rotation) {
    displayRotation = rotation;
}

int ST7703_DSI_Driver::width() {
    if (displayRotation == 1 || displayRotation == 3) {
        return displayHeight;
    }
    return displayWidth;
}

int ST7703_DSI_Driver::height() {
    if (displayRotation == 1 || displayRotation == 3) {
        return displayWidth;
    }
    return displayHeight;
}

void ST7703_DSI_Driver::setBacklight(bool on) {
    setBacklightBrightness(on ? 100 : 0);
}

void ST7703_DSI_Driver::setBacklightBrightness(uint8_t brightness) {
    if (brightness > 100) brightness = 100;
    currentBrightness = brightness;
    backlightOn = (brightness > 0);

    #ifdef LCD_BL_PIN
    #if HAS_BACKLIGHT
    // Remap brightness to usable hardware duty range.
    // MOSFET dimming range varies by board/frequency — configured via
    // TFT_BACKLIGHT_DUTY_MIN/MAX in board_overrides.h.
    // Map: 0% → 0 (off), 1-99% → DUTY_MIN..DUTY_MAX (visible dimming),
    //       100% → 255 (constant DC, maximum brightness)
    uint32_t duty = 0;
    if (brightness >= 100) {
        duty = 255;
    } else if (brightness > 0) {
        duty = TFT_BACKLIGHT_DUTY_MIN + ((uint32_t)(brightness - 1) * (TFT_BACKLIGHT_DUTY_MAX - TFT_BACKLIGHT_DUTY_MIN)) / 98;
    }
    #ifdef TFT_BACKLIGHT_ON
    if (!TFT_BACKLIGHT_ON) duty = 255 - duty;  // Invert for active-low
    #endif
    #if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(LCD_BL_PIN, duty);
    #else
    ledcWrite(TFT_BACKLIGHT_PWM_CHANNEL, duty);
    #endif
    #else
    // Digital on/off fallback
    #ifdef TFT_BACKLIGHT_ON
    digitalWrite(LCD_BL_PIN, backlightOn ? TFT_BACKLIGHT_ON : !TFT_BACKLIGHT_ON);
    #else
    digitalWrite(LCD_BL_PIN, backlightOn ? HIGH : LOW);
    #endif
    #endif
    #endif
}

uint8_t ST7703_DSI_Driver::getBacklightBrightness() {
    return currentBrightness;
}

bool ST7703_DSI_Driver::hasBacklightControl() {
    #ifdef LCD_BL_PIN
    return true;
    #else
    return false;
    #endif
}

void ST7703_DSI_Driver::applyDisplayFixes() {
    // No specific fixes needed for ST7703 DSI
}

void ST7703_DSI_Driver::startWrite() {
    // Not needed — draw16bitRGBBitmap handles everything
}

void ST7703_DSI_Driver::endWrite() {
    // Not needed — draw16bitRGBBitmap handles everything
}

void ST7703_DSI_Driver::setAddrWindow(int16_t x, int16_t y, uint16_t w, uint16_t h) {
    // Save the area for pushColors()
    flushX = x;
    flushY = y;
    flushW = w;
    flushH = h;
}

void ST7703_DSI_Driver::pushColors(uint16_t* data, uint32_t len, bool swap_bytes) {
    // Direct memcpy to PSRAM framebuffer + cache writeback.
    // Replaces Arduino_GFX draw16bitRGBBitmap() with identical logic:
    //   1. Copy pixels from LVGL buffer → PSRAM framebuffer (row by row)
    //   2. esp_cache_msync() to flush dirty cache lines so DPI DMA sees new pixels
    if (!framebuffer || !data || flushW == 0 || flushH == 0) {
        return;
    }

    // Row-by-row copy to framebuffer (rotation 0, no offset)
    uint16_t* dst = framebuffer + ((int32_t)flushY * displayWidth) + flushX;
    uint16_t* src = data;
    for (uint16_t row = 0; row < flushH; row++) {
        memcpy(dst, src, flushW * sizeof(uint16_t));
        dst += displayWidth;
        src += flushW;
    }

    // Writeback dirty cache lines covering the affected framebuffer region.
    // Range: from first pixel of first row to last pixel of last row.
    uint16_t* cacheStart = framebuffer + ((int32_t)flushY * displayWidth) + flushX;
    size_t cacheSize = ((size_t)displayWidth * (flushH - 1) + flushW) * sizeof(uint16_t);
    esp_cache_msync(cacheStart, cacheSize,
                    ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
}
