#include "board_config.h"

#if HAS_DISPLAY

#include "display_manager.h"
#include "log_manager.h"

// Include selected display driver
#ifndef DISPLAY_DRIVER
#define DISPLAY_DRIVER DISPLAY_DRIVER_TFT_ESPI  // Default to TFT_eSPI
#endif

#if DISPLAY_DRIVER == DISPLAY_DRIVER_TFT_ESPI
#include "drivers/tft_espi_driver.h"
#endif

#include <SPI.h>

// Global instance
DisplayManager* displayManager = nullptr;

DisplayManager::DisplayManager(DeviceConfig* cfg) 
    : driver(nullptr), config(cfg), currentScreen(nullptr), infoScreen(cfg, this), testScreen(this),
      lvglTaskHandle(nullptr), lvglMutex(nullptr), screenCount(0) {
    // Instantiate selected display driver
    #if DISPLAY_DRIVER == DISPLAY_DRIVER_TFT_ESPI
    driver = new TFT_eSPI_Driver();
    #else
    #error "No display driver selected or unknown driver type"
    #endif
    
    // Create mutex for thread-safe LVGL access
    lvglMutex = xSemaphoreCreateMutex();
    
    // Initialize screen registry (exclude splash - it's boot-specific)
    availableScreens[0] = {"info", "Info Screen", &infoScreen};
    availableScreens[1] = {"test", "Test Screen", &testScreen};
    screenCount = 2;
}

DisplayManager::~DisplayManager() {
    // Stop rendering task
    if (lvglTaskHandle) {
        vTaskDelete(lvglTaskHandle);
        lvglTaskHandle = nullptr;
    }
    
    if (currentScreen) {
        currentScreen->hide();
    }
    
    splashScreen.destroy();
    infoScreen.destroy();
    testScreen.destroy();
    
    // Delete display driver
    if (driver) {
        delete driver;
        driver = nullptr;
    }
    
    // Delete mutex
    if (lvglMutex) {
        vSemaphoreDelete(lvglMutex);
        lvglMutex = nullptr;
    }
}

// LVGL flush callback
void DisplayManager::flushCallback(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    DisplayManager* mgr = (DisplayManager*)disp->user_data;
    
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    
    mgr->driver->startWrite();
    mgr->driver->setAddrWindow(area->x1, area->y1, w, h);
    mgr->driver->pushColors((uint16_t *)&color_p->full, w * h, true);
    mgr->driver->endWrite();
    
    lv_disp_flush_ready(disp);
}

void DisplayManager::lock() {
    if (lvglMutex) {
        xSemaphoreTake(lvglMutex, portMAX_DELAY);
    }
}

void DisplayManager::unlock() {
    if (lvglMutex) {
        xSemaphoreGive(lvglMutex);
    }
}

