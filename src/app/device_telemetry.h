#ifndef DEVICE_TELEMETRY_H
#define DEVICE_TELEMETRY_H

#include <ArduinoJson.h>

// Initializes cached values used by device telemetry (safe to call multiple times).
// This exists to avoid re-entrant calls into ESP-IDF image helpers from different tasks.
void device_telemetry_init();

// Cached flash/sketch metadata helpers.
size_t device_telemetry_sketch_size();
size_t device_telemetry_free_sketch_space();

// Fill a JsonDocument with device telemetry for the web API (/api/health).
void device_telemetry_fill_api(JsonDocument &doc);

// Fill a JsonDocument with device telemetry optimized for MQTT publishing.
// Intentionally excludes volatile/low-value fields like IP address.
void device_telemetry_fill_mqtt(JsonDocument &doc);

// Get current CPU usage percentage (0-100).
// Thread-safe - reads cached value updated by background task.
int device_telemetry_get_cpu_usage();

// Get CPU usage min/max over the last 60 seconds.
void device_telemetry_get_cpu_minmax(int* out_min, int* out_max);

// Initialize CPU monitoring background task.
// Must be called once during setup.
void device_telemetry_start_cpu_monitoring();

#endif // DEVICE_TELEMETRY_H
