#ifndef MAIN_SCREEN_H
#define MAIN_SCREEN_H

#include "screen.h"
#include "../config_manager.h"
#include <lvgl.h>

// Forward declaration
class DisplayManager;

// ============================================================================
// Main Screen
// ============================================================================
// Primary status screen showing device info, WiFi, and test pattern.
// Dependencies: DeviceConfig* for WiFi status, DisplayManager* for navigation

class MainScreen : public Screen {
private:
    lv_obj_t* screen;
    DeviceConfig* config;
    DisplayManager* displayMgr;
    
    // Widget references for updates
    lv_obj_t* wifiLabel;
    lv_obj_t* boardLabel;
    
public:
    MainScreen(DeviceConfig* deviceConfig, DisplayManager* manager);
    ~MainScreen();
    
    void create() override;
    void destroy() override;
    void show() override;
    void hide() override;
    void update() override;
};

#endif // MAIN_SCREEN_H