// FreeRTOS task for continuous LVGL rendering
void DisplayManager::lvglTask(void* pvParameter) {
    DisplayManager* mgr = (DisplayManager*)pvParameter;
    
    Logger.logBegin("LVGL Rendering Task");
    Logger.logLinef("Started on core %d", xPortGetCoreID());
    Logger.logEnd();
    
    while (true) {
        mgr->lock();
        
        // Handle LVGL rendering (animations, timers, etc.)
        lv_timer_handler();
        
        // Update current screen (data refresh)
        if (mgr->currentScreen) {
            mgr->currentScreen->update();
        }
        
        mgr->unlock();
        
        // LVGL recommends 5-10ms update interval
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void DisplayManager::initHardware() {
    Logger.logBegin("Display Init");
    
    // Initialize display driver
    driver->init();
    driver->setRotation(DISPLAY_ROTATION);
    
    // Apply saved brightness from config (or default to 100%)
    #if HAS_BACKLIGHT
    uint8_t brightness = config ? config->backlight_brightness : 100;
    if (brightness > 100) brightness = 100;
    driver->setBacklightBrightness(brightness);
    Logger.logLinef("Backlight: %d%%", brightness);
    #else
    // Turn on backlight (on/off only)
    driver->setBacklight(true);
    Logger.logLine("Backlight: ON");
    #endif
    
    Logger.logLinef("Resolution: %dx%d", DISPLAY_WIDTH, DISPLAY_HEIGHT);
    Logger.logLinef("Rotation: %d", DISPLAY_ROTATION);
    
    // Apply display-specific settings (inversion, gamma, etc.)
    driver->applyDisplayFixes();
    
    Logger.logEnd();
}

void DisplayManager::initLVGL() {
    Logger.logBegin("LVGL Init");
    
    lv_init();
    
    // Initialize default theme (dark mode with custom primary color)
    lv_theme_t* theme = lv_theme_default_init(
        NULL,                           // Display (use default)
        lv_color_hex(0x3399FF),        // Primary color (light blue)
        lv_color_hex(0x303030),        // Secondary color (dark gray)
        true,                           // Dark mode
        LV_FONT_DEFAULT                // Default font
    );
    lv_disp_set_theme(NULL, theme);
    Logger.logLine("Theme: Default dark mode initialized");
    
    // Set up display buffer
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, LVGL_BUFFER_SIZE);
    
    // Initialize display driver
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = DISPLAY_WIDTH;
    disp_drv.ver_res = DISPLAY_HEIGHT;
    disp_drv.flush_cb = DisplayManager::flushCallback;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.user_data = this;  // Pass instance for callback
    lv_disp_drv_register(&disp_drv);
    
    Logger.logLinef("Buffer: %d pixels (%d lines)", LVGL_BUFFER_SIZE, LVGL_BUFFER_SIZE / DISPLAY_WIDTH);
    Logger.logEnd();
}

void DisplayManager::init() {
    // Initialize hardware (TFT + gamma fix)
    initHardware();
    
    // Initialize LVGL
    initLVGL();
    
    Logger.logBegin("Display Manager Init");
    
    // Create all screens
    splashScreen.create();
    infoScreen.create();
    testScreen.create();
    
    Logger.logLine("Screens created");
    
    // Show splash immediately
    showSplash();
    
    // Create LVGL rendering task
    // On dual-core: pin to Core 0 (Arduino loop runs on Core 1)
    // On single-core: runs on Core 0 (time-sliced with Arduino loop)
    #if CONFIG_FREERTOS_UNICORE
    xTaskCreate(lvglTask, "LVGL", 4096, this, 1, &lvglTaskHandle);
    Logger.logLine("Rendering task created (single-core)");
    #else
    xTaskCreatePinnedToCore(lvglTask, "LVGL", 4096, this, 1, &lvglTaskHandle, 0);
    Logger.logLine("Rendering task created (pinned to Core 0)");
    #endif
    
    Logger.logEnd();
}

void DisplayManager::showSplash() {
    Logger.logBegin("Show Splash");
    lock();
    if (currentScreen) {
        Logger.logLine("Hiding current screen");
        currentScreen->hide();
    }
    currentScreen = &splashScreen;
    Logger.logLine("Loading splash screen");
    currentScreen->show();
    unlock();
    Logger.logEnd();
}

void DisplayManager::showInfo() {
    Logger.logBegin("Show Info");
    // Note: Don't lock if called from LVGL callback (already locked by task)
    // Just perform the screen change directly
    if (currentScreen) {
        Logger.logLine("Hiding current screen");
        currentScreen->hide();
    }
    currentScreen = &infoScreen;
    Logger.logLine("Loading info screen");
    currentScreen->show();
    Logger.logEnd();
}

void DisplayManager::showTest() {
    Logger.logBegin("Show Test");
    // Note: Don't lock if called from LVGL callback (already locked by task)
    // Just perform the screen change directly
    if (currentScreen) {
        Logger.logLine("Hiding current screen");
        currentScreen->hide();
    }
    currentScreen = &testScreen;
    Logger.logLine("Loading test screen");
    currentScreen->show();
    Logger.logEnd();
}

void DisplayManager::setSplashStatus(const char* text) {
    lock();
    splashScreen.setStatus(text);
    unlock();
}

bool DisplayManager::showScreen(const char* screen_id) {
    if (!screen_id) return false;
    
    // Look up screen in registry
    for (size_t i = 0; i < screenCount; i++) {
        if (strcmp(availableScreens[i].id, screen_id) == 0) {
            Logger.logMessagef("Display", "Switching to screen: %s", screen_id);
            
            // Lock - this method is called from web server task (external to LVGL)
            lock();
            if (currentScreen) {
                currentScreen->hide();
            }
            currentScreen = availableScreens[i].instance;
            currentScreen->show();
            unlock();
            
            return true;
        }
    }
    
    Logger.logMessagef("Display", "Screen not found: %s", screen_id);
    return false;
}

const char* DisplayManager::getCurrentScreenId() {
    // Return ID of current screen (nullptr if splash or unknown)
    for (size_t i = 0; i < screenCount; i++) {
        if (currentScreen == availableScreens[i].instance) {
            return availableScreens[i].id;
        }
    }
    return nullptr;  // Splash or unknown screen
}

const ScreenInfo* DisplayManager::getAvailableScreens(size_t* count) {
    if (count) *count = screenCount;
    return availableScreens;
}

// C-style interface for app.ino
void display_manager_init(DeviceConfig* config) {
    if (!displayManager) {
        displayManager = new DisplayManager(config);
        displayManager->init();
    }
}

void display_manager_show_splash() {
    if (displayManager) {
        displayManager->showSplash();
    }
}

void display_manager_show_info() {
    if (displayManager) {
        displayManager->showInfo();
    }
}

void display_manager_show_test() {
    if (displayManager) {
        displayManager->showTest();
    }
}

void display_manager_set_splash_status(const char* text) {
    if (displayManager) {
        displayManager->setSplashStatus(text);
    }
}

void display_manager_show_screen(const char* screen_id, bool* success) {
    bool result = false;
    if (displayManager) {
        result = displayManager->showScreen(screen_id);
    }
    if (success) *success = result;
}

const char* display_manager_get_current_screen_id() {
    if (displayManager) {
        return displayManager->getCurrentScreenId();
    }
    return nullptr;
}

const ScreenInfo* display_manager_get_available_screens(size_t* count) {
    if (displayManager) {
        return displayManager->getAvailableScreens(count);
    }
    if (count) *count = 0;
    return nullptr;
}

void display_manager_set_backlight_brightness(uint8_t brightness) {
    if (displayManager && displayManager->getDriver()) {
        displayManager->getDriver()->setBacklightBrightness(brightness);
    }
}

#endif // HAS_DISPLAY
