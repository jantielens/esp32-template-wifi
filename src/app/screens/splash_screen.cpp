
#include "screens/splash_screen.h"
#include "log_manager.h"

SplashScreen::SplashScreen() : screen(nullptr), logoLabel(nullptr), statusLabel(nullptr), spinner(nullptr) {}

SplashScreen::~SplashScreen() {
    destroy();
}

void SplashScreen::create() {
    Logger.logBegin("SplashScreen::create");
    if (screen) {
        Logger.logLine("Already created");
        Logger.logEnd();
        return;  // Already created
    }
    
    // Create screen
    screen = lv_obj_create(NULL);
    // Override theme background to pure black
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    
    // Logo/title (centered above middle)
    logoLabel = lv_label_create(screen);
    lv_label_set_text(logoLabel, "ESP32");
    lv_obj_set_style_text_color(logoLabel, lv_color_white(), 0);
    lv_obj_set_style_text_font(logoLabel, &lv_font_montserrat_24, 0);
    lv_obj_align(logoLabel, LV_ALIGN_CENTER, 0, -40);
    
    // Status text (centered at middle)
    statusLabel = lv_label_create(screen);
    lv_label_set_text(statusLabel, "Booting...");
    lv_obj_set_style_text_color(statusLabel, lv_color_make(100, 100, 100), 0);
    lv_obj_align(statusLabel, LV_ALIGN_CENTER, 0, 10);
    
    // Spinner to show activity (centered below middle)
    spinner = lv_spinner_create(screen, 1000, 60);
    lv_obj_set_size(spinner, 40, 40);
    lv_obj_align(spinner, LV_ALIGN_CENTER, 0, 60);
    lv_obj_set_style_arc_color(spinner, lv_color_make(0, 150, 255), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(spinner, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(spinner, lv_color_make(40, 40, 40), LV_PART_MAIN);
    lv_obj_set_style_arc_width(spinner, 4, LV_PART_MAIN);
    
    Logger.logLine("Screen created successfully");
    Logger.logEnd();
}

void SplashScreen::destroy() {
    if (screen) {
        lv_obj_del(screen);
        screen = nullptr;
        logoLabel = nullptr;
        statusLabel = nullptr;
        spinner = nullptr;
    }
}

void SplashScreen::show() {
    Logger.logBegin("SplashScreen::show");
    if (screen) {
        Logger.logLine("Calling lv_scr_load");
        lv_scr_load(screen);
        Logger.logLine("Screen loaded");
    } else {
        Logger.logLine("ERROR: Screen is NULL!");
    }
    Logger.logEnd();
}

void SplashScreen::hide() {
    Logger.logLine("SplashScreen::hide");
    // Nothing to do - LVGL handles screen switching
}

void SplashScreen::update() {
    // Static screen - no updates needed
}

void SplashScreen::setStatus(const char* text) {
    if (statusLabel) {
        Logger.logLinef("SplashScreen::setStatus: %s", text);
        lv_label_set_text(statusLabel, text);
    } else {
        Logger.logLine("ERROR: statusLabel is NULL!");
    }
}
