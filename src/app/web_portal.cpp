/*
 * Web Configuration Portal Implementation
 * 
 * Async web server with captive portal support.
 * Serves static files and provides REST API for configuration.
 */

#include "web_portal.h"
#include "web_assets.h"
#include "config_manager.h"
#include "../version.h"
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

// Web server on port 80 (pointer to avoid constructor issues)
AsyncWebServer *server = nullptr;

// DNS server for captive portal (port 53)
DNSServer dnsServer;

// AP configuration
#define AP_SSID_PREFIX "ESP32-"
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
    Serial.println("[Portal] Serving portal.html");
    request->send_P(200, "text/html", portal_html);
}

// Serve CSS
void handleCSS(AsyncWebServerRequest *request) {
    Serial.println("[Portal] Serving portal.css");
    request->send_P(200, "text/css", portal_css);
}

// Serve JavaScript
void handleJS(AsyncWebServerRequest *request) {
    Serial.println("[Portal] Serving portal.js");
    request->send_P(200, "application/javascript", portal_js);
}

// GET /api/mode - Return portal mode (core vs full)
void handleGetMode(AsyncWebServerRequest *request) {
    Serial.println("[Portal] GET /api/mode");
    
    JsonDocument doc;
    doc["mode"] = ap_mode_active ? "core" : "full";
    doc["ap_active"] = ap_mode_active;
    
    String response;
    serializeJson(doc, response);
    
    request->send(200, "application/json", response);
}

// GET /api/config - Return current configuration
void handleGetConfig(AsyncWebServerRequest *request) {
    Serial.println("[Portal] GET /api/config");
    
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
    Serial.println("[Portal] POST /api/config");
    
    if (!current_config) {
        request->send(500, "application/json", "{\"success\":false,\"message\":\"Config not initialized\"}");
        return;
    }
    
    // Parse JSON body
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);
    
    if (error) {
        Serial.print("[Portal] JSON parse error: ");
        Serial.println(error.c_str());
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
        Serial.println("[Portal] Configuration saved successfully");
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Configuration saved\"}");
        
        // Schedule reboot after response is sent
        delay(100);
        ESP.restart();
    } else {
        Serial.println("[Portal] Failed to save configuration");
        request->send(500, "application/json", "{\"success\":false,\"message\":\"Failed to save\"}");
    }
}

// DELETE /api/config - Reset configuration
void handleDeleteConfig(AsyncWebServerRequest *request) {
    Serial.println("[Portal] DELETE /api/config");
    
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
    Serial.println("[Portal] GET /api/info");
    
    JsonDocument doc;
    doc["version"] = FIRMWARE_VERSION;
    doc["build_date"] = BUILD_DATE;
    doc["build_time"] = BUILD_TIME;
    doc["chip_model"] = ESP.getChipModel();
    doc["chip_revision"] = ESP.getChipRevision();
    doc["chip_cores"] = ESP.getChipCores();
    doc["cpu_freq"] = ESP.getCpuFreqMHz();
    doc["flash_chip_size"] = ESP.getFlashChipSize();
    doc["psram_size"] = ESP.getPsramSize();
    doc["free_heap"] = ESP.getFreeHeap();
    doc["sketch_size"] = ESP.getSketchSize();
    doc["free_sketch_space"] = ESP.getFreeSketchSpace();
    
    String response;
    serializeJson(doc, response);
    
    request->send(200, "application/json", response);
}

