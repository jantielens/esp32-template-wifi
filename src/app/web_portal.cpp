/*
 * Web Configuration Portal Implementation
 * 
 * Async web server with captive portal support.
 * Serves static files and provides REST API for configuration.
 */

// AsyncTCP task stack sizing:
// - The AsyncTCP library is compiled as a separate translation unit.
// - Defining CONFIG_ASYNC_TCP_STACK_SIZE in this file does NOT reliably affect the library build.
// - To override it, define CONFIG_ASYNC_TCP_STACK_SIZE in src/boards/<board>/board_overrides.h.
//   The build script propagates this allowlisted define into library builds.

#include "web_portal.h"
#include "config_manager.h"
#include "log_manager.h"
#include "board_config.h"
#include "device_telemetry.h"
#include "github_release_config.h"
#include "project_branding.h"
#include "../version.h"
#include "psram_json_allocator.h"
#include "web_portal_routes.h"
#include "web_portal_auth.h"
#include "web_portal_config.h"

#if HAS_DISPLAY
#include "display_manager.h"
#include "screen_saver_manager.h"
#endif

#if HAS_IMAGE_API
#include "image_api.h"
#endif

#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <esp_heap_caps.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static void log_async_tcp_stack_watermark_once() {
    static bool logged = false;
    if (logged) return;
    logged = true;

    // AsyncTCP task name varies by library/core version; try a few common ones.
    TaskHandle_t task = xTaskGetHandle("async_tcp");
    if (!task) task = xTaskGetHandle("async_tcp_task");
    if (!task) task = xTaskGetHandle("AsyncTCP");
    if (!task) return;

    const UBaseType_t high_water_words = uxTaskGetStackHighWaterMark(task);
    const unsigned high_water_bytes = (unsigned)high_water_words * (unsigned)sizeof(StackType_t);

    #ifdef CONFIG_ASYNC_TCP_STACK_SIZE
        Logger.logMessagef("Portal", "AsyncTCP stack watermark: %u bytes (CONFIG_ASYNC_TCP_STACK_SIZE=%u)", high_water_bytes, (unsigned)CONFIG_ASYNC_TCP_STACK_SIZE);
    #else
        Logger.logMessagef("Portal", "AsyncTCP stack watermark: %u bytes (CONFIG_ASYNC_TCP_STACK_SIZE not set)", high_water_bytes);
    #endif
}


// Forward declarations
void handleRoot(AsyncWebServerRequest *request);
void handleGetMode(AsyncWebServerRequest *request);
void handleGetHealth(AsyncWebServerRequest *request);
void handleReboot(AsyncWebServerRequest *request);
void handleOTAUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);

void handleGetFirmwareLatest(AsyncWebServerRequest *request);
void handlePostFirmwareUpdate(AsyncWebServerRequest *request);
void handleGetFirmwareUpdateStatus(AsyncWebServerRequest *request);

// Web server on port 80 (pointer to avoid constructor issues)
AsyncWebServer *server = nullptr;

// DNS server for captive portal (port 53)
DNSServer dnsServer;

// AP configuration
#define DNS_PORT 53
#define CAPTIVE_PORTAL_IP IPAddress(192, 168, 4, 1)

// State
static bool ap_mode_active = false;
static DeviceConfig *current_config = nullptr;
static bool ota_in_progress = false;
static size_t ota_progress = 0;
static size_t ota_total = 0;

bool web_portal_is_ap_mode_active() {
    return ap_mode_active;
}

DeviceConfig* web_portal_get_current_config() {
    return current_config;
}

// OTA upload state gate (avoid concurrent uploads).
static portMUX_TYPE g_ota_upload_mux = portMUX_INITIALIZER_UNLOCKED;
static uint8_t g_ota_last_percent = 0;

// ===== Basic Auth (optional; STA/full mode only) =====
// (Basic auth gate moved to web_portal_auth.cpp)

// ===== GitHub Releases firmware update (app-only) =====
static TaskHandle_t firmware_update_task_handle = nullptr;
static volatile bool firmware_update_in_progress = false;
static volatile size_t firmware_update_progress = 0;
static volatile size_t firmware_update_total = 0;

static char firmware_update_state[16] = "idle"; // idle|downloading|writing|rebooting|error
static char firmware_update_error[192] = "";
static char firmware_update_latest_version[24] = "";
static char firmware_update_download_url[512] = "";

