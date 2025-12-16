#include "device_telemetry.h"

#include <Arduino.h>
#include <WiFi.h>
#include "soc/soc_caps.h"

// Temperature sensor support (ESP32-C3, ESP32-S2, ESP32-S3, ESP32-C2, ESP32-C6, ESP32-H2)
#if SOC_TEMP_SENSOR_SUPPORTED
#include "driver/temperature_sensor.h"
#endif

// CPU usage tracking (delta-based)
static uint32_t last_idle_runtime = 0;
static uint32_t last_total_runtime = 0;
static unsigned long last_cpu_check = 0;

// Flash/sketch metadata caching (avoid re-entrant ESP-IDF image/mmap helpers)
static bool flash_cache_initialized = false;
static size_t cached_sketch_size = 0;
static size_t cached_free_sketch_space = 0;

static void fill_common(JsonDocument &doc, bool include_ip_and_channel, bool include_debug_fields);

void device_telemetry_fill_api(JsonDocument &doc) {
    fill_common(doc, true, true);

    // =====================================================================
    // USER-EXTEND: Add your own sensors to the web "health" API (/api/health)
    // =====================================================================
    // If you want your external sensors to show up in the web portal health widget,
    // add fields here.
    //
    // IMPORTANT:
    // - The key "cpu_temperature" is used for the SoC/internal temperature.
    //   You can safely use "temperature" for an external/ambient sensor.
    // - If you also publish these over MQTT, keep the JSON keys identical in
    //   device_telemetry_fill_mqtt() so you can reuse the same HA templates.
    //
    // Example (commented out):
    // doc["temperature"] = 23.4;
    // doc["humidity"] = 55.2;
}

void device_telemetry_fill_mqtt(JsonDocument &doc) {
    fill_common(doc, false, false);

    // =====================================================================
    // USER-EXTEND: Add your own sensors to the MQTT state payload
    // =====================================================================
    // The MQTT integration publishes ONE batched JSON document (retained) to:
    //   devices/<sanitized>/health/state
    // Home Assistant entities then extract values via value_template, e.g.:
    //   {{ value_json.temperature }}
    //
    // Add your custom sensor fields below.
    //
    // IMPORTANT:
    // - The key "cpu_temperature" is used for the SoC/internal temperature.
    //   You can safely use "temperature" for an external/ambient sensor.
    //
    // Example (commented out):
    // doc["temperature"] = 23.4;
    // doc["humidity"] = 55.2;
}

void device_telemetry_init() {
    if (flash_cache_initialized) return;

    cached_sketch_size = ESP.getSketchSize();
    cached_free_sketch_space = ESP.getFreeSketchSpace();
    flash_cache_initialized = true;
}

size_t device_telemetry_sketch_size() {
    if (!flash_cache_initialized) {
        device_telemetry_init();
    }
    return cached_sketch_size;
}

size_t device_telemetry_free_sketch_space() {
    if (!flash_cache_initialized) {
        device_telemetry_init();
    }
    return cached_free_sketch_space;
}

static void fill_common(JsonDocument &doc, bool include_ip_and_channel, bool include_debug_fields) {
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

    // CPU (API includes cpu_freq; MQTT keeps payload smaller)
    if (include_debug_fields) {
        doc["cpu_freq"] = ESP.getCpuFreqMHz();
    }

    // CPU usage via IDLE task delta calculation
    TaskStatus_t task_stats[16];
    uint32_t total_runtime;
    int task_count = uxTaskGetSystemState(task_stats, 16, &total_runtime);

    uint32_t idle_runtime = 0;
    for (int i = 0; i < task_count; i++) {
        if (strstr(task_stats[i].pcTaskName, "IDLE") != nullptr) {
            idle_runtime += task_stats[i].ulRunTimeCounter;
        }
    }

    unsigned long now = millis();
    int cpu_usage = 0;

    if (last_cpu_check > 0 && (now - last_cpu_check) > 100) {
        uint32_t idle_delta = idle_runtime - last_idle_runtime;
        uint32_t total_delta = total_runtime - last_total_runtime;

        if (total_delta > 0) {
            float idle_percent = ((float)idle_delta / total_delta) * 100.0;
            cpu_usage = (int)(100.0 - idle_percent);
            if (cpu_usage < 0) cpu_usage = 0;
            if (cpu_usage > 100) cpu_usage = 100;
        }
    }

    last_idle_runtime = idle_runtime;
    last_total_runtime = total_runtime;
    last_cpu_check = now;

    doc["cpu_usage"] = cpu_usage;

    // CPU / SoC temperature
#if SOC_TEMP_SENSOR_SUPPORTED
    float temp_celsius = 0;
    temperature_sensor_handle_t temp_sensor = NULL;
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);

    if (temperature_sensor_install(&temp_sensor_config, &temp_sensor) == ESP_OK) {
        if (temperature_sensor_enable(temp_sensor) == ESP_OK) {
            if (temperature_sensor_get_celsius(temp_sensor, &temp_celsius) == ESP_OK) {
                doc["cpu_temperature"] = (int)temp_celsius;
            } else {
                doc["cpu_temperature"] = nullptr;
            }
            temperature_sensor_disable(temp_sensor);
        } else {
            doc["cpu_temperature"] = nullptr;
        }
        temperature_sensor_uninstall(temp_sensor);
    } else {
        doc["cpu_temperature"] = nullptr;
    }
#else
    doc["cpu_temperature"] = nullptr;
#endif

    // Memory
    doc["heap_free"] = ESP.getFreeHeap();
    doc["heap_min"] = ESP.getMinFreeHeap();
    if (include_debug_fields) {
        doc["heap_size"] = ESP.getHeapSize();
    }

    // Heap fragmentation
    size_t largest_block = ESP.getMaxAllocHeap();
    size_t free_heap = ESP.getFreeHeap();
    float fragmentation = 0;
    if (free_heap > 0) {
        fragmentation = (1.0 - ((float)largest_block / free_heap)) * 100.0;
    }
    doc["heap_fragmentation"] = (int)fragmentation;

    // Flash usage
    const size_t sketch_size = device_telemetry_sketch_size();
    const size_t free_sketch_space = device_telemetry_free_sketch_space();
    doc["flash_used"] = sketch_size;
    doc["flash_total"] = sketch_size + free_sketch_space;

    // WiFi stats (only if connected)
    if (WiFi.status() == WL_CONNECTED) {
        doc["wifi_rssi"] = WiFi.RSSI();

        if (include_ip_and_channel) {
            doc["wifi_channel"] = WiFi.channel();
            doc["ip_address"] = WiFi.localIP().toString();
            doc["hostname"] = WiFi.getHostname();
        }
    } else {
        doc["wifi_rssi"] = nullptr;

        if (include_ip_and_channel) {
            doc["wifi_channel"] = nullptr;
            doc["ip_address"] = nullptr;
            doc["hostname"] = nullptr;
        }
    }
}