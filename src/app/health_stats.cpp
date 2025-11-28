#include "health_stats.h"

#include <WiFi.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

// Temperature sensor support (ESP32-C3, ESP32-S2, ESP32-S3, ESP32-C2, ESP32-C6, ESP32-H2)
#if SOC_TEMP_SENSOR_SUPPORTED
#include "driver/temperature_sensor.h"
#endif

// CPU usage tracking
static uint32_t last_idle_runtime = 0;
static uint32_t last_total_runtime = 0;
static unsigned long last_cpu_check = 0;

// Flash size cache (read once at boot to avoid bootloader_mmap conflicts)
static uint32_t cached_flash_used = 0;
static uint32_t cached_flash_total = 0;
static bool flash_size_cached = false;

bool collect_health_stats(HealthStats &out) {
    // System
    uint64_t uptime_us = esp_timer_get_time();
    out.uptime_seconds = uptime_us / 1000000ULL;

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
    out.reset_reason = reset_str;

    // CPU
    out.cpu_freq = ESP.getCpuFreqMHz();

    // CPU usage via IDLE task delta calculation
    TaskStatus_t task_stats[16];
    uint32_t total_runtime = 0;
    int task_count = uxTaskGetSystemState(task_stats, 16, &total_runtime);

    uint32_t idle_runtime = 0;
    for (int i = 0; i < task_count; i++) {
        if (strstr(task_stats[i].pcTaskName, "IDLE") != nullptr) {
            idle_runtime += task_stats[i].ulRunTimeCounter;
        }
    }

    // Calculate CPU usage based on delta since last measurement
    unsigned long now = millis();
    int cpu_usage = 0;

    if (last_cpu_check > 0 && (now - last_cpu_check) > 100) {  // Minimum 100ms between measurements
        uint32_t idle_delta = idle_runtime - last_idle_runtime;
        uint32_t total_delta = total_runtime - last_total_runtime;

        if (total_delta > 0) {
            float idle_percent = ((float)idle_delta / total_delta) * 100.0f;
            cpu_usage = (int)(100.0f - idle_percent);
            // Clamp to valid range
            if (cpu_usage < 0) cpu_usage = 0;
            if (cpu_usage > 100) cpu_usage = 100;
        }
    }

    // Update tracking variables
    last_idle_runtime = idle_runtime;
    last_total_runtime = total_runtime;
    last_cpu_check = now;

    out.cpu_usage = cpu_usage;

    // Temperature - Internal sensor (supported on ESP32-C3, S2, S3, C2, C6, H2)
#if SOC_TEMP_SENSOR_SUPPORTED
    float temp_celsius = 0;
    temperature_sensor_handle_t temp_sensor = NULL;
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);

    if (temperature_sensor_install(&temp_sensor_config, &temp_sensor) == ESP_OK) {
        if (temperature_sensor_enable(temp_sensor) == ESP_OK) {
            if (temperature_sensor_get_celsius(temp_sensor, &temp_celsius) == ESP_OK) {
                out.temperature_valid = true;
                out.temperature_c = (int)temp_celsius;
            } else {
                out.temperature_valid = false;
            }
            temperature_sensor_disable(temp_sensor);
        } else {
            out.temperature_valid = false;
        }
        temperature_sensor_uninstall(temp_sensor);
    } else {
        out.temperature_valid = false;
    }
#else
    // Original ESP32 and other chips without temp sensor support
    out.temperature_valid = false;
#endif

    // Memory
    out.heap_free = ESP.getFreeHeap();
    out.heap_min = ESP.getMinFreeHeap();
    out.heap_size = ESP.getHeapSize();

    // Heap fragmentation calculation
    size_t largest_block = ESP.getMaxAllocHeap();
    size_t free_heap = ESP.getFreeHeap();
    float fragmentation = 0;
    if (free_heap > 0) {
        fragmentation = (1.0f - ((float)largest_block / free_heap)) * 100.0f;
    }
    out.heap_fragmentation = (int)fragmentation;

    // Flash usage (cache on first call to avoid repeated flash partition reads)
    if (!flash_size_cached) {
        cached_flash_used = ESP.getSketchSize();
        cached_flash_total = ESP.getSketchSize() + ESP.getFreeSketchSpace();
        flash_size_cached = true;
    }
    out.flash_used = cached_flash_used;
    out.flash_total = cached_flash_total;

    // WiFi stats (only if connected)
    if (WiFi.status() == WL_CONNECTED) {
        out.wifi_connected = true;
        out.wifi_rssi = WiFi.RSSI();
        out.wifi_channel = WiFi.channel();
        out.ip_address = WiFi.localIP().toString();
        out.hostname = WiFi.getHostname();
    } else {
        out.wifi_connected = false;
        out.wifi_rssi = 0;
        out.wifi_channel = 0;
        out.ip_address = String();
        out.hostname = String();
    }

    return true;
}

void health_stats_to_json(const HealthStats &stats, JsonDocument &doc) {
    // System
    doc["uptime_seconds"] = stats.uptime_seconds;
    doc["reset_reason"] = stats.reset_reason;

    // CPU
    doc["cpu_freq"] = stats.cpu_freq;
    doc["cpu_usage"] = stats.cpu_usage;

    // Temperature
    if (stats.temperature_valid) {
        doc["temperature"] = stats.temperature_c;
    } else {
        doc["temperature"] = nullptr;
    }

    // Memory
    doc["heap_free"] = stats.heap_free;
    doc["heap_min"] = stats.heap_min;
    doc["heap_size"] = stats.heap_size;
    doc["heap_fragmentation"] = stats.heap_fragmentation;

    // Flash
    doc["flash_used"] = stats.flash_used;
    doc["flash_total"] = stats.flash_total;

    // Network
    if (stats.wifi_connected) {
        doc["wifi_rssi"] = stats.wifi_rssi;
        doc["wifi_channel"] = stats.wifi_channel;
        doc["ip_address"] = stats.ip_address;
        doc["hostname"] = stats.hostname;
    } else {
        doc["wifi_rssi"] = nullptr;
        doc["wifi_channel"] = nullptr;
        doc["ip_address"] = nullptr;
        doc["hostname"] = nullptr;
    }
}
