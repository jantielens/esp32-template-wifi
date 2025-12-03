/*
 * MQTT Client Manager
 * 
 * Manages MQTT connection for receiving camera image URLs from Home Assistant.
 * Handles connection lifecycle, automatic reconnection, and message callbacks.
 * 
 * USAGE:
 *   mqtt_client_init(&config);           // Initialize with config
 *   mqtt_client_set_callback(callback);  // Set message callback
 *   mqtt_client_loop();                  // Call in main loop()
 *   mqtt_client_disconnect();            // Clean disconnect
 */

#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <Arduino.h>
#include "config_manager.h"

// Callback function type for received messages
// Parameters: topic, payload (null-terminated string), length
typedef void (*MqttMessageCallback)(const char* topic, const char* payload, size_t length);

// Initialize MQTT client with configuration
// Returns true if configuration is valid and client is initialized
bool mqtt_client_init(const DeviceConfig* config);

// Set callback for received messages
void mqtt_client_set_callback(MqttMessageCallback callback);

// Connect to MQTT broker (call after WiFi is connected)
// Returns true if connected or connection attempt started
bool mqtt_client_connect();

// Disconnect from MQTT broker
void mqtt_client_disconnect();

// Check if connected to MQTT broker
bool mqtt_client_is_connected();

// Process MQTT messages and maintain connection
// Call this in main loop()
void mqtt_client_loop();

// Get connection status string for debugging
const char* mqtt_client_get_status();

#endif // MQTT_CLIENT_H
