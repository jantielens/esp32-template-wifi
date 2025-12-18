/*
 * Display Driver Interface
 * 
 * Hardware abstraction layer for display libraries.
 * Allows DisplayManager to work with different display libraries
 * (TFT_eSPI, LovyanGFX, Adafruit_GFX, etc.) through a common interface.
 * 
 * To add a new display library:
 * 1. Create a new driver class implementing this interface
 * 2. Add driver selection in board_config.h
 * 3. Include the driver header in display_manager.cpp
 */

#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <Arduino.h>

// ============================================================================
// Display Driver Interface
// ============================================================================
// Pure virtual interface for display hardware abstraction.
// Minimal set of methods required for LVGL integration.

class DisplayDriver {
public:
    virtual ~DisplayDriver() = default;
    
    // Hardware initialization
    virtual void init() = 0;
    
    // Display configuration
    virtual void setRotation(uint8_t rotation) = 0;
    virtual void setBacklight(bool on) = 0;
    
    // Backlight brightness control (0-100%)
    virtual void setBacklightBrightness(uint8_t brightness) = 0;  // 0-100
    virtual uint8_t getBacklightBrightness() = 0;
    virtual bool hasBacklightControl() = 0;  // Capability query
    
    // Display-specific fixes/configuration (optional, board-dependent)
    virtual void applyDisplayFixes() = 0;
    
    // LVGL flush interface (critical path - called frequently)
    virtual void startWrite() = 0;
    virtual void endWrite() = 0;
    virtual void setAddrWindow(int16_t x, int16_t y, uint16_t w, uint16_t h) = 0;
    virtual void pushColors(uint16_t* data, uint32_t len, bool swap_bytes = true) = 0;
};

#endif // DISPLAY_DRIVER_H
