#include "web_portal_firmware.h"

#include "web_portal_auth.h"
#include "web_portal_state.h"

#include "device_telemetry.h"
#include "github_release_config.h"
#include "log_manager.h"
#include "project_branding.h"
#include "psram_json_allocator.h"
#include "../version.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// ===== GitHub Releases firmware update (app-only) =====
static TaskHandle_t firmware_update_task_handle = nullptr;
static volatile bool firmware_update_in_progress = false;
static volatile size_t firmware_update_progress = 0;
static volatile size_t firmware_update_total = 0;

static char firmware_update_state[16] = "idle"; // idle|downloading|writing|rebooting|error
static char firmware_update_error[192] = "";
static char firmware_update_latest_version[24] = "";
static char firmware_update_download_url[512] = "";

bool web_portal_firmware_update_in_progress() {
    return firmware_update_in_progress;
}

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

static bool github_fetch_latest_release(
    char *out_version,
    size_t out_version_len,
    char *out_asset_url,
    size_t out_asset_url_len,
    size_t *out_asset_size,
    char *out_error,
    size_t out_error_len
) {
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

    firmware_update_progress = 0;
    strlcpy(firmware_update_state, "downloading", sizeof(firmware_update_state));
    firmware_update_error[0] = '\0';

    const char *url = firmware_update_download_url;
    const char *latest_version = firmware_update_latest_version;

    web_portal_set_ota_in_progress(true);

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(30000);

    HTTPClient http;
    http.setTimeout(30000);

    if (!http.begin(client, url)) {
        strlcpy(firmware_update_state, "error", sizeof(firmware_update_state));
        strlcpy(firmware_update_error, "Failed to start download", sizeof(firmware_update_error));
        firmware_update_in_progress = false;
        web_portal_set_ota_in_progress(false);
        vTaskDelete(nullptr);
        return;
    }

    const int http_code = http.GET();
    if (http_code != 200) {
        strlcpy(firmware_update_state, "error", sizeof(firmware_update_state));
        snprintf(firmware_update_error, sizeof(firmware_update_error), "Download HTTP %d", http_code);
        http.end();
        firmware_update_in_progress = false;
        web_portal_set_ota_in_progress(false);
        vTaskDelete(nullptr);
        return;
    }

    int http_len = http.getSize();
    const size_t expected_total = firmware_update_total;
    size_t total = (http_len > 0) ? (size_t)http_len : expected_total;
    firmware_update_total = total;

    const size_t freeSpace = device_telemetry_free_sketch_space();
    if (total > 0 && total > freeSpace) {
        strlcpy(firmware_update_state, "error", sizeof(firmware_update_state));
        snprintf(firmware_update_error, sizeof(firmware_update_error), "Firmware too large (%u > %u)", (unsigned)total, (unsigned)freeSpace);
        http.end();
        firmware_update_in_progress = false;
        web_portal_set_ota_in_progress(false);
        vTaskDelete(nullptr);
        return;
    }

    if (!Update.begin((total > 0) ? total : UPDATE_SIZE_UNKNOWN, U_FLASH)) {
        strlcpy(firmware_update_state, "error", sizeof(firmware_update_state));
        strlcpy(firmware_update_error, "OTA begin failed", sizeof(firmware_update_error));
        http.end();
        firmware_update_in_progress = false;
        web_portal_set_ota_in_progress(false);
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
            web_portal_set_ota_in_progress(false);
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
        web_portal_set_ota_in_progress(false);
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
    if (web_portal_ota_in_progress() || firmware_update_in_progress) {
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
