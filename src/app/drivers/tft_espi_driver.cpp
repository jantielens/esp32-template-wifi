#include "tft_espi_driver.h"
#include "../log_manager.h"

TFT_eSPI_Driver::TFT_eSPI_Driver() {
    // TFT_eSPI constructor already called
}

void TFT_eSPI_Driver::init() {
    Logger.logLine("TFT_eSPI: Initializing");
    tft.init();
}

void TFT_eSPI_Driver::setRotation(uint8_t rotation) {
    tft.setRotation(rotation);
}

void TFT_eSPI_Driver::setBacklight(bool on) {
    #ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, on ? TFT_BACKLIGHT_ON : !TFT_BACKLIGHT_ON);
    #endif
}

void TFT_eSPI_Driver::applyDisplayFixes() {
    // Apply display-specific settings (inversion, gamma, etc.)
    #ifdef DISPLAY_INVERSION_ON
    tft.invertDisplay(true);
    Logger.logLine("TFT_eSPI: Inversion ON");
    #endif
    
    #ifdef DISPLAY_INVERSION_OFF
    tft.invertDisplay(false);
    Logger.logLine("TFT_eSPI: Inversion OFF");
    #endif
    
    // Apply gamma fix (both v2 and v3 CYD variants need this)
    #ifdef DISPLAY_NEEDS_GAMMA_FIX
    Logger.logLine("TFT_eSPI: Applying gamma correction fix");
    tft.writecommand(0x26);
    tft.writedata(2);
    delay(120);
    tft.writecommand(0x26);
    tft.writedata(1);
    Logger.logLine("TFT_eSPI: Gamma fix applied");
    #endif
}

void TFT_eSPI_Driver::startWrite() {
    tft.startWrite();
}

void TFT_eSPI_Driver::endWrite() {
    tft.endWrite();
}

void TFT_eSPI_Driver::setAddrWindow(int16_t x, int16_t y, uint16_t w, uint16_t h) {
    tft.setAddrWindow(x, y, w, h);
}

void TFT_eSPI_Driver::pushColors(uint16_t* data, uint32_t len, bool swap_bytes) {
    tft.pushColors(data, len, swap_bytes);
}
