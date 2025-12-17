#include "test_screen.h"
#include "log_manager.h"
#include "../board_config.h"

TestScreen::TestScreen(DisplayManager* manager) 
    : screen(nullptr), displayMgr(manager),
      titleLabel(nullptr), redBar(nullptr), greenBar(nullptr), blueBar(nullptr),
      gradientBar(nullptr), yellowBar(nullptr), cyanBar(nullptr), magentaBar(nullptr),
      infoLabel(nullptr) {}

TestScreen::~TestScreen() {
    destroy();
}

void TestScreen::create() {
    if (screen) return;  // Already created
    
    Logger.logBegin("TestScreen::create");
    
    // Create main screen container
    screen = lv_obj_create(NULL);
    // Override theme background to pure black
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    
    // Title
    titleLabel = lv_label_create(screen);
    lv_label_set_text(titleLabel, "Display Test");
    lv_obj_set_style_text_color(titleLabel, lv_color_white(), 0);
    lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_18, 0);
    lv_obj_align(titleLabel, LV_ALIGN_CENTER, 0, -90);
    
    // Red bar
    redBar = lv_obj_create(screen);
    lv_obj_set_size(redBar, lv_pct(100), 12);
    lv_obj_set_style_bg_color(redBar, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_border_width(redBar, 0, 0);
    lv_obj_set_style_pad_all(redBar, 0, 0);
    lv_obj_align(redBar, LV_ALIGN_CENTER, 0, -60);
    
    // Green bar
    greenBar = lv_obj_create(screen);
    lv_obj_set_size(greenBar, lv_pct(100), 12);
    lv_obj_set_style_bg_color(greenBar, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_border_width(greenBar, 0, 0);
    lv_obj_set_style_pad_all(greenBar, 0, 0);
    lv_obj_align(greenBar, LV_ALIGN_CENTER, 0, -45);
    
    // Blue bar
    blueBar = lv_obj_create(screen);
    lv_obj_set_size(blueBar, lv_pct(100), 12);
    lv_obj_set_style_bg_color(blueBar, lv_color_hex(0x0000FF), 0);
    lv_obj_set_style_border_width(blueBar, 0, 0);
    lv_obj_set_style_pad_all(blueBar, 0, 0);
    lv_obj_align(blueBar, LV_ALIGN_CENTER, 0, -30);
    
    // Grayscale gradient bar (centered for maximum width on round displays)
    gradientBar = lv_obj_create(screen);
    lv_obj_set_size(gradientBar, lv_pct(100), 40);
    lv_obj_set_style_bg_color(gradientBar, lv_color_black(), 0);
    lv_obj_set_style_bg_grad_color(gradientBar, lv_color_white(), 0);
    lv_obj_set_style_bg_grad_dir(gradientBar, LV_GRAD_DIR_HOR, 0);  // Horizontal gradient
    lv_obj_set_style_border_width(gradientBar, 0, 0);
    lv_obj_set_style_pad_all(gradientBar, 0, 0);
    lv_obj_align(gradientBar, LV_ALIGN_CENTER, 0, 0);
    
    // Yellow bar (R+G)
    yellowBar = lv_obj_create(screen);
    lv_obj_set_size(yellowBar, lv_pct(100), 12);
    lv_obj_set_style_bg_color(yellowBar, lv_color_hex(0xFFFF00), 0);
    lv_obj_set_style_border_width(yellowBar, 0, 0);
    lv_obj_set_style_pad_all(yellowBar, 0, 0);
    lv_obj_align(yellowBar, LV_ALIGN_CENTER, 0, 30);
    
    // Cyan bar (G+B)
    cyanBar = lv_obj_create(screen);
    lv_obj_set_size(cyanBar, lv_pct(100), 12);
    lv_obj_set_style_bg_color(cyanBar, lv_color_hex(0x00FFFF), 0);
    lv_obj_set_style_border_width(cyanBar, 0, 0);
    lv_obj_set_style_pad_all(cyanBar, 0, 0);
    lv_obj_align(cyanBar, LV_ALIGN_CENTER, 0, 45);
    
    // Magenta bar (R+B)
    magentaBar = lv_obj_create(screen);
    lv_obj_set_size(magentaBar, lv_pct(100), 12);
    lv_obj_set_style_bg_color(magentaBar, lv_color_hex(0xFF00FF), 0);
    lv_obj_set_style_border_width(magentaBar, 0, 0);
    lv_obj_set_style_pad_all(magentaBar, 0, 0);
    lv_obj_align(magentaBar, LV_ALIGN_CENTER, 0, 60);
    
    // Resolution info
    infoLabel = lv_label_create(screen);
    char info_text[32];
    snprintf(info_text, sizeof(info_text), "%dx%d RGB565", DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_label_set_text(infoLabel, info_text);
    lv_obj_set_style_text_color(infoLabel, lv_color_make(150, 150, 150), 0);
    lv_obj_align(infoLabel, LV_ALIGN_CENTER, 0, 85);
    
    Logger.logEnd();
}

void TestScreen::destroy() {
    if (screen) {
        lv_obj_del(screen);
        screen = nullptr;
        titleLabel = nullptr;
        redBar = nullptr;
        greenBar = nullptr;
        blueBar = nullptr;
        gradientBar = nullptr;
        yellowBar = nullptr;
        cyanBar = nullptr;
        magentaBar = nullptr;
        infoLabel = nullptr;
    }
}

void TestScreen::show() {
    Logger.logBegin("TestScreen::show");
    if (screen) {
        Logger.logLine("Calling lv_scr_load");
        lv_scr_load(screen);
        Logger.logLine("Screen loaded");
    } else {
        Logger.logLine("ERROR: Screen is NULL!");
    }
    Logger.logEnd();
}

void TestScreen::hide() {
    Logger.logLine("TestScreen::hide");
    // Nothing to do - LVGL handles screen switching
}

void TestScreen::update() {
    // Static screen - no dynamic updates needed
}
