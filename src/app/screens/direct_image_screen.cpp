/*
 * Direct Image Screen Implementation
 * 
 * Blank black LVGL screen for strip-by-strip image display.
 * Images are decoded and written directly to LCD hardware.
 */

#include "board_config.h"

#if HAS_IMAGE_API

#include "direct_image_screen.h"
#include "../display_manager.h"
#include "../log_manager.h"
#include <Arduino.h>

DirectImageScreen::DirectImageScreen(DisplayManager* mgr) 
    : manager(mgr), screen_obj(nullptr), session_active(false), visible(false) {
    // Constructor - member variables initialized in initializer list
}

DirectImageScreen::~DirectImageScreen() {
    destroy();
}

void DirectImageScreen::create() {
    if (screen_obj) return;  // Already created
    
    Logger.logBegin("DirectImageScreen");
    
    // Create screen object with solid black background
    // This blank screen prevents LVGL from rendering while we write directly to LCD
    screen_obj = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_obj, lv_color_hex(0x000000), 0);  // Black background
    lv_obj_set_style_bg_opa(screen_obj, LV_OPA_COVER, 0);  // Fully opaque
    
    // Disable scrollbars
    lv_obj_clear_flag(screen_obj, LV_OBJ_FLAG_SCROLLABLE);
    
    // Set display driver for strip decoder
    if (manager) {
        decoder.setDisplayDriver(manager->getDriver());
    }
    
    Logger.logEnd();
}

void DirectImageScreen::destroy() {
    Logger.logBegin("DirectImageScreen Destroy");
    
    // End any active strip session
    if (session_active) {
        end_strip_session();
    }
    
    // Delete screen
    if (screen_obj) {
        lv_obj_del(screen_obj);
        screen_obj = nullptr;
    }
    
    Logger.logEnd();
}

void DirectImageScreen::update() {
    // Check timeout if visible
    if (visible && is_timeout_expired()) {
        Logger.logMessage("DirectImageScreen", "Timeout expired, returning to previous screen");
        // Return to previous screen (the one before image was shown)
        if (manager) {
            manager->returnToPreviousScreen();
        }
        visible = false;
    }
}

void DirectImageScreen::show() {
    if (!screen_obj) {
        create();
    }
    
    lv_scr_load(screen_obj);
    visible = true;
    
    // Set timeout start time if not already set
    if (display_start_time == 0) {
        display_start_time = millis();
    }
    
    Logger.logMessagef("DirectImageScreen", "Show (timeout: %lums)", display_timeout_ms);
}

void DirectImageScreen::hide() {
    visible = false;
    
    // End any active strip session
    if (session_active) {
        end_strip_session();
    }
    
    // Reset timeout
    display_start_time = 0;
}

void DirectImageScreen::begin_strip_session(int width, int height) {
    Logger.logBegin("Strip Session");
    Logger.logLinef("Image: %dx%d", width, height);
    
    // Use the *visible* LCD dimensions (match LVGL rotation)
    int lcd_width = 240;
    int lcd_height = 320;

    #if defined(DISPLAY_WIDTH) && defined(DISPLAY_HEIGHT)
        #if defined(DISPLAY_ROTATION) && (DISPLAY_ROTATION == 1 || DISPLAY_ROTATION == 3)
            lcd_width = DISPLAY_HEIGHT;
            lcd_height = DISPLAY_WIDTH;
        #else
            lcd_width = DISPLAY_WIDTH;
            lcd_height = DISPLAY_HEIGHT;
        #endif
    #endif
    
    // Initialize decoder
    decoder.begin(width, height, lcd_width, lcd_height);
    session_active = true;
    
    Logger.logEnd();
}

bool DirectImageScreen::decode_strip(const uint8_t* jpeg_data, size_t jpeg_size, int strip_index, bool output_bgr565) {
    if (!session_active) {
        Logger.logMessage("DirectImageScreen", "ERROR: No active strip session");
        return false;
    }
    
    // Decode strip and write directly to LCD
    bool success = decoder.decode_strip(jpeg_data, jpeg_size, strip_index, output_bgr565);
    
    if (!success) {
        Logger.logMessagef("DirectImageScreen", "ERROR: Strip %d decode failed", strip_index);
    }
    
    return success;
}

void DirectImageScreen::end_strip_session() {
    if (!session_active) return;
    
    Logger.logMessage("DirectImageScreen", "End strip session");
    
    decoder.end();
    session_active = false;
}

void DirectImageScreen::set_timeout(unsigned long timeout_ms) {
    display_timeout_ms = timeout_ms;
    Logger.logMessagef("DirectImageScreen", "Timeout set to %lu ms", timeout_ms);
}

void DirectImageScreen::set_start_time(unsigned long start_time) {
    display_start_time = start_time;
    Logger.logMessagef("DirectImageScreen", "Start time set to %lu", start_time);
}

bool DirectImageScreen::is_timeout_expired() {
    // 0 means display forever
    if (display_timeout_ms == 0) {
        return false;
    }
    
    // Check if timeout has elapsed
    unsigned long elapsed = millis() - display_start_time;
    return elapsed >= display_timeout_ms;
}

#endif // HAS_IMAGE_API
