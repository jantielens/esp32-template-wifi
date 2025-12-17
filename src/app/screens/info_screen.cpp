#include "info_screen.h"
#include "../../version.h"
#include "log_manager.h"
#include "../device_telemetry.h"
#include "../board_config.h"
#include <WiFi.h>
#include <esp_chip_info.h>

InfoScreen::InfoScreen(DeviceConfig* deviceConfig, DisplayManager* manager) 
    : screen(nullptr), config(deviceConfig), displayMgr(manager),
      deviceNameLabel(nullptr), mdnsLabel(nullptr), ssidLabel(nullptr), ipLabel(nullptr),
      versionLabel(nullptr), uptimeLabel(nullptr), heapLabel(nullptr), chipLabel(nullptr) {}

InfoScreen::~InfoScreen() {
    destroy();
}

void InfoScreen::create() {
    if (screen) return;  // Already created
    
    // Create main screen container
    screen = lv_obj_create(NULL);
    // Override theme background to pure black
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    
    // All text centered for round display compatibility
    // 7 items ordered with project name as hero (largest font) in center
    // Spacing: 20px between items
    
    // Uptime (shortest - top)
    uptimeLabel = lv_label_create(screen);
    lv_obj_set_style_text_color(uptimeLabel, lv_color_make(200, 200, 200), 0);
    lv_obj_align(uptimeLabel, LV_ALIGN_CENTER, 0, -60);
    
    // Free heap
    heapLabel = lv_label_create(screen);
    lv_obj_set_style_text_color(heapLabel, lv_color_make(200, 200, 200), 0);
    lv_obj_align(heapLabel, LV_ALIGN_CENTER, 0, -40);
    
    // Firmware version
    versionLabel = lv_label_create(screen);
    lv_label_set_text(versionLabel, "v" FIRMWARE_VERSION);
    lv_obj_set_style_text_color(versionLabel, lv_color_make(200, 200, 200), 0);
    lv_obj_align(versionLabel, LV_ALIGN_CENTER, 0, -25);
    
    // Device name (HERO - center of screen, larger font)
    deviceNameLabel = lv_label_create(screen);
    lv_label_set_text(deviceNameLabel, "...");  // Will be set in update()
    lv_obj_set_style_text_color(deviceNameLabel, lv_color_white(), 0);
    lv_obj_set_style_text_font(deviceNameLabel, &lv_font_montserrat_18, 0);
    lv_obj_align(deviceNameLabel, LV_ALIGN_CENTER, 0, 0);
    
    // Separator lines above and below device name (full width)
    separatorTop = lv_obj_create(screen);
    lv_obj_set_size(separatorTop, lv_pct(100), 1);  // Full width, 1px tall
    lv_obj_set_style_bg_color(separatorTop, lv_color_make(80, 80, 80), 0);
    lv_obj_set_style_border_width(separatorTop, 0, 0);
    lv_obj_set_style_pad_all(separatorTop, 0, 0);
    lv_obj_align(separatorTop, LV_ALIGN_CENTER, 0, -15);  // 15px above device name
    
    separatorBottom = lv_obj_create(screen);
    lv_obj_set_size(separatorBottom, lv_pct(100), 1);  // Full width, 1px tall
    lv_obj_set_style_bg_color(separatorBottom, lv_color_make(80, 80, 80), 0);
    lv_obj_set_style_border_width(separatorBottom, 0, 0);
    lv_obj_set_style_pad_all(separatorBottom, 0, 0);
    lv_obj_align(separatorBottom, LV_ALIGN_CENTER, 0, 15);  // 15px below device name
    
    // Chip info (below center)
    chipLabel = lv_label_create(screen);
    char chip_text[64];
    snprintf(chip_text, sizeof(chip_text), "%s Rev %d", ESP.getChipModel(), ESP.getChipRevision());
    lv_label_set_text(chipLabel, chip_text);
    lv_obj_set_style_text_color(chipLabel, lv_color_make(150, 150, 150), 0);
    lv_obj_align(chipLabel, LV_ALIGN_CENTER, 0, 25);
    
    // mDNS hostname
    mdnsLabel = lv_label_create(screen);
    lv_obj_set_style_text_color(mdnsLabel, lv_color_make(150, 150, 150), 0);
    lv_obj_align(mdnsLabel, LV_ALIGN_CENTER, 0, 40);
    
    // IP Address (bottom)
    ipLabel = lv_label_create(screen);
    lv_obj_set_style_text_color(ipLabel, lv_color_make(100, 200, 255), 0);
    lv_obj_align(ipLabel, LV_ALIGN_CENTER, 0, 60);
    
    // Removed label
    ssidLabel = nullptr;
}