static bool parse_semver_triplet(const char *s, int *major, int *minor, int *patch) {
    if (!s || !major || !minor || !patch) return false;

    // Accept optional leading 'v'
    if (s[0] == 'v' || s[0] == 'V') {
        s++;
    }

    int a = 0, b = 0, c = 0;
    if (sscanf(s, "%d.%d.%d", &a, &b, &c) != 3) {
        return false;
    }
    *major = a;
    *minor = b;
    *patch = c;
    return true;
}

static int compare_semver(const char *a, const char *b) {
    int am = 0, an = 0, ap = 0;
    int bm = 0, bn = 0, bp = 0;
    if (!parse_semver_triplet(a, &am, &an, &ap)) return 0;
    if (!parse_semver_triplet(b, &bm, &bn, &bp)) return 0;

    if (am != bm) return (am < bm) ? -1 : 1;
    if (an != bn) return (an < bn) ? -1 : 1;
    if (ap != bp) return (ap < bp) ? -1 : 1;
    return 0;
}

static bool github_fetch_latest_release(char *out_version, size_t out_version_len, char *out_asset_url, size_t out_asset_url_len, size_t *out_asset_size, char *out_error, size_t out_error_len) {
#if !GITHUB_UPDATES_ENABLED
    (void)out_version;
    (void)out_version_len;
    (void)out_asset_url;
    (void)out_asset_url_len;
    (void)out_asset_size;
    if (out_error && out_error_len > 0) {
        strlcpy(out_error, "GitHub updates disabled", out_error_len);
    }
    return false;
#else
    if (out_error && out_error_len > 0) out_error[0] = '\0';
    if (out_version && out_version_len > 0) out_version[0] = '\0';
    if (out_asset_url && out_asset_url_len > 0) out_asset_url[0] = '\0';
    if (out_asset_size) *out_asset_size = 0;

    if (WiFi.status() != WL_CONNECTED) {
        if (out_error && out_error_len > 0) {
            strlcpy(out_error, "WiFi not connected", out_error_len);
        }
        return false;
    }

    char api_url[256];
    snprintf(api_url, sizeof(api_url), "https://api.github.com/repos/%s/%s/releases/latest", GITHUB_OWNER, GITHUB_REPO);

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(15000);

    HTTPClient http;
    http.setTimeout(15000);

    if (!http.begin(client, api_url)) {
        if (out_error && out_error_len > 0) {
            strlcpy(out_error, "Failed to init HTTP client", out_error_len);
        }
        return false;
    }

    http.addHeader("User-Agent", "esp32-template-firmware");
    http.addHeader("Accept", "application/vnd.github+json");

    const int http_code = http.GET();
    if (http_code != 200) {
        if (out_error && out_error_len > 0) {
            snprintf(out_error, out_error_len, "GitHub API HTTP %d", http_code);
        }
        http.end();
        return false;
    }

    // Build expected app-only asset name: <project>-<board>-vX.Y.Z.bin
    const char *board = "unknown";
    #ifdef BUILD_BOARD_NAME
    board = BUILD_BOARD_NAME;
    #endif

    char expected_asset_name[160];
    expected_asset_name[0] = '\0';

    // Parse JSON with filter to reduce memory.
    // `assets` is an array, so use [0] to apply the filter to all elements.
    StaticJsonDocument<256> filter;
    filter["tag_name"] = true;
    filter["assets"][0]["name"] = true;
    filter["assets"][0]["browser_download_url"] = true;
    filter["assets"][0]["size"] = true;

    // Read the full response before parsing. Parsing directly from the stream can
    // occasionally fail with IncompleteInput if the connection stalls.
    String payload = http.getString();
    if (payload.length() == 0) {
        if (out_error && out_error_len > 0) {
            strlcpy(out_error, "GitHub API returned empty body", out_error_len);
        }
        http.end();
        return false;
    }

    BasicJsonDocument<PsramJsonAllocator> doc(8192);
    DeserializationError err = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
    if (err) {
        if (out_error && out_error_len > 0) {
            snprintf(out_error, out_error_len, "GitHub JSON parse error: %s", err.c_str());
        }
        http.end();
        return false;
    }

    const char *tag_name = doc["tag_name"] | "";
    if (!tag_name || strlen(tag_name) == 0) {
        if (out_error && out_error_len > 0) {
            strlcpy(out_error, "GitHub response missing tag_name", out_error_len);
        }
        http.end();
        return false;
    }

    // Strip leading 'v' for VERSION in filenames.
    const char *version = tag_name;
    if (version[0] == 'v' || version[0] == 'V') {
        version++;
    }

    snprintf(expected_asset_name, sizeof(expected_asset_name), "%s-%s-v%s.bin", PROJECT_NAME, board, version);

    JsonArray assets = doc["assets"].as<JsonArray>();
    const char *found_url = nullptr;
    size_t found_size = 0;

    for (JsonVariant v : assets) {
        const char *name = v["name"] | "";
        const char *url = v["browser_download_url"] | "";
        const size_t size = (size_t)(v["size"] | 0);
        if (name && url && strlen(name) > 0 && strcmp(name, expected_asset_name) == 0) {
            found_url = url;
            found_size = size;
            break;
        }
    }

    if (!found_url || strlen(found_url) == 0) {
        if (out_error && out_error_len > 0) {
            snprintf(out_error, out_error_len, "No asset found: %s", expected_asset_name);
        }
        http.end();
        return false;
    }

    if (out_version && out_version_len > 0) {
        strlcpy(out_version, version, out_version_len);
    }
    if (out_asset_url && out_asset_url_len > 0) {
        strlcpy(out_asset_url, found_url, out_asset_url_len);
    }
    if (out_asset_size) {
        *out_asset_size = found_size;
    }

    http.end();
    return true;
#endif
}

