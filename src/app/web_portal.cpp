/*
 * Web Configuration Portal Implementation
 * 
 * Async web server with captive portal support.
 * Serves static files and provides REST API for configuration.
 */

// Increase AsyncTCP task stack size to prevent overflow
// Default is 8192, increase to 16384 for web assets
#define CONFIG_ASYNC_TCP_STACK_SIZE 16384

#include "web_portal.h"
#include "web_assets.h"
#include "config_manager.h"
#include "log_manager.h"
#include "../version.h"
#include "health_stats.h"
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Update.h>

// Temperature sensor support (ESP32-C3, ESP32-S2, ESP32-S3, ESP32-C2, ESP32-C6, ESP32-H2)
#if SOC_TEMP_SENSOR_SUPPORTED
#include "driver/temperature_sensor.h"
#endif

// Forward declarations
void handleRoot(AsyncWebServerRequest *request);
void handleCSS(AsyncWebServerRequest *request);
void handleJS(AsyncWebServerRequest *request);
void handleGetConfig(AsyncWebServerRequest *request);
void handlePostConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void handleDeleteConfig(AsyncWebServerRequest *request);
void handleGetVersion(AsyncWebServerRequest *request);
void handleGetMode(AsyncWebServerRequest *request);
void handleGetHealth(AsyncWebServerRequest *request);
void handleOTAUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void handlePostDemoCaption(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void handlePostDisplayCamera(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void handlePostDisplayCamera(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
#if defined(HAS_DISPLAY) && HAS_DISPLAY
extern bool enqueue_boot_status(const char *text);
#endif

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

// ===== WEB SERVER HANDLERS =====

// Serve portal HTML
void handleRoot(AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", 
                                                               portal_html_gz, 
                                                               portal_html_gz_len);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

// Serve CSS
void handleCSS(AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "text/css", 
                                                               portal_css_gz, 
                                                               portal_css_gz_len);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

// Serve JavaScript
void handleJS(AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "application/javascript", 
                                                               portal_js_gz, 
                                                               portal_js_gz_len);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
}

// GET /api/mode - Return portal mode (core vs full)
void handleGetMode(AsyncWebServerRequest *request) {
    
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    response->print("{\"mode\":\"");
    response->print(ap_mode_active ? "core" : "full");
    response->print("\",\"ap_active\":");
    response->print(ap_mode_active ? "true" : "false");
    response->print("}");
    request->send(response);
}

// GET /api/config - Return current configuration
void handleGetConfig(AsyncWebServerRequest *request) {
    
    if (!current_config) {
        request->send(500, "application/json", "{\"error\":\"Config not initialized\"}");
        return;
    }
    
    // Create JSON response (don't include passwords)
    JsonDocument doc;
    doc["wifi_ssid"] = current_config->wifi_ssid;
    doc["wifi_password"] = ""; // Don't send password
    doc["device_name"] = current_config->device_name;
    
    // Sanitized name for display
    char sanitized[CONFIG_DEVICE_NAME_MAX_LEN];
    config_manager_sanitize_device_name(current_config->device_name, sanitized, CONFIG_DEVICE_NAME_MAX_LEN);
    doc["device_name_sanitized"] = sanitized;
    
    // Fixed IP settings
    doc["fixed_ip"] = current_config->fixed_ip;
    doc["subnet_mask"] = current_config->subnet_mask;
    doc["gateway"] = current_config->gateway;
    doc["dns1"] = current_config->dns1;
    doc["dns2"] = current_config->dns2;
    
    // Dummy setting
    doc["dummy_setting"] = current_config->dummy_setting;
    
    String response;
    serializeJson(doc, response);
    
    request->send(200, "application/json", response);
}

// POST /api/config - Save new configuration
void handlePostConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    
    if (!current_config) {
        request->send(500, "application/json", "{\"success\":false,\"message\":\"Config not initialized\"}");
        return;
    }
    
    // Parse JSON body
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    
    if (error) {
        Logger.logMessagef("Portal", "JSON parse error: %s", error.c_str());
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
        return;
    }
    
    // Update config structure
    strlcpy(current_config->wifi_ssid, doc["wifi_ssid"] | "", CONFIG_SSID_MAX_LEN);
    
    // Only update password if provided
    const char* wifi_pass = doc["wifi_password"];
    if (wifi_pass && strlen(wifi_pass) > 0) {
        strlcpy(current_config->wifi_password, wifi_pass, CONFIG_PASSWORD_MAX_LEN);
    }
    
    // Device name
    const char* device_name = doc["device_name"];
    if (device_name && strlen(device_name) > 0) {
        strlcpy(current_config->device_name, device_name, CONFIG_DEVICE_NAME_MAX_LEN);
    } else {
        // Use default if not provided
        String default_name = config_manager_get_default_device_name();
        strlcpy(current_config->device_name, default_name.c_str(), CONFIG_DEVICE_NAME_MAX_LEN);
    }
    
    // Fixed IP settings
    strlcpy(current_config->fixed_ip, doc["fixed_ip"] | "", CONFIG_IP_STR_MAX_LEN);
    strlcpy(current_config->subnet_mask, doc["subnet_mask"] | "", CONFIG_IP_STR_MAX_LEN);
    strlcpy(current_config->gateway, doc["gateway"] | "", CONFIG_IP_STR_MAX_LEN);
    strlcpy(current_config->dns1, doc["dns1"] | "", CONFIG_IP_STR_MAX_LEN);
    strlcpy(current_config->dns2, doc["dns2"] | "", CONFIG_IP_STR_MAX_LEN);
    
    // Dummy setting
    strlcpy(current_config->dummy_setting, doc["dummy_setting"] | "", CONFIG_DUMMY_MAX_LEN);
    
    current_config->magic = CONFIG_MAGIC;
    
    // Validate config
    if (!config_manager_is_valid(current_config)) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid configuration\"}");
        return;
    }
    
    // Save to NVS
    if (config_manager_save(current_config)) {
        Logger.logMessage("Portal", "Config saved - rebooting");
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Configuration saved\"}");
        
        // Schedule reboot after response is sent
        delay(100);
        ESP.restart();
    } else {
        Logger.logMessage("Portal", "Config save failed");
        request->send(500, "application/json", "{\"success\":false,\"message\":\"Failed to save\"}");
    }
}

