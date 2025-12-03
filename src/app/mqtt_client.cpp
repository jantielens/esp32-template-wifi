/*
 * MQTT Client Manager Implementation
 * 
 * Uses PubSubClient library for MQTT communication.
 * Automatically reconnects on connection loss.
 */

#include "mqtt_client.h"
#include "log_manager.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// MQTT client state
static WiFiClient wifi_client;
static PubSubClient mqtt_client(wifi_client);
static MqttMessageCallback user_callback = nullptr;
static DeviceConfig mqtt_config;
static bool mqtt_initialized = false;
static unsigned long last_reconnect_attempt = 0;
static const unsigned long RECONNECT_INTERVAL = 5000; // 5 seconds

// Internal callback wrapper for PubSubClient
static void mqtt_internal_callback(char* topic, byte* payload, unsigned int length) {
    if (!user_callback) return;
    
    // Create null-terminated string from payload
    char* message = (char*)malloc(length + 1);
    if (!message) {
        Logger.logMessage("MQTT", "Failed to allocate memory for message");
        return;
    }
    
    memcpy(message, payload, length);
    message[length] = '\0';
    
    Logger.logBegin("MQTT Message");
    Logger.logLinef("Topic: %s", topic);
    Logger.logLinef("Length: %u bytes", length);
    Logger.logEnd();
    
    // Call user callback
    user_callback(topic, message, length);
    
    free(message);
}

// Initialize MQTT client with configuration
bool mqtt_client_init(const DeviceConfig* config) {
    if (!config) {
        Logger.logMessage("MQTT", "Init failed: NULL config");
        return false;
    }
    
    if (!config->mqtt_enabled) {
        Logger.logMessage("MQTT", "MQTT is disabled in config");
        return false;
    }
    
    if (strlen(config->mqtt_host) == 0) {
        Logger.logMessage("MQTT", "Init failed: No MQTT host configured");
        return false;
    }
    
    // Copy config
    memcpy(&mqtt_config, config, sizeof(DeviceConfig));
    
    // Configure MQTT client
    mqtt_client.setServer(mqtt_config.mqtt_host, mqtt_config.mqtt_port);
    mqtt_client.setCallback(mqtt_internal_callback);
    mqtt_client.setBufferSize(512); // Enough for JSON with URL
    
    mqtt_initialized = true;
    
    Logger.logBegin("MQTT Init");
    Logger.logLinef("Broker: %s:%d", mqtt_config.mqtt_host, mqtt_config.mqtt_port);
    Logger.logLinef("Topic: %s", mqtt_config.mqtt_topic);
    Logger.logEnd();
    
    return true;
}

// Set callback for received messages
void mqtt_client_set_callback(MqttMessageCallback callback) {
    user_callback = callback;
}

// Connect to MQTT broker
bool mqtt_client_connect() {
    if (!mqtt_initialized) {
        Logger.logMessage("MQTT", "Not initialized");
        return false;
    }
    
    if (!WiFi.isConnected()) {
        Logger.logMessage("MQTT", "WiFi not connected");
        return false;
    }
    
    if (mqtt_client.connected()) {
        return true; // Already connected
    }
    
    Logger.logBegin("MQTT Connect");
    Logger.logLinef("Broker: %s:%d", mqtt_config.mqtt_host, mqtt_config.mqtt_port);
    
    // Generate client ID from device name
    char client_id[64];
    snprintf(client_id, sizeof(client_id), "%s_%08X", 
             mqtt_config.device_name, (uint32_t)ESP.getEfuseMac());
    
    Logger.logLinef("Client ID: %s", client_id);
    
    // Attempt connection
    bool connected = false;
    if (strlen(mqtt_config.mqtt_username) > 0) {
        // Connect with credentials
        connected = mqtt_client.connect(client_id, 
                                       mqtt_config.mqtt_username, 
                                       mqtt_config.mqtt_password);
    } else {
        // Connect without credentials
        connected = mqtt_client.connect(client_id);
    }
    
    if (connected) {
        Logger.logLine("Connected!");
        
        // Subscribe to topic
        Logger.logLinef("Subscribing to: %s", mqtt_config.mqtt_topic);
        if (mqtt_client.subscribe(mqtt_config.mqtt_topic)) {
            Logger.logLine("Subscribed successfully");
            Logger.logEnd();
            return true;
        } else {
            Logger.logLine("Subscribe failed");
            Logger.logEnd();
            return false;
        }
    } else {
        int state = mqtt_client.state();
        Logger.logLinef("Connection failed, rc=%d", state);
        
        // Decode error state
        switch (state) {
            case -4: Logger.logLine("MQTT_CONNECTION_TIMEOUT"); break;
            case -3: Logger.logLine("MQTT_CONNECTION_LOST"); break;
            case -2: Logger.logLine("MQTT_CONNECT_FAILED"); break;
            case -1: Logger.logLine("MQTT_DISCONNECTED"); break;
            case 1: Logger.logLine("MQTT_CONNECT_BAD_PROTOCOL"); break;
            case 2: Logger.logLine("MQTT_CONNECT_BAD_CLIENT_ID"); break;
            case 3: Logger.logLine("MQTT_CONNECT_UNAVAILABLE"); break;
            case 4: Logger.logLine("MQTT_CONNECT_BAD_CREDENTIALS"); break;
            case 5: Logger.logLine("MQTT_CONNECT_UNAUTHORIZED"); break;
            default: Logger.logLine("MQTT_UNKNOWN_ERROR"); break;
        }
        
        Logger.logEnd();
        return false;
    }
}

// Disconnect from MQTT broker
void mqtt_client_disconnect() {
    if (mqtt_client.connected()) {
        Logger.logMessage("MQTT", "Disconnecting...");
        mqtt_client.disconnect();
    }
    mqtt_initialized = false;
}

// Check if connected to MQTT broker
bool mqtt_client_is_connected() {
    return mqtt_initialized && mqtt_client.connected();
}

// Process MQTT messages and maintain connection
void mqtt_client_loop() {
    if (!mqtt_initialized) return;
    
    if (mqtt_client.connected()) {
        mqtt_client.loop();
    } else {
        // Attempt reconnection with throttling
        unsigned long now = millis();
        if (now - last_reconnect_attempt > RECONNECT_INTERVAL) {
            last_reconnect_attempt = now;
            
            if (WiFi.isConnected()) {
                Logger.logMessage("MQTT", "Reconnecting...");
                mqtt_client_connect();
            }
        }
    }
}

// Get connection status string
const char* mqtt_client_get_status() {
    if (!mqtt_initialized) return "Not initialized";
    if (mqtt_client.connected()) return "Connected";
    if (!WiFi.isConnected()) return "WiFi disconnected";
    
    int state = mqtt_client.state();
    switch (state) {
        case -4: return "Connection timeout";
        case -3: return "Connection lost";
        case -2: return "Connect failed";
        case -1: return "Disconnected";
        case 1: return "Bad protocol";
        case 2: return "Bad client ID";
        case 3: return "Unavailable";
        case 4: return "Bad credentials";
        case 5: return "Unauthorized";
        default: return "Unknown error";
    }
}