static void firmware_update_task(void *pv) {
    (void)pv;

    // Snapshot URL and size/version at task start.
    char url[sizeof(firmware_update_download_url)];
    char latest_version[sizeof(firmware_update_latest_version)];
    size_t expected_total = firmware_update_total;
    strlcpy(url, firmware_update_download_url, sizeof(url));
    strlcpy(latest_version, firmware_update_latest_version, sizeof(latest_version));

    firmware_update_progress = 0;
    strlcpy(firmware_update_state, "downloading", sizeof(firmware_update_state));
    firmware_update_error[0] = '\0';

    // Mark OTA in progress to block other OTA/image operations.
    ota_in_progress = true;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(10000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    if (!http.begin(client, url)) {
        strlcpy(firmware_update_state, "error", sizeof(firmware_update_state));
        strlcpy(firmware_update_error, "Failed to init download", sizeof(firmware_update_error));
        firmware_update_in_progress = false;
        ota_in_progress = false;
        vTaskDelete(nullptr);
        return;
    }

    http.addHeader("User-Agent", "esp32-template-firmware");
    const int http_code = http.GET();
    if (http_code != 200) {
        strlcpy(firmware_update_state, "error", sizeof(firmware_update_state));
        snprintf(firmware_update_error, sizeof(firmware_update_error), "Download HTTP %d", http_code);
        http.end();
        firmware_update_in_progress = false;
        ota_in_progress = false;
        vTaskDelete(nullptr);
        return;
    }

    int http_len = http.getSize();
    if (http_len <= 0) {
        http_len = -1; // unknown length
    }
    size_t total = (http_len > 0) ? (size_t)http_len : expected_total;
    firmware_update_total = total;

    const size_t freeSpace = device_telemetry_free_sketch_space();
    if (total > 0 && total > freeSpace) {
        strlcpy(firmware_update_state, "error", sizeof(firmware_update_state));
        snprintf(firmware_update_error, sizeof(firmware_update_error), "Firmware too large (%u > %u)", (unsigned)total, (unsigned)freeSpace);
        http.end();
        firmware_update_in_progress = false;
        ota_in_progress = false;
        vTaskDelete(nullptr);
        return;
    }

    if (!Update.begin((total > 0) ? total : UPDATE_SIZE_UNKNOWN, U_FLASH)) {
        strlcpy(firmware_update_state, "error", sizeof(firmware_update_state));
        strlcpy(firmware_update_error, "OTA begin failed", sizeof(firmware_update_error));
        http.end();
        firmware_update_in_progress = false;
        ota_in_progress = false;
        vTaskDelete(nullptr);
        return;
    }

    strlcpy(firmware_update_state, "writing", sizeof(firmware_update_state));

    WiFiClient *stream = http.getStreamPtr();
    uint8_t buf[2048];

    while (http.connected() && (http_len > 0 || http_len == -1)) {
        const size_t available = stream->available();
        if (!available) {
            delay(1);
            continue;
        }

        const size_t to_read = (available > sizeof(buf)) ? sizeof(buf) : available;
        const int read_bytes = stream->readBytes(buf, to_read);
        if (read_bytes <= 0) {
            break;
        }

        const size_t written = Update.write(buf, (size_t)read_bytes);
        if (written != (size_t)read_bytes) {
            strlcpy(firmware_update_state, "error", sizeof(firmware_update_state));
            strlcpy(firmware_update_error, "Flash write failed", sizeof(firmware_update_error));
            Update.abort();
            http.end();
            firmware_update_in_progress = false;
            ota_in_progress = false;
            vTaskDelete(nullptr);
            return;
        }

        firmware_update_progress += written;
        if (http_len > 0) {
            http_len -= (int)read_bytes;
        }
    }

    http.end();

    if (!Update.end(true)) {
        strlcpy(firmware_update_state, "error", sizeof(firmware_update_state));
        strlcpy(firmware_update_error, "OTA finalize failed", sizeof(firmware_update_error));
        firmware_update_in_progress = false;
        ota_in_progress = false;
        vTaskDelete(nullptr);
        return;
    }

    strlcpy(firmware_update_state, "rebooting", sizeof(firmware_update_state));
    (void)latest_version;

    // Give the HTTP response/polling a moment to observe completion.
    delay(300);
    ESP.restart();
    vTaskDelete(nullptr);
}

// GET /api/firmware/latest - Query GitHub releases/latest and compare with current firmware.
void handleGetFirmwareLatest(AsyncWebServerRequest *request) {
    if (!portal_auth_gate(request)) return;
#if !GITHUB_UPDATES_ENABLED
    request->send(404, "application/json", "{\"success\":false,\"message\":\"GitHub updates disabled\"}");
    return;
#else
    char latest[24];
    char url[512];
    size_t size = 0;
    char err[192];

    if (!github_fetch_latest_release(latest, sizeof(latest), url, sizeof(url), &size, err, sizeof(err))) {
        char resp[256];
        snprintf(resp, sizeof(resp), "{\"success\":false,\"message\":\"%s\"}", err[0] ? err : "Failed");
        request->send(500, "application/json", resp);
        return;
    }

    const bool update_available = (compare_semver(FIRMWARE_VERSION, latest) < 0);

    StaticJsonDocument<384> doc;
    doc["success"] = true;
    doc["current_version"] = FIRMWARE_VERSION;
    doc["latest_version"] = latest;
    doc["update_available"] = update_available;

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(doc, *response);
    request->send(response);
#endif
}

// POST /api/firmware/update - Start background download+OTA of latest app-only firmware from GitHub.
void handlePostFirmwareUpdate(AsyncWebServerRequest *request) {
    if (!portal_auth_gate(request)) return;
#if !GITHUB_UPDATES_ENABLED
    request->send(404, "application/json", "{\"success\":false,\"message\":\"GitHub updates disabled\"}");
    return;
#else
    if (ota_in_progress || firmware_update_in_progress) {
        request->send(409, "application/json", "{\"success\":false,\"message\":\"Update already in progress\"}");
        return;
    }

    char latest[24];
    char url[512];
    size_t size = 0;
    char err[192];

    if (!github_fetch_latest_release(latest, sizeof(latest), url, sizeof(url), &size, err, sizeof(err))) {
        char resp[256];
        snprintf(resp, sizeof(resp), "{\"success\":false,\"message\":\"%s\"}", err[0] ? err : "Failed");
        request->send(500, "application/json", resp);
        return;
    }

    // If no update is available, still allow re-install? For now, require newer.
    if (compare_semver(FIRMWARE_VERSION, latest) >= 0) {
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Already up to date\",\"update_started\":false}");
        return;
    }

    // Seed global state for status polling.
    firmware_update_in_progress = true;
    firmware_update_progress = 0;
    firmware_update_total = size;
    strlcpy(firmware_update_latest_version, latest, sizeof(firmware_update_latest_version));
    strlcpy(firmware_update_download_url, url, sizeof(firmware_update_download_url));
    firmware_update_error[0] = '\0';
    strlcpy(firmware_update_state, "downloading", sizeof(firmware_update_state));

    // Spawn background task to avoid blocking AsyncTCP.
    const BaseType_t ok = xTaskCreate(
        firmware_update_task,
        "fw_update",
        12288,
        nullptr,
        1,
        &firmware_update_task_handle
    );

    if (ok != pdPASS) {
        firmware_update_in_progress = false;
        strlcpy(firmware_update_state, "error", sizeof(firmware_update_state));
        strlcpy(firmware_update_error, "Failed to start update task", sizeof(firmware_update_error));
        request->send(500, "application/json", "{\"success\":false,\"message\":\"Failed to start update\"}");
        return;
    }

    StaticJsonDocument<256> doc;
    doc["success"] = true;
    doc["update_started"] = true;
    doc["current_version"] = FIRMWARE_VERSION;
    doc["latest_version"] = latest;

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(doc, *response);
    request->send(response);
#endif
}