// POST /api/demo/caption - Update demo button caption (not persisted)
void handlePostDemoCaption(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    // Basic JSON parse
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
        return;
    }
    const char *caption = doc["caption"] | "";
    if (!caption || strlen(caption) == 0) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Caption required\"}");
        return;
    }
    if (strlen(caption) > 63) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Caption too long (max 63)\"}");
        return;
    }

    extern bool enqueue_demo_caption(const char *text);
    if (!enqueue_demo_caption(caption)) {
        request->send(500, "application/json", "{\"success\":false,\"message\":\"Queue full\"}");
        return;
    }

    request->send(200, "application/json", "{\"success\":true}");
}

// POST /api/display-camera - Display camera image from URL
void handlePostDisplayCamera(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
#if defined(HAS_DISPLAY) && HAS_DISPLAY
    // Parse JSON payload: {"url": "http://..."}
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
        return;
    }
    
    const char *url = doc["url"];
    if (!url || strlen(url) == 0) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"URL required\"}");
        return;
    }
    
    if (strlen(url) > 512) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"URL too long (max 512)\"}");
        return;
    }
    
    Logger.logMessagef("Portal", "Camera display request: %s", url);
    
    // Trigger camera screen display via screen manager
    extern ScreenManager UI;
    UI.showCameraImageFromUrl(url);
    
    request->send(200, "application/json", "{\"success\":true}");
#else
    request->send(501, "application/json", "{\"success\":false,\"message\":\"Display not available\"}");
#endif
}

// DELETE /api/config - Reset configuration
void handleDeleteConfig(AsyncWebServerRequest *request) {
    
    if (config_manager_reset()) {
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Configuration reset\"}");
        
        // Schedule reboot after response is sent
        delay(100);
        ESP.restart();
    } else {
        request->send(500, "application/json", "{\"success\":false,\"message\":\"Failed to reset\"}");
    }
}

// GET /api/info - Get device information
void handleGetVersion(AsyncWebServerRequest *request) {
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
    response->print(ESP.getSketchSize());
    response->print(",\"free_sketch_space\":");
    response->print(ESP.getFreeSketchSpace());
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
    response->print("\"}");
    request->send(response);
}

// GET /api/health - Get device health statistics
void handleGetHealth(AsyncWebServerRequest *request) {
    HealthStats stats;
    collect_health_stats(stats);

    DynamicJsonDocument doc(768);
    health_stats_to_json(stats, doc);

    String response;
    serializeJson(doc, response);

    request->send(200, "application/json", response);
}

