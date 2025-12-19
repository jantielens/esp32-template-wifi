/*
 * Display Driver Interface
 * 
 * Hardware abstraction layer for display libraries.
 * Allows DisplayManager to work with different display libraries
 * (TFT_eSPI, LovyanGFX, Adafruit_GFX, etc.) through a common interface.
 * 
 * IMPLEMENTATION GUIDE FOR NEW DRIVERS:
 * =====================================
 * 
 * 1. Create driver class implementing this interface:
 *    - drivers/your_driver.h (interface)
 *    - drivers/your_driver.cpp (implementation)
 * 
 * 2. Register in display_manager.cpp:
 *    Arduino build system only compiles .cpp files in sketch root directory,
 *    not subdirectories. Driver .cpp files MUST be included in display_manager.cpp:
 * 
 *      #if DISPLAY_DRIVER == DISPLAY_DRIVER_YOUR_DRIVER
 *      #include "drivers/your_driver.h"
 *      #include "drivers/your_driver.cpp"  // Required for compilation!
 *      #endif
 * 
 * 3. Add driver constant to board_config.h:
 *    #define DISPLAY_DRIVER_YOUR_DRIVER 3  // Next available number
 * 
 * 4. Configure in board override file:
 *    #define DISPLAY_DRIVER DISPLAY_DRIVER_YOUR_DRIVER
 */

#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <Arduino.h>
#include <lvgl.h>

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
    
    // Canvas/buffer flush (called after LVGL rendering to push buffered data to display)
    // Default implementation: no-op (for direct rendering drivers like TFT_eSPI)
    virtual void flush() {
        // Override in canvas-based drivers (e.g., Arduino_GFX)
    }
    
    // LVGL configuration hook (override to customize LVGL driver settings)
    // Called during LVGL initialization to allow driver-specific configuration
    // such as software rotation, full refresh mode, etc.
    // Default implementation: no special configuration needed
    virtual void configureLVGL(lv_disp_drv_t* drv, uint8_t rotation) {
        // Default: hardware handles rotation via setRotation()
        // Override if driver needs software rotation or other LVGL tweaks
    }
};

#endif // DISPLAY_DRIVER_H