// GET /api/firmware/update/status - Progress snapshot for online update.
void handleGetFirmwareUpdateStatus(AsyncWebServerRequest *request) {
    if (!portal_auth_gate(request)) return;
    StaticJsonDocument<384> doc;
    doc["enabled"] = (GITHUB_UPDATES_ENABLED ? true : false);
    doc["in_progress"] = firmware_update_in_progress;
    doc["state"] = firmware_update_state;
    doc["progress"] = (uint32_t)firmware_update_progress;
    doc["total"] = (uint32_t)firmware_update_total;
    doc["latest_version"] = firmware_update_latest_version;
    doc["error"] = firmware_update_error;

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(doc, *response);
    request->send(response);
}

#if HAS_IMAGE_API && HAS_DISPLAY
// AsyncWebServer callbacks run on the AsyncTCP task; never touch LVGL/display from there.
// Use this flag to defer "hide current image / return" operations to the main loop.
static volatile bool pending_image_hide_request = false;
#endif

// GET /api/mode - Return portal mode (core vs full)
void handleGetMode(AsyncWebServerRequest *request) {
    if (!portal_auth_gate(request)) return;
    
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    response->print("{\"mode\":\"");
    response->print(ap_mode_active ? "core" : "full");
    response->print("\",\"ap_active\":");
    response->print(ap_mode_active ? "true" : "false");
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
    StaticJsonDocument<1024> doc;

    device_telemetry_fill_api(doc);

    if (doc.overflowed()) {
        Logger.logMessage("Portal", "ERROR: /api/health JSON overflow (StaticJsonDocument too small)");
        request->send(500, "application/json", "{\"success\":false,\"message\":\"Response too large\"}");
        return;
    }

    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(doc, *response);
    request->send(response);
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

// POST /api/update - Handle OTA firmware upload
void handleOTAUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!portal_auth_gate(request)) return;
    // First chunk - initialize OTA
    if (index == 0) {
        // Guard against concurrent OTA uploads or online-update flow.
        bool allowed = false;
        portENTER_CRITICAL(&g_ota_upload_mux);
        if (!ota_in_progress && !firmware_update_in_progress) {
            ota_in_progress = true;
            g_ota_last_percent = 0;
            allowed = true;
        }
        portEXIT_CRITICAL(&g_ota_upload_mux);

        if (!allowed) {
            request->send(409, "application/json", "{\"success\":false,\"message\":\"Update already in progress\"}");
            return;
        }

        Logger.logBegin("OTA Update");
        Logger.logLinef("File: %s", filename.c_str());
        Logger.logLinef("Size: %d bytes", request->contentLength());

        ota_progress = 0;
        ota_total = request->contentLength();
        
        // Check if filename ends with .bin
        if (!filename.endsWith(".bin")) {
            Logger.logEnd("Not a .bin file");
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Only .bin files are supported\"}");
            portENTER_CRITICAL(&g_ota_upload_mux);
            ota_in_progress = false;
            portEXIT_CRITICAL(&g_ota_upload_mux);
            return;
        }
        
        // Get OTA partition size
        size_t updateSize = (ota_total > 0) ? ota_total : UPDATE_SIZE_UNKNOWN;
        size_t freeSpace = device_telemetry_free_sketch_space();
        
        Logger.logLinef("Free space: %d bytes", freeSpace);
        
        // Validate size before starting
        if (ota_total > 0 && ota_total > freeSpace) {
            Logger.logEnd("Firmware too large");
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Firmware too large\"}");
            portENTER_CRITICAL(&g_ota_upload_mux);
            ota_in_progress = false;
            portEXIT_CRITICAL(&g_ota_upload_mux);
            return;
        }
        
        // Begin OTA update
        if (!Update.begin(updateSize, U_FLASH)) {
            Logger.logEnd("Begin failed");
            Update.printError(Serial);
            request->send(500, "application/json", "{\"success\":false,\"message\":\"OTA begin failed\"}");
            portENTER_CRITICAL(&g_ota_upload_mux);
            ota_in_progress = false;
            portEXIT_CRITICAL(&g_ota_upload_mux);
            return;
        }
    }
    
    // Write chunk to flash
    if (len) {
        if (Update.write(data, len) != len) {
            Logger.logEnd("Write failed");
            Update.printError(Serial);
            Update.abort();
            request->send(500, "application/json", "{\"success\":false,\"message\":\"Write failed\"}");
            portENTER_CRITICAL(&g_ota_upload_mux);
            ota_in_progress = false;
            portEXIT_CRITICAL(&g_ota_upload_mux);
            return;
        }
        
        ota_progress += len;
        
        // Print progress every 10%
        if (ota_total > 0) {
            uint8_t percent = (ota_progress * 100) / ota_total;
            if (percent >= (uint8_t)(g_ota_last_percent + 10)) {
                Logger.logLinef("Progress: %d%%", percent);
                g_ota_last_percent = percent;
            }
        }
    }
    
    // Final chunk - complete OTA
    if (final) {
        if (Update.end(true)) {
            Logger.logLinef("Written: %d bytes", ota_progress);
            Logger.logEnd("Success - rebooting");
            
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Update successful! Rebooting...\"}");
            
            delay(500);
            ESP.restart();
        } else {
            Logger.logEnd("Update failed");
            Update.printError(Serial);
            request->send(500, "application/json", "{\"success\":false,\"message\":\"Update failed\"}");
        }

        portENTER_CRITICAL(&g_ota_upload_mux);
        ota_in_progress = false;
        portEXIT_CRITICAL(&g_ota_upload_mux);
    }
}