// POST /api/reboot - Reboot device without saving
void handleReboot(AsyncWebServerRequest *request) {
    Logger.logMessage("API", "POST /api/reboot");
    
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Rebooting device...\"}");
    
    // Schedule reboot after response is sent
    delay(100);
    Logger.logMessage("Portal", "Rebooting");
    ESP.restart();
}

// POST /api/update - Handle OTA firmware upload
void handleOTAUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    // First chunk - initialize OTA
    if (index == 0) {

        
        Logger.logBegin("OTA Update");
        Logger.logLinef("File: %s", filename.c_str());
        Logger.logLinef("Size: %d bytes", request->contentLength());
        
        ota_in_progress = true;
        ota_progress = 0;
        ota_total = request->contentLength();
        
        // Check if filename ends with .bin
        if (!filename.endsWith(".bin")) {
            Logger.logEnd("Not a .bin file");
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Only .bin files are supported\"}");
            ota_in_progress = false;
            return;
        }
        
        // Get OTA partition size
        size_t updateSize = (ota_total > 0) ? ota_total : UPDATE_SIZE_UNKNOWN;
        size_t freeSpace = ESP.getFreeSketchSpace();
        
        Logger.logLinef("Free space: %d bytes", freeSpace);
        
        // Validate size before starting
        if (ota_total > 0 && ota_total > freeSpace) {
            Logger.logEnd("Firmware too large");
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Firmware too large\"}");
            ota_in_progress = false;
            return;
        }
        
        // Begin OTA update
        if (!Update.begin(updateSize, U_FLASH)) {
            Logger.logEnd("Begin failed");
            Update.printError(Serial);
            request->send(500, "application/json", "{\"success\":false,\"message\":\"OTA begin failed\"}");
            ota_in_progress = false;
            return;
        }
    }
    
    // Write chunk to flash
    if (len) {
        if (Update.write(data, len) != len) {
            Logger.logEnd("Write failed");
            Update.printError(Serial);
            request->send(500, "application/json", "{\"success\":false,\"message\":\"Write failed\"}");
            ota_in_progress = false;
            return;
        }
        
        ota_progress += len;
        
        // Print progress every 10%
        static uint8_t last_percent = 0;
        uint8_t percent = (ota_progress * 100) / ota_total;
        if (percent >= last_percent + 10) {
            Logger.logLinef("Progress: %d%%", percent);
            last_percent = percent;
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
        
        ota_in_progress = false;
    }
}

// ===== PUBLIC API =====

// Initialize web portal
void web_portal_init(DeviceConfig *config) {
    Logger.logBegin("Portal Init");
    
    current_config = config;
    
    // Create web server instance (avoid global constructor issues)
    if (server == nullptr) {
        yield();
        delay(100);
        
        server = new AsyncWebServer(80);
        
        yield();
        delay(100);
    }

    server->on("/", HTTP_GET, handleRoot);
    server->on("/portal.css", HTTP_GET, handleCSS);
    server->on("/portal.js", HTTP_GET, handleJS);
    
    // API endpoints
    server->on("/api/mode", HTTP_GET, handleGetMode);
    server->on("/api/config", HTTP_GET, handleGetConfig);
    
        server->on("/api/config", HTTP_POST, 
        [](AsyncWebServerRequest *request) {},
        NULL,
        handlePostConfig
    );
        server->on("/api/demo/caption", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, handlePostDemoCaption);
        server->on("/api/display-camera", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, handlePostDisplayCamera);
    
    server->on("/api/config", HTTP_DELETE, handleDeleteConfig);
    server->on("/api/info", HTTP_GET, handleGetVersion);
    server->on("/api/health", HTTP_GET, handleGetHealth);
    server->on("/api/reboot", HTTP_POST, handleReboot);
    
    // OTA upload endpoint
    server->on("/api/update", HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        handleOTAUpload
    );
    
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

#if defined(HAS_DISPLAY) && HAS_DISPLAY
    char status_msg[64];
    snprintf(status_msg, sizeof(status_msg), "AP: %s @ %s", apName.c_str(), WiFi.softAPIP().toString().c_str());
    enqueue_boot_status(status_msg);
#endif
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
}

// Check if in AP mode
bool web_portal_is_ap_mode() {
    return ap_mode_active;
}

// Check if OTA update is in progress
bool web_portal_ota_in_progress() {
    return ota_in_progress;
}