void InfoScreen::destroy() {
    if (screen) {
        lv_obj_del(screen);
        screen = nullptr;
        deviceNameLabel = nullptr;
        mdnsLabel = nullptr;
        ssidLabel = nullptr;
        ipLabel = nullptr;
        versionLabel = nullptr;
        uptimeLabel = nullptr;
        heapLabel = nullptr;
        chipLabel = nullptr;
        separatorTop = nullptr;
        separatorBottom = nullptr;
    }
}

void InfoScreen::show() {
    Logger.logBegin("InfoScreen::show");
    if (screen) {
        Logger.logLine("Calling lv_scr_load");
        lv_scr_load(screen);
        Logger.logLine("Screen loaded");
    } else {
        Logger.logLine("ERROR: Screen is NULL!");
    }
    Logger.logEnd();
}

void InfoScreen::hide() {
    Logger.logLine("InfoScreen::hide");
    // Nothing to do - LVGL handles screen switching
}

void InfoScreen::update() {
    if (!screen) return;
    
    // Update device name (from config)
    if (deviceNameLabel) {
        if (strlen(config->device_name) > 0) {
            lv_label_set_text(deviceNameLabel, config->device_name);
        } else {
            lv_label_set_text(deviceNameLabel, "ESP32 Device");
        }
    }
    
    // Update uptime (formatted)
    if (uptimeLabel) {
        unsigned long uptime_sec = millis() / 1000;
        char uptime_text[32];
        if (uptime_sec < 60) {
            snprintf(uptime_text, sizeof(uptime_text), "%lus", uptime_sec);
        } else if (uptime_sec < 3600) {
            snprintf(uptime_text, sizeof(uptime_text), "%lum %lus", uptime_sec / 60, uptime_sec % 60);
        } else {
            unsigned long hours = uptime_sec / 3600;
            unsigned long mins = (uptime_sec % 3600) / 60;
            snprintf(uptime_text, sizeof(uptime_text), "%luh %lum", hours, mins);
        }
        lv_label_set_text(uptimeLabel, uptime_text);
    }
    
    // Update free heap
    if (heapLabel) {
        char heap_text[32];
        unsigned long heap_kb = ESP.getFreeHeap() / 1024;
        snprintf(heap_text, sizeof(heap_text), "%lu KB free", heap_kb);
        lv_label_set_text(heapLabel, heap_text);
    }
    
    // Update IP address
    if (ipLabel) {
        if (WiFi.status() == WL_CONNECTED) {
            lv_label_set_text(ipLabel, WiFi.localIP().toString().c_str());
        } else if (WiFi.getMode() == WIFI_AP) {
            lv_label_set_text(ipLabel, WiFi.softAPIP().toString().c_str());
        } else {
            lv_label_set_text(ipLabel, "No IP");
        }
    }
    
    // Update mDNS hostname
    if (mdnsLabel) {
        char sanitized[CONFIG_DEVICE_NAME_MAX_LEN];
        config_manager_sanitize_device_name(config->device_name, sanitized, sizeof(sanitized));
        char mdns_text[CONFIG_DEVICE_NAME_MAX_LEN + 10];
        snprintf(mdns_text, sizeof(mdns_text), "%s.local", sanitized);
        lv_label_set_text(mdnsLabel, mdns_text);
    }
}
