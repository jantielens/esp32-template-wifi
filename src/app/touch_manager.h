/*
 * Touch Manager
 * 
 * Manages touch controller lifecycle and LVGL integration.
 * Follows the same pattern as DisplayManager.
 */

#ifndef TOUCH_MANAGER_H
#define TOUCH_MANAGER_H

#include "board_config.h"

#if HAS_TOUCH

#include <Arduino.h>
#include <lvgl.h>
#include "touch_driver.h"

class TouchManager {
private:
    TouchDriver* driver;
    lv_indev_drv_t indev_drv;
    lv_indev_t* indev;
    
    // LVGL read callback (static, accesses instance via user_data)
    static void readCallback(lv_indev_drv_t* drv, lv_indev_data_t* data);
    
public:
    TouchManager();
    ~TouchManager();
    
    // Initialize touch hardware and register with LVGL
    void init();
    
    // Get touch state (for debugging)
    bool isTouched();
    bool getTouch(uint16_t* x, uint16_t* y);
};

// C-style interface for app.ino
void touch_manager_init();
bool touch_manager_is_touched();

#endif // HAS_TOUCH

#endif // TOUCH_MANAGER_H