// GET /api/health - Get device health statistics
void handleGetHealth(AsyncWebServerRequest *request) {
    Serial.println("[Portal] GET /api/health");
    
    JsonDocument doc;
    
    // System
    uint64_t uptime_us = esp_timer_get_time();
    doc["uptime_seconds"] = uptime_us / 1000000;
    
    // Reset reason
    esp_reset_reason_t reset_reason = esp_reset_reason();
    const char* reset_str = "Unknown";
    switch (reset_reason) {
        case ESP_RST_POWERON:   reset_str = "Power On"; break;
        case ESP_RST_SW:        reset_str = "Software"; break;
        case ESP_RST_PANIC:     reset_str = "Panic"; break;
        case ESP_RST_INT_WDT:   reset_str = "Interrupt WDT"; break;
        case ESP_RST_TASK_WDT:  reset_str = "Task WDT"; break;
        case ESP_RST_WDT:       reset_str = "WDT"; break;
        case ESP_RST_DEEPSLEEP: reset_str = "Deep Sleep"; break;
        case ESP_RST_BROWNOUT:  reset_str = "Brownout"; break;
        case ESP_RST_SDIO:      reset_str = "SDIO"; break;
        default: break;
    }
    doc["reset_reason"] = reset_str;
    
    // CPU
    doc["cpu_freq"] = ESP.getCpuFreqMHz();
    
    // CPU usage via IDLE task calculation
    TaskStatus_t task_stats[16];
    uint32_t total_runtime;
    int task_count = uxTaskGetSystemState(task_stats, 16, &total_runtime);
    
    uint32_t idle_runtime = 0;
    for (int i = 0; i < task_count; i++) {
        if (strstr(task_stats[i].pcTaskName, "IDLE") != nullptr) {
            idle_runtime += task_stats[i].ulRunTimeCounter;
        }
    }
    
    if (total_runtime > 0) {
        float idle_percent = ((float)idle_runtime / total_runtime) * 100.0;
        float cpu_percent = 100.0 - idle_percent;
        doc["cpu_usage"] = (int)cpu_percent;
    } else {
        doc["cpu_usage"] = 0;
    }
    
    // Temperature - Internal sensor (supported on ESP32-C3, S2, S3, C2, C6, H2)
#if SOC_TEMP_SENSOR_SUPPORTED
    float temp_celsius = 0;
    temperature_sensor_handle_t temp_sensor = NULL;
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
    
    if (temperature_sensor_install(&temp_sensor_config, &temp_sensor) == ESP_OK) {
        if (temperature_sensor_enable(temp_sensor) == ESP_OK) {
            if (temperature_sensor_get_celsius(temp_sensor, &temp_celsius) == ESP_OK) {
                doc["temperature"] = (int)temp_celsius;
            } else {
                doc["temperature"] = nullptr;
            }
            temperature_sensor_disable(temp_sensor);
        } else {
            doc["temperature"] = nullptr;
        }
        temperature_sensor_uninstall(temp_sensor);
    } else {
        doc["temperature"] = nullptr;
    }
#else
    // Original ESP32 and other chips without temp sensor support
    doc["temperature"] = nullptr;
#endif
    
    // Memory
    doc["heap_free"] = ESP.getFreeHeap();
    doc["heap_min"] = ESP.getMinFreeHeap();
    doc["heap_size"] = ESP.getHeapSize();
    
    // Heap fragmentation calculation
    size_t largest_block = ESP.getMaxAllocHeap();
    size_t free_heap = ESP.getFreeHeap();
    float fragmentation = 0;
    if (free_heap > 0) {
        fragmentation = (1.0 - ((float)largest_block / free_heap)) * 100.0;
    }
    doc["heap_fragmentation"] = (int)fragmentation;
    
    // Flash usage
    doc["flash_used"] = ESP.getSketchSize();
    doc["flash_total"] = ESP.getSketchSize() + ESP.getFreeSketchSpace();
    
    // WiFi stats (only if connected)
    if (WiFi.status() == WL_CONNECTED) {
        doc["wifi_rssi"] = WiFi.RSSI();
        doc["wifi_channel"] = WiFi.channel();
        doc["ip_address"] = WiFi.localIP().toString();
    } else {
        doc["wifi_rssi"] = nullptr;
        doc["wifi_channel"] = nullptr;
        doc["ip_address"] = nullptr;
    }
    
    String response;
    serializeJson(doc, response);
    
    request->send(200, "application/json", response);
}

// POST /api/reboot - Reboot device without saving
void handleReboot(AsyncWebServerRequest *request) {
    Serial.println("[Portal] POST /api/reboot");
    
    JsonDocument doc;
    doc["success"] = true;
    doc["message"] = "Rebooting device...";
    
    String response;
    serializeJson(doc, response);
    
    request->send(200, "application/json", response);
    
    // Schedule reboot after response is sent
    delay(100);
    Serial.println("[Portal] Rebooting...");
    ESP.restart();
}

