#include "board_config.h"

#if HAS_TOUCH

#include "touch_manager.h"
#include "log_manager.h"

// Include selected touch driver header.
// Driver implementations are compiled via src/app/touch_drivers.cpp.
#if TOUCH_DRIVER == TOUCH_DRIVER_XPT2046
#include "drivers/xpt2046_driver.h"
#elif TOUCH_DRIVER == TOUCH_DRIVER_AXS15231B
#include "drivers/axs15231b_touch_driver.h"
#endif

// Global instance
TouchManager* touchManager = nullptr;

TouchManager::TouchManager() 
    : driver(nullptr), indev(nullptr) {
    // Driver will be instantiated in init() after display is ready
}

TouchManager::~TouchManager() {
    if (driver) {
        delete driver;
        driver = nullptr;
    }
}

void TouchManager::readCallback(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    TouchManager* manager = (TouchManager*)drv->user_data;
    
    uint16_t x, y;
    if (manager->driver->getTouch(&x, &y)) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = x;
        data->point.y = y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void TouchManager::init() {
    Logger.logBegin("Touch Manager Init");
    
    // Create standalone touch driver (no dependency on display)
    #if TOUCH_DRIVER == TOUCH_DRIVER_XPT2046
    driver = new XPT2046_Driver(TOUCH_CS, TOUCH_IRQ);
    #elif TOUCH_DRIVER == TOUCH_DRIVER_AXS15231B
    driver = new AXS15231B_TouchDriver();
    #else
    #error "No touch driver selected or unknown driver type"
    #endif
    
    // Initialize hardware
    driver->init();
    
    // Set calibration if defined
    #if defined(TOUCH_CAL_X_MIN) && defined(TOUCH_CAL_X_MAX) && defined(TOUCH_CAL_Y_MIN) && defined(TOUCH_CAL_Y_MAX)
    driver->setCalibration(TOUCH_CAL_X_MIN, TOUCH_CAL_X_MAX, TOUCH_CAL_Y_MIN, TOUCH_CAL_Y_MAX);
    #endif
    
    // Set rotation to match display
    #ifdef DISPLAY_ROTATION
    driver->setRotation(DISPLAY_ROTATION);
    Logger.logLinef("Touch rotation: %d", DISPLAY_ROTATION);
    #endif
    
    // Register with LVGL as input device
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = TouchManager::readCallback;
    indev_drv.user_data = this;
    indev = lv_indev_drv_register(&indev_drv);
    
    Logger.logLine("Touch input device registered with LVGL");
    Logger.logEnd();
}

bool TouchManager::isTouched() {
    return driver->isTouched();
}

bool TouchManager::getTouch(uint16_t* x, uint16_t* y) {
    return driver->getTouch(x, y);
}

// C-style interface for app.ino
void touch_manager_init() {
    if (!touchManager) {
        touchManager = new TouchManager();
    }
    touchManager->init();
}

bool touch_manager_is_touched() {
    if (!touchManager) return false;
    return touchManager->isTouched();
}

#endif // HAS_TOUCH
