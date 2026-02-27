/*
 * ST7703 MIPI-DSI Display Driver (Direct ESP-IDF)
 * 
 * Driver for ST7703-based MIPI-DSI displays (e.g., Waveshare ESP32-P4-WIFI6-Touch-LCD-4B).
 * Uses direct ESP-IDF MIPI-DSI APIs instead of Arduino_GFX wrappers.
 * 
 * Why bypass Arduino_GFX for DSI?
 *   Arduino_GFX's Arduino_ESP32DSIPanel does not expose the `disable_lp` flag in
 *   ESP-IDF's esp_lcd_dpi_panel_config_t.  With disable_lp=false (the default),
 *   the DSI host enters Low-Power mode during every blanking interval (HBP, HFP,
 *   VBP, VFP, VSA).  The resulting hundreds of HS↔LP D-PHY transitions per frame
 *   cause visible cyan/idle-color flashes on the ST7703 panel.
 * 
 *   By calling ESP-IDF directly we set `.flags.disable_lp = true`, keeping the
 *   D-PHY in High-Speed mode continuously and eliminating the flashes.
 * 
 * Hardware interface:
 *   MIPI-DSI 2-lane, 480 Mbps/lane — managed entirely by ESP-IDF DSI peripheral.
 *   No SPI bus needed; DSI handles both command and pixel data.
 * 
 * Panel lifecycle:
 *   1. LDO powers MIPI D-PHY
 *   2. esp_lcd_new_dsi_bus()       — DSI bus (2-lane, clock)
 *   3. esp_lcd_new_panel_io_dbi()  — command channel (DBI virtual-channel 0)
 *   4. esp_lcd_new_panel_dpi()     — DPI panel (framebuffer, timing, disable_lp)
 *   5. Vendor init commands via esp_lcd_panel_io_tx_param()
 *   6. esp_lcd_panel_init()        — starts continuous DMA refresh from PSRAM FB
 *   7. esp_lcd_dpi_panel_get_frame_buffer() — LVGL writes pixels here
 * 
 * LVGL v9 flush path:
 *   setAddrWindow → pushColors → esp_lcd_panel_draw_bitmap()
 *   → DMA2D hardware async copy → on_color_trans_done ISR callback
 *   → lv_display_flush_ready() (decouples CPU from pixel copy)
 * 
 * Reference: Waveshare ESP32-P4-WIFI6-Touch-LCD-4B (720×720 IPS MIPI-DSI panel)
 */

#ifndef ST7703_DSI_DRIVER_H
#define ST7703_DSI_DRIVER_H

#include "../display_driver.h"
#include "../board_config.h"

// ESP-IDF MIPI-DSI and LCD panel APIs (ESP32-P4 only)
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_mipi_dsi.h>
#include <esp_lcd_panel_io.h>
#include <esp_ldo_regulator.h>
#include <esp_cache.h>

// Vendor init command descriptor
// Matches the Arduino_GFX lcd_init_cmd_t layout so existing init tables work
// unchanged.  Defined locally to avoid depending on Arduino_GFX for DSI init.
typedef struct {
    int cmd;
    const void* data;
    size_t data_bytes;
    unsigned int delay_ms;
} st7703_init_cmd_t;

class ST7703_DSI_Driver : public DisplayDriver {
private:
    uint16_t* framebuffer;                   // DPI panel PSRAM framebuffer (from ESP-IDF)
    esp_lcd_panel_handle_t panel_handle;      // DPI panel handle
    uint8_t currentBrightness;               // Current brightness level (0-100%)
    uint16_t displayWidth;
    uint16_t displayHeight;
    uint8_t displayRotation;
    
    // Backlight control
    bool backlightOn;
    
    // Current flush area (for pushColors implementation)
    int16_t flushX, flushY;
    uint16_t flushW, flushH;
    
    // LVGL display pointer (needed for DMA2D completion callback)
    lv_display_t* lvglDisplay;
    
public:
    ST7703_DSI_Driver();
    ~ST7703_DSI_Driver() override;
    
    void init() override;
    void setRotation(uint8_t rotation) override;
    int width() override;
    int height() override;
    void setBacklight(bool on) override;
    void setBacklightBrightness(uint8_t brightness) override;  // 0-100%
    uint8_t getBacklightBrightness() override;
    bool hasBacklightControl() override;
    void applyDisplayFixes() override;
    
    void startWrite() override;
    void endWrite() override;
    void setAddrWindow(int16_t x, int16_t y, uint16_t w, uint16_t h) override;
    void pushColors(uint16_t* data, uint32_t len, bool swap_bytes = true) override;
    
    // DMA2D async flush: draw_bitmap returns before copy completes;
    // on_color_trans_done callback signals lv_display_flush_ready().
    void configureLVGL(lv_display_t* disp, uint8_t rotation) override;
    bool asyncFlush() const override;
};

#endif // ST7703_DSI_DRIVER_H
