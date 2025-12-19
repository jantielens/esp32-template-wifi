/*
 * Arduino_GFX Display Driver Implementation
 * 
 * Wraps Arduino_GFX library for QSPI displays (AXS15231B).
 * Uses canvas-based rendering like the JC3248W535 sample.
 */

#include "arduino_gfx_driver.h"
#include "../log_manager.h"

Arduino_GFX_Driver::Arduino_GFX_Driver() 
    : bus(nullptr), gfx(nullptr), canvas(nullptr), currentBrightness(100),
      displayWidth(DISPLAY_WIDTH), displayHeight(DISPLAY_HEIGHT), displayRotation(DISPLAY_ROTATION),
      currentX(0), currentY(0), currentW(0), currentH(0) {
}

Arduino_GFX_Driver::~Arduino_GFX_Driver() {
    if (canvas) delete canvas;
    if (gfx) delete gfx;
    if (bus) delete bus;
}

void Arduino_GFX_Driver::init() {
    Logger.logLine("Arduino_GFX: Initializing QSPI display driver");
    
    // Initialize backlight pin first
    #ifdef LCD_BL_PIN
    pinMode(LCD_BL_PIN, OUTPUT);
    digitalWrite(LCD_BL_PIN, HIGH);  // Turn on backlight immediately
    Logger.logLinef("Arduino_GFX: Backlight enabled on GPIO%d", LCD_BL_PIN);
    #endif
    
    // Create QSPI bus (from sample: Arduino_ESP32QSPI)
    #ifdef LCD_QSPI_CS
    bus = new Arduino_ESP32QSPI(
        LCD_QSPI_CS,    // CS
        LCD_QSPI_PCLK,  // SCK
        LCD_QSPI_D0,    // D0
        LCD_QSPI_D1,    // D1
        LCD_QSPI_D2,    // D2
        LCD_QSPI_D3     // D3
    );
    Logger.logLine("Arduino_GFX: QSPI bus created");
    #else
    Logger.logLine("Arduino_GFX: ERROR - QSPI pins not defined in board_config.h");
    return;
    #endif
    
    // Create AXS15231B display (physical panel, portrait orientation)
    // GFX_NOT_DEFINED = -1 for RST (no reset pin on this board)
    // 0 = initial rotation (portrait)
    // false = IPS mode
    gfx = new Arduino_AXS15231B(bus, LCD_QSPI_RST, 0, false, displayWidth, displayHeight);
    Logger.logLine("Arduino_GFX: AXS15231B panel object created");
    
    // Create canvas for buffered rendering
    // Canvas rotation matches DISPLAY_ROTATION for proper LVGL alignment
    canvas = new Arduino_Canvas(displayWidth, displayHeight, gfx, 0, 0, displayRotation);
    Logger.logLinef("Arduino_GFX: Canvas created with rotation=%d", displayRotation);
    
    // Initialize display via canvas (canvas->begin() initializes the underlying display)
    if (!canvas->begin(40000000UL)) {  // 40MHz QSPI frequency
        Logger.logLine("Arduino_GFX: ERROR - Failed to initialize display");
        return;
    }
    Logger.logLine("Arduino_GFX: Display initialized via canvas");
    
    // Clear screen
    canvas->fillScreen(BLACK);
    canvas->flush();
    Logger.logLine("Arduino_GFX: Screen cleared");
    
    Logger.logLinef("Arduino_GFX: Display ready: %dx%d", displayWidth, displayHeight);
}

void Arduino_GFX_Driver::setRotation(uint8_t rotation) {
    displayRotation = rotation;
    if (canvas) {
        canvas->setRotation(rotation);
    }
}

void Arduino_GFX_Driver::setBacklight(bool on) {
    #ifdef LCD_BL_PIN
    digitalWrite(LCD_BL_PIN, on ? HIGH : LOW);
    #endif
}

void Arduino_GFX_Driver::setBacklightBrightness(uint8_t brightness) {
    #ifdef LCD_BL_PIN
    currentBrightness = brightness;
    
    #ifdef TFT_BACKLIGHT_PWM_CHANNEL
    // Use PWM for smooth brightness control
    uint32_t duty = (brightness * 255) / 100;
    ledcWrite(TFT_BACKLIGHT_PWM_CHANNEL, duty);
    #else
    // No PWM - simple on/off
    digitalWrite(LCD_BL_PIN, brightness > 0 ? HIGH : LOW);
    #endif
    #endif
}

uint8_t Arduino_GFX_Driver::getBacklightBrightness() {
    return currentBrightness;
}

bool Arduino_GFX_Driver::hasBacklightControl() {
    #ifdef LCD_BL_PIN
    return true;
    #else
    return false;
    #endif
}

void Arduino_GFX_Driver::applyDisplayFixes() {
    // AXS15231B doesn't need gamma correction or inversion fixes
    // Panel is configured correctly by Arduino_GFX library
}

void Arduino_GFX_Driver::startWrite() {
    // Canvas-based rendering doesn't need startWrite/endWrite
    // All drawing goes to canvas buffer first
}

void Arduino_GFX_Driver::endWrite() {
    // Flush happens in pushColors after all data is written
}

void Arduino_GFX_Driver::setAddrWindow(int16_t x, int16_t y, uint16_t w, uint16_t h) {
    // Store the current drawing area for pushColors
    // Canvas-based rendering uses draw16bitRGBBitmap instead of raw pixel pushing
    currentX = x;
    currentY = y;
    currentW = w;
    currentH = h;
}

void Arduino_GFX_Driver::pushColors(uint16_t* data, uint32_t len, bool swap_bytes) {
    // For canvas-based rendering, draw the bitmap directly
    // This matches the sample's approach: canvas->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)color_p, w, h)
    // Note: canvas accumulates draws; flush happens in LVGL rendering task
    if (canvas) {
        canvas->draw16bitRGBBitmap(currentX, currentY, data, currentW, currentH);
    }
}

DisplayDriver::RenderMode Arduino_GFX_Driver::renderMode() const {
    return RenderMode::Buffered;
}

void Arduino_GFX_Driver::present() {
    // Push canvas buffer to physical display.
    // Called by DisplayManager only when LVGL produced draw data.
    if (canvas) {
        canvas->flush();
    }
}

void Arduino_GFX_Driver::configureLVGL(lv_disp_drv_t* drv, uint8_t rotation) {
    // For Arduino_GFX with canvas, we handle rotation via canvas->setRotation()
    // LVGL works in logical (rotated) coordinates
    // Canvas translates to physical panel coordinates
    
    // Software rotation is already set via canvas->setRotation() in init()
    // No additional LVGL configuration needed - canvas handles it
    
    // Set screen dimensions based on rotation
    if (rotation == 1 || rotation == 3) {
        // Landscape: swap width/height
        drv->hor_res = displayHeight;
        drv->ver_res = displayWidth;
    } else {
        // Portrait: keep original
        drv->hor_res = displayWidth;
        drv->ver_res = displayHeight;
    }
}
