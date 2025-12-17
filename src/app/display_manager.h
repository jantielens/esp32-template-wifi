#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "config_manager.h"
#include "screens/screen.h"
#include "screens/splash_screen.h"
#include "screens/info_screen.h"
#include "screens/test_screen.h"

#include <TFT_eSPI.h>
#include <lvgl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// ============================================================================
// Display Manager
// ============================================================================
// Manages display hardware, LVGL, screen lifecycle, and navigation.
// Uses FreeRTOS task for continuous LVGL rendering (works on single and dual core).
//
// Usage:
//   display_manager_init(&device_config);  // In setup() - starts rendering task
//   display_manager_show_main();           // When WiFi connected
//   display_manager_set_splash_status();   // Update splash text
//
// Note: No need to call update() in loop() - rendering task handles it

class DisplayManager {
private:
    // Hardware
    TFT_eSPI tft;
    lv_disp_draw_buf_t draw_buf;
    lv_color_t buf[LVGL_BUFFER_SIZE];
    lv_disp_drv_t disp_drv;
    
    // FreeRTOS task and mutex
    TaskHandle_t lvglTaskHandle;
    SemaphoreHandle_t lvglMutex;
    
    // Screen management
    Screen* currentScreen;
    
    // Screen instances (created at init, kept in memory)
    SplashScreen splashScreen;
    InfoScreen infoScreen;
    TestScreen testScreen;
    
    // Hardware initialization
    void initHardware();
    void initLVGL();
    
    // LVGL flush callback (static, accesses instance via user_data)
    static void flushCallback(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
    
    // FreeRTOS task for LVGL rendering
    static void lvglTask(void* pvParameter);
    
public:
    DisplayManager(DeviceConfig* config);
    ~DisplayManager();
    
    // Initialize hardware + LVGL + screens + rendering task (shows splash automatically)
    void init();
    
    // Navigation API (thread-safe)
    void showSplash();
    void showInfo();
    void showTest();
    
    // Splash status update (thread-safe)
    void setSplashStatus(const char* text);
    
    // Mutex helpers for external thread-safe access
    void lock();
    void unlock();
    
    // Access to splash screen for status updates
    SplashScreen* getSplash() { return &splashScreen; }
};

// Global instance (managed by app.ino)
extern DisplayManager* displayManager;

// C-style interface for app.ino
void display_manager_init(DeviceConfig* config);
void display_manager_show_splash();
void display_manager_show_info();
void display_manager_show_test();
void display_manager_set_splash_status(const char* text);

#endif // DISPLAY_MANAGER_H