// ===== PUBLIC API =====

// Initialize web portal
void web_portal_init(DeviceConfig *config) {
    Logger.logBegin("Portal Init");
    
    current_config = config;
    Logger.logLinef("Portal config pointer: %p, backlight_brightness: %d", 
                    current_config, current_config->backlight_brightness);
    
    // Create web server instance (avoid global constructor issues)
    if (server == nullptr) {
        yield();
        delay(100);
        
        server = new AsyncWebServer(80);
        
        yield();
        delay(100);
    }

    // Routes (factored out for maintainability)
    web_portal_register_routes(server);
    
    // Image API integration (if enabled)
    #if HAS_IMAGE_API && HAS_DISPLAY
    Logger.logMessage("Portal", "Initializing image API");
    
    // Setup backend adapter
    ImageApiBackend backend;
    backend.hide_current_image = []() {
        #if HAS_DISPLAY
        // Called from AsyncTCP task and sometimes from the main loop.
        // Always defer actual display/LVGL operations to the main loop.
        pending_image_hide_request = true;
        #endif
    };
    
    backend.start_strip_session = [](int width, int height, unsigned long timeout_ms, unsigned long start_time) -> bool {
        #if HAS_DISPLAY
        (void)start_time;
        DirectImageScreen* screen = display_manager_get_direct_image_screen();
        if (!screen) {
            Logger.logMessage("ImageAPI", "ERROR: No direct image screen");
            return false;
        }
        
        // Now called from main loop with proper task context
        // Show the DirectImageScreen first
        display_manager_show_direct_image();

        // Screen-affecting action counts as explicit activity and should wake.
        screen_saver_manager_notify_activity(true);
        
        // Configure timeout and start session
        screen->set_timeout(timeout_ms);
        screen->begin_strip_session(width, height);
        return true;
        #else
        return false;
        #endif
    };
    
    backend.decode_strip = [](const uint8_t* jpeg_data, size_t jpeg_size, uint8_t strip_index, bool output_bgr565) -> bool {
        #if HAS_DISPLAY
        DirectImageScreen* screen = display_manager_get_direct_image_screen();
        if (!screen) {
            Logger.logMessage("ImageAPI", "ERROR: No direct image screen");
            return false;
        }
        
        // Now called from main loop - safe to decode
        return screen->decode_strip(jpeg_data, jpeg_size, strip_index, output_bgr565);
        #else
        return false;
        #endif
    };
    
    // Setup configuration
    ImageApiConfig image_cfg;

    // Use the display driver's coordinate space (fast path for direct image writes).
    // This intentionally avoids LVGL calls and any DISPLAY_ROTATION heuristics.
    image_cfg.lcd_width = DISPLAY_WIDTH;
    image_cfg.lcd_height = DISPLAY_HEIGHT;

    #if HAS_DISPLAY
        if (displayManager && displayManager->getDriver()) {
            image_cfg.lcd_width = displayManager->getDriver()->width();
            image_cfg.lcd_height = displayManager->getDriver()->height();
        }
    #endif
    
    image_cfg.max_image_size_bytes = IMAGE_API_MAX_SIZE_BYTES;
    image_cfg.decode_headroom_bytes = IMAGE_API_DECODE_HEADROOM_BYTES;
    image_cfg.default_timeout_ms = IMAGE_API_DEFAULT_TIMEOUT_MS;
    image_cfg.max_timeout_ms = IMAGE_API_MAX_TIMEOUT_MS;
    
    // Initialize and register routes
    Logger.logMessage("Portal", "Calling image_api_init...");
    image_api_init(image_cfg, backend);
    Logger.logMessage("Portal", "Calling image_api_register_routes...");
    image_api_register_routes(server, portal_auth_gate);
    Logger.logMessage("Portal", "Image API initialized");
    #endif // HAS_IMAGE_API && HAS_DISPLAY
    
    // 404 handler
    server->onNotFound([](AsyncWebServerRequest *request) {
        // In AP mode, redirect to root for captive portal
        if (ap_mode_active) {
            request->redirect("/");
        } else {
            request->send(404, "text/plain", "Not found");
        }
    });
    
    // Start server
    yield();
    delay(100);
    server->begin();

    log_async_tcp_stack_watermark_once();
    Logger.logEnd();
}

