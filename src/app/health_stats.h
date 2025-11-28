#ifndef HEALTH_STATS_H
#define HEALTH_STATS_H

#include <Arduino.h>
#include <ArduinoJson.h>

struct HealthStats {
    uint32_t uptime_seconds = 0;
    String reset_reason;
    uint32_t cpu_freq = 0;
    int cpu_usage = 0;
    bool temperature_valid = false;
    int temperature_c = 0;

    uint32_t heap_free = 0;
    uint32_t heap_min = 0;
    uint32_t heap_size = 0;
    int heap_fragmentation = 0;

    uint32_t flash_used = 0;
    uint32_t flash_total = 0;

    bool wifi_connected = false;
    int wifi_rssi = 0;
    int wifi_channel = 0;
    String ip_address;
    String hostname;
};

// Collect current health statistics into `out`.
bool collect_health_stats(HealthStats &out);

// Serialize health statistics into an ArduinoJson document using the keys
// expected by /api/health and the web portal.
void health_stats_to_json(const HealthStats &stats, JsonDocument &doc);

#endif // HEALTH_STATS_H
