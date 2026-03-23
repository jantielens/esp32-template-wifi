#pragma once

#include "board_config.h"

#if HAS_MQTT && HAS_DISPLAY

#include "mqtt_manager.h"

// Subscribes to the configured screen saver wake topic and wakes the display
// when a payload of "ON", "1", or "true" (case-insensitive) is received.
//
// Call mqtt_wake_begin() once after MqttManager::begin() if a wake topic is configured.
// No per-loop call needed — the wake is handled by the MQTT callback.

void mqtt_wake_begin(MqttManager *mqtt, const char *topic);

#endif // HAS_MQTT && HAS_DISPLAY
