#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "config_manager.h"
#include "display_driver.h"
#include "screens/screen.h"
#include "screens/splash_screen.h"
#include "screens/info_screen.h"
#include "screens/test_screen.h"

#include <lvgl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// ============================================================================
// Screen Registry
// ============================================================================
// Maximum number of screens that can be registered for runtime navigation
// Generous headroom (8 slots) allows adding new screens without recompiling
// Only ~192 bytes total (24 bytes × 8), negligible overhead vs heap allocation
#define MAX_SCREENS 8

// Struct for registering available screens dynamically
struct ScreenInfo {
    const char* id;            // Unique identifier (e.g., "info", "test")
    const char* display_name;  // Human-readable name (e.g., "Info Screen")
    Screen* instance;          // Pointer to screen instance
};

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
    // Hardware (display driver abstraction)
    DisplayDriver* driver;
    lv_disp_draw_buf_t draw_buf;
    lv_color_t buf[LVGL_BUFFER_SIZE];
    lv_disp_drv_t disp_drv;
    
    // Configuration reference
    DeviceConfig* config;
    
    // FreeRTOS task and mutex
    TaskHandle_t lvglTaskHandle;
    SemaphoreHandle_t lvglMutex;
    
    // Screen management
    Screen* currentScreen;
    
    // Screen instances (created at init, kept in memory)
    SplashScreen splashScreen;
    InfoScreen infoScreen;
    TestScreen testScreen;
    
    // Screen registry for runtime navigation (static allocation, no heap)
    // screenCount tracks how many slots are actually used (currently 2: info, test)
    // Splash excluded from runtime selection (boot-specific only)
    ScreenInfo availableScreens[MAX_SCREENS];
    size_t screenCount;
    
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
    
    // Screen selection by ID (thread-safe, returns true if found)
    bool showScreen(const char* screen_id);
    
    // Get current screen ID (returns nullptr if splash or no screen)
    const char* getCurrentScreenId();
    
    // Get available screens for runtime navigation
    const ScreenInfo* getAvailableScreens(size_t* count);
    
    // Splash status update (thread-safe)
    void setSplashStatus(const char* text);
    
    // Mutex helpers for external thread-safe access
    void lock();
    void unlock();
    
    // Access to splash screen for status updates
    SplashScreen* getSplash() { return &splashScreen; }
    
    // Access to display driver (for touch integration)
    DisplayDriver* getDriver() { return driver; }
};

// Global instance (managed by app.ino)
extern DisplayManager* displayManager;

// C-style interface for app.ino
void display_manager_init(DeviceConfig* config);
void display_manager_show_splash();
void display_manager_show_info();
void display_manager_show_test();
void display_manager_show_screen(const char* screen_id, bool* success);  // success is optional output
const char* display_manager_get_current_screen_id();
const ScreenInfo* display_manager_get_available_screens(size_t* count);
void display_manager_set_splash_status(const char* text);
void display_manager_set_backlight_brightness(uint8_t brightness);  // 0-100%

#endif // DISPLAY_MANAGER_H