// Start AP mode with captive portal
void web_portal_start_ap() {
    Logger.logBegin("AP Mode");
    
    // Generate AP name with chip ID
    uint32_t chipId = 0;
    for(int i=0; i<17; i=i+8) {
        chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }
    
    // Convert PROJECT_NAME to uppercase for AP SSID
    String apPrefix = String(PROJECT_NAME);
    apPrefix.toUpperCase();
    String apName = apPrefix + "-" + String(chipId, HEX);
    
    Logger.logLinef("SSID: %s", apName.c_str());
    
    // Configure AP
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(CAPTIVE_PORTAL_IP, CAPTIVE_PORTAL_IP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(apName.c_str());
    
    // Start DNS server for captive portal (redirect all DNS queries to our IP)
    dnsServer.start(DNS_PORT, "*", CAPTIVE_PORTAL_IP);
    
    WiFi.softAPsetHostname(apName.c_str());

    // Mark AP mode active so watchdog/DNS handling knows we're in captive portal
    ap_mode_active = true;

    Logger.logLinef("IP: %s", WiFi.softAPIP().toString().c_str());
    Logger.logEnd("Captive portal active");
}

// Stop AP mode
void web_portal_stop_ap() {
    if (ap_mode_active) {
        Logger.logMessage("Portal", "Stopping AP mode");
        dnsServer.stop();
        WiFi.softAPdisconnect(true);
        ap_mode_active = false;
    }
}

// Handle web server (call in loop)
void web_portal_handle() {
    if (ap_mode_active) {
        dnsServer.processNextRequest();
    }

    web_portal_config_loop();
}

// Check if in AP mode
bool web_portal_is_ap_mode() {
    return ap_mode_active;
}

// Check if OTA update is in progress
bool web_portal_ota_in_progress() {
    return ota_in_progress;
}

#if HAS_IMAGE_API
// Process pending image uploads (call from main loop)
void web_portal_process_pending_images() {
    // If the image API asked us to hide/dismiss the current image (or recover
    // from a failure), do it from the main loop so DisplayManager can safely
    // clear direct-image mode.
    #if HAS_DISPLAY
    if (pending_image_hide_request) {
        pending_image_hide_request = false;
        display_manager_return_to_previous_screen();
    }
    #endif

    image_api_process_pending(ota_in_progress);
}
#endif
