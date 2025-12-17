#include "main_screen.h"
#include "../../version.h"
#include "log_manager.h"
#include <WiFi.h>
#include <esp_chip_info.h>

MainScreen::MainScreen(DeviceConfig* deviceConfig, DisplayManager* manager) 
    : screen(nullptr), config(deviceConfig), displayMgr(manager), wifiLabel(nullptr), boardLabel(nullptr) {}

MainScreen::~MainScreen() {
    destroy();
}

void MainScreen::create() {
    if (screen) return;  // Already created
    
    // Create main screen container
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    
    // Title banner with navy background
    lv_obj_t *title_banner = lv_obj_create(screen);
    lv_obj_set_size(title_banner, DISPLAY_WIDTH, 40);
    lv_obj_set_pos(title_banner, 0, 0);
    lv_obj_set_style_bg_color(title_banner, lv_color_make(0, 0, 128), 0);
    lv_obj_set_style_border_width(title_banner, 0, 0);
    lv_obj_set_style_pad_all(title_banner, 0, 0);
    
    lv_obj_t *title = lv_label_create(title_banner);
    lv_label_set_text(title, "ESP32 Display Test");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
    
    // Hello World label
    lv_obj_t *hello = lv_label_create(screen);
    lv_label_set_text(hello, "Hello World!");
    lv_obj_set_style_text_color(hello, lv_color_make(0, 255, 0), 0);
    lv_obj_set_style_text_font(hello, &lv_font_montserrat_24, 0);
    lv_obj_set_pos(hello, 20, 55);
    
    // Device info
    char info_text[128];
    snprintf(info_text, sizeof(info_text), "Firmware: v%s\nChip: %s Rev %d", 
             FIRMWARE_VERSION, ESP.getChipModel(), ESP.getChipRevision());
    lv_obj_t *info = lv_label_create(screen);
    lv_label_set_text(info, info_text);
    lv_obj_set_style_text_color(info, lv_color_make(0, 255, 255), 0);
    lv_obj_set_pos(info, 20, 95);
    
    // Color test bars (RGB)
    int barHeight = 20;
    int yStart = 135;
    
    lv_obj_t *red_bar = lv_obj_create(screen);
    lv_obj_set_size(red_bar, DISPLAY_WIDTH/3, barHeight);
    lv_obj_set_pos(red_bar, 0, yStart);
    lv_obj_set_style_bg_color(red_bar, lv_color_make(255, 0, 0), 0);
    lv_obj_set_style_border_width(red_bar, 0, 0);
    
    lv_obj_t *green_bar = lv_obj_create(screen);
    lv_obj_set_size(green_bar, DISPLAY_WIDTH/3, barHeight);
    lv_obj_set_pos(green_bar, DISPLAY_WIDTH/3, yStart);
    lv_obj_set_style_bg_color(green_bar, lv_color_make(0, 255, 0), 0);
    lv_obj_set_style_border_width(green_bar, 0, 0);
    
    lv_obj_t *blue_bar = lv_obj_create(screen);
    lv_obj_set_size(blue_bar, DISPLAY_WIDTH/3, barHeight);
    lv_obj_set_pos(blue_bar, (DISPLAY_WIDTH/3)*2, yStart);
    lv_obj_set_style_bg_color(blue_bar, lv_color_make(0, 0, 255), 0);
    lv_obj_set_style_border_width(blue_bar, 0, 0);
    
    // Gradient label
    lv_obj_t *grad_label = lv_label_create(screen);
    lv_label_set_text(grad_label, "Grayscale Gradient (256 levels):");
    lv_obj_set_style_text_color(grad_label, lv_color_white(), 0);
    lv_obj_set_pos(grad_label, 10, yStart + barHeight + 8);
    
    // Grayscale gradient using individual rectangles (memory efficient)
    int gradientY = yStart + barHeight + 25;
    int gradientHeight = 30;
    
    // Draw gradient in 32 steps to reduce memory usage
    int numSteps = 32;
    int stepWidth = DISPLAY_WIDTH / numSteps;
    for (int i = 0; i < numSteps; i++) {
        uint8_t gray = map(i, 0, numSteps - 1, 0, 255);
        lv_obj_t *bar = lv_obj_create(screen);
        lv_obj_set_size(bar, stepWidth + 1, gradientHeight);  // +1 to avoid gaps
        lv_obj_set_pos(bar, i * stepWidth, gradientY);
        lv_obj_set_style_bg_color(bar, lv_color_make(gray, gray, gray), 0);
        lv_obj_set_style_border_width(bar, 0, 0);
        lv_obj_set_style_pad_all(bar, 0, 0);
    }
    
    // Border around gradient
    lv_obj_t *grad_border = lv_obj_create(screen);
    lv_obj_set_size(grad_border, DISPLAY_WIDTH, gradientHeight + 2);
    lv_obj_set_pos(grad_border, 0, gradientY - 1);
    lv_obj_set_style_bg_opa(grad_border, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(grad_border, lv_color_white(), 0);
    lv_obj_set_style_border_width(grad_border, 1, 0);
    
    // Board variant footer (will be updated in update())
    boardLabel = lv_label_create(screen);
    lv_obj_set_style_text_color(boardLabel, lv_color_make(255, 255, 0), 0);
    lv_obj_set_pos(boardLabel, 10, DISPLAY_HEIGHT - 15);
    
    // WiFi status (will be updated in update())
    wifiLabel = lv_label_create(screen);
    lv_obj_set_style_text_color(wifiLabel, lv_color_make(255, 255, 255), 0);
    lv_obj_set_pos(wifiLabel, 10, DISPLAY_HEIGHT - 30);
}

void MainScreen::destroy() {
    if (screen) {
        lv_obj_del(screen);
        screen = nullptr;
        wifiLabel = nullptr;
        boardLabel = nullptr;
    }
}

void MainScreen::show() {
    Logger.logBegin("MainScreen::show");
    if (screen) {
        Logger.logLine("Calling lv_scr_load");
        lv_scr_load(screen);
        Logger.logLine("Screen loaded");
    } else {
        Logger.logLine("ERROR: Screen is NULL!");
    }
    Logger.logEnd();
}

void MainScreen::hide() {
    Logger.logLine("MainScreen::hide");
    // Nothing to do - LVGL handles screen switching
}

void MainScreen::update() {
    if (!screen) return;
    
    // Update WiFi status
    if (wifiLabel) {
        if (WiFi.status() == WL_CONNECTED) {
            char wifi_text[64];
            snprintf(wifi_text, sizeof(wifi_text), "WiFi: %s", WiFi.localIP().toString().c_str());
            lv_label_set_text(wifiLabel, wifi_text);
        } else {
            lv_label_set_text(wifiLabel, "WiFi: Disconnected");
        }
    }
    
    // Update board variant info
    if (boardLabel) {
        #if defined(BOARD_CYD2USB_V2)
        lv_label_set_text(boardLabel, "Board: CYD v2 (1 USB)");
        #elif defined(BOARD_CYD2USB_V3)
        lv_label_set_text(boardLabel, "Board: CYD v3 (2 USB)");
        #else
        lv_label_set_text(boardLabel, "Board: ESP32");
        #endif
    }
}