// POST /api/update - Handle OTA firmware upload
void handleOTAUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    // First chunk - initialize OTA
    if (index == 0) {
        Serial.println("[OTA] Upload started");
        Serial.print("[OTA] Filename: ");
        Serial.println(filename);
        
        ota_in_progress = true;
        ota_progress = 0;
        ota_total = request->contentLength();
        
        Serial.print("[OTA] Total size: ");
        Serial.print(ota_total);
        Serial.println(" bytes");
        
        // Check if filename ends with .bin
        if (!filename.endsWith(".bin")) {
            Serial.println("[OTA] Error: Not a .bin file");
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Only .bin files are supported\"}");
            ota_in_progress = false;
            return;
        }
        
        // Get OTA partition size
        size_t updateSize = (ota_total > 0) ? ota_total : UPDATE_SIZE_UNKNOWN;
        
        Serial.print("[OTA] Free sketch space: ");
        Serial.print(ESP.getFreeSketchSpace());
        Serial.println(" bytes");
        
        // Validate size before starting
        if (ota_total > 0 && ota_total > ESP.getFreeSketchSpace()) {
            Serial.println("[OTA] Error: Firmware too large for OTA partition");
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Firmware too large\"}");
            ota_in_progress = false;
            return;
        }
        
        // Begin OTA update
        if (!Update.begin(updateSize, U_FLASH)) {
            Serial.println("[OTA] Error: Failed to begin OTA update");
            Update.printError(Serial);
            request->send(500, "application/json", "{\"success\":false,\"message\":\"OTA begin failed\"}");
            ota_in_progress = false;
            return;
        }
        
        Serial.println("[OTA] Update initialized successfully");
    }
    
    // Write chunk to flash
    if (len) {
        if (Update.write(data, len) != len) {
            Serial.println("[OTA] Error: Write failed");
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
            Serial.print("[OTA] Progress: ");
            Serial.print(percent);
            Serial.println("%");
            last_percent = percent;
        }
    }
    
    // Final chunk - complete OTA
    if (final) {
        Serial.println("[OTA] Upload complete, finalizing...");
        
        if (Update.end(true)) {
            Serial.println("[OTA] Update successful!");
            Serial.print("[OTA] Written: ");
            Serial.print(ota_progress);
            Serial.println(" bytes");
            
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Update successful! Rebooting...\"}");
            
            delay(500);
            
            Serial.println("[OTA] Rebooting...");
            ESP.restart();
        } else {
            Serial.println("[OTA] Error: Update failed");
            Update.printError(Serial);
            request->send(500, "application/json", "{\"success\":false,\"message\":\"Update failed\"}");
        }
        
        ota_in_progress = false;
    }
}

// ===== PUBLIC API =====

// Initialize web portal
void web_portal_init(DeviceConfig *config) {
    Serial.println("[Portal] Initializing web server...");
    
    current_config = config;
    
    // Create web server instance (avoid global constructor issues)
    if (server == nullptr) {
        yield();
        delay(100);
        
        Serial.println("[Portal] Creating AsyncWebServer instance...");
        server = new AsyncWebServer(80);
        Serial.println("[Portal] Web server instance created");
        
        yield();
        delay(100);
    }
    
    // Serve static files (embedded in PROGMEM)
    Serial.println("[Portal] Configuring routes...");
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
        Serial.print("[Portal] Not found: ");
        Serial.println(request->url());
        
        // In AP mode, redirect to root for captive portal
        if (ap_mode_active) {
            request->redirect("/");
        } else {
            request->send(404, "text/plain", "Not found");
        }
    });
    
    // Start server
    Serial.println("[Portal] Starting server...");
    yield();
    delay(100);
    server->begin();
    Serial.println("[Portal] Web server started");
}

// Start AP mode with captive portal
void web_portal_start_ap() {
    Serial.println("[Portal] Starting AP mode...");
    
    // Generate AP name with chip ID
    uint32_t chipId = 0;
    for(int i=0; i<17; i=i+8) {
        chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }
    
    String apName = String(AP_SSID_PREFIX) + String(chipId, HEX);
    apName.toUpperCase();
    
    Serial.print("[Portal] AP SSID: ");
    Serial.println(apName);
    
    // Configure AP
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(CAPTIVE_PORTAL_IP, CAPTIVE_PORTAL_IP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(apName.c_str());
    
    // Start DNS server for captive portal (redirect all DNS queries to our IP)
    dnsServer.start(DNS_PORT, "*", CAPTIVE_PORTAL_IP);
    
    ap_mode_active = true;
    
    Serial.print("[Portal] AP IP address: ");
    Serial.println(WiFi.softAPIP());
    Serial.println("[Portal] Captive portal active - connect to configure");
}

// Stop AP mode
void web_portal_stop_ap() {
    if (ap_mode_active) {
        Serial.println("[Portal] Stopping AP mode...");
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
