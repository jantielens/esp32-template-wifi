#include "web_portal_device_api.h"

#include "web_portal_auth.h"
#include "web_portal_state.h"

#include "board_config.h"
#include "device_telemetry.h"
#include "github_release_config.h"
#include "log_manager.h"
#include "psram_json_allocator.h"
#include "project_branding.h"
#include "web_portal_json.h"
#include "../version.h"

#include <ArduinoJson.h>
#include <WiFi.h>

#if HAS_DISPLAY
#include "display_manager.h"
#endif

// GET /api/mode - Return portal mode (core vs full)
void handleGetMode(AsyncWebServerRequest *request) {
    if (!portal_auth_gate(request)) return;

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    response->print("{\"mode\":\"");
    response->print(web_portal_is_ap_mode_active() ? "core" : "full");
    response->print("\",\"ap_active\":");
    response->print(web_portal_is_ap_mode_active() ? "true" : "false");
    response->print("}");
    request->send(response);
}

// GET /api/info - Get device information
void handleGetVersion(AsyncWebServerRequest *request) {
    if (!portal_auth_gate(request)) return;

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    response->print("{\"version\":\"");
    response->print(FIRMWARE_VERSION);
    response->print("\",\"build_date\":\"");
    response->print(BUILD_DATE);
    response->print("\",\"build_time\":\"");
    response->print(BUILD_TIME);
    response->print("\",\"chip_model\":\"");
    response->print(ESP.getChipModel());
    response->print("\",\"chip_revision\":");
    response->print(ESP.getChipRevision());
    response->print(",\"chip_cores\":");
    response->print(ESP.getChipCores());
    response->print(",\"cpu_freq\":");
    response->print(ESP.getCpuFreqMHz());
    response->print(",\"flash_chip_size\":");
    response->print(ESP.getFlashChipSize());
    response->print(",\"psram_size\":");
    response->print(ESP.getPsramSize());
    response->print(",\"free_heap\":");
    response->print(ESP.getFreeHeap());
    response->print(",\"sketch_size\":");
    response->print(device_telemetry_sketch_size());
    response->print(",\"free_sketch_space\":");
    response->print(device_telemetry_free_sketch_space());
    response->print(",\"mac_address\":\"");
    response->print(WiFi.macAddress());
    response->print("\",\"wifi_hostname\":\"");
    response->print(WiFi.getHostname());
    response->print("\",\"mdns_name\":\"");
    response->print(WiFi.getHostname());
    response->print(".local\",\"hostname\":\"");
    response->print(WiFi.getHostname());
    response->print("\",\"project_name\":\"");
    response->print(PROJECT_NAME);
    response->print("\",\"project_display_name\":\"");
    response->print(PROJECT_DISPLAY_NAME);

    // Build metadata for GitHub-based updates
    response->print("\",\"board_name\":\"");
    #ifdef BUILD_BOARD_NAME
        response->print(BUILD_BOARD_NAME);
    #else
        response->print("unknown");
    #endif

    // Health widget client tuning (sparklines + polling cadence)
    response->print("\",\"health_poll_interval_ms\":");
    response->print((unsigned long)HEALTH_POLL_INTERVAL_MS);
    response->print(",\"health_history_seconds\":");
    response->print((unsigned long)HEALTH_HISTORY_SECONDS);

    response->print("\",\"github_updates_enabled\":");
    response->print(GITHUB_UPDATES_ENABLED ? "true" : "false");
    #if GITHUB_UPDATES_ENABLED
        response->print(",\"github_owner\":\"");
        response->print(GITHUB_OWNER);
        response->print("\",\"github_repo\":\"");
        response->print(GITHUB_REPO);
        response->print("\"");
    #endif

    response->print(",\"has_mqtt\":");
    response->print(HAS_MQTT ? "true" : "false");
    response->print(",\"has_backlight\":");
    response->print(HAS_BACKLIGHT ? "true" : "false");

    #if HAS_DISPLAY
        // Display screen information
        response->print(",\"has_display\":true");

        // Display resolution (driver coordinate space for direct writes / image upload)
        int display_coord_width = DISPLAY_WIDTH;
        int display_coord_height = DISPLAY_HEIGHT;
        if (displayManager && displayManager->getDriver()) {
            display_coord_width = displayManager->getDriver()->width();
            display_coord_height = displayManager->getDriver()->height();
        }
        response->print(",\"display_coord_width\":");
        response->print(display_coord_width);
        response->print(",\"display_coord_height\":");
        response->print(display_coord_height);

        // Get available screens
        size_t screen_count = 0;
        const ScreenInfo* screens = display_manager_get_available_screens(&screen_count);

        response->print(",\"available_screens\":[");
        for (size_t i = 0; i < screen_count; i++) {
            if (i > 0) response->print(",");
            response->print("{\"id\":\"");
            response->print(screens[i].id);
            response->print("\",\"name\":\"");
            response->print(screens[i].display_name);
            response->print("\"}");
        }
        response->print("]");

        // Get current screen
        const char* current_screen = display_manager_get_current_screen_id();
        response->print(",\"current_screen\":");
        if (current_screen) {
            response->print("\"");
            response->print(current_screen);
            response->print("\"");
        } else {
            response->print("null");
        }
    #else
        response->print(",\"has_display\":false");
    #endif

    response->print("}");
    request->send(response);
}

// GET /api/health - Get device health statistics
void handleGetHealth(AsyncWebServerRequest *request) {
    if (!portal_auth_gate(request)) return;

    std::shared_ptr<BasicJsonDocument<PsramJsonAllocator>> doc = make_psram_json_doc(2048);
    if (doc && doc->capacity() > 0) {
        device_telemetry_fill_api(*doc);
        if (doc->overflowed()) {
            Logger.logMessage("Portal", "ERROR: /api/health JSON overflow");
        }
    }

    web_portal_send_json_chunked(request, doc);
}

// POST /api/reboot - Reboot device without saving
void handleReboot(AsyncWebServerRequest *request) {
    if (!portal_auth_gate(request)) return;

    Logger.logMessage("API", "POST /api/reboot");
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Rebooting device...\"}");

    // Schedule reboot after response is sent
    delay(100);
    Logger.logMessage("Portal", "Rebooting");
    ESP.restart();
}
