#include "../version.h"
#include "config_manager.h"
#include "web_portal.h"
#include <WiFi.h>
#include <ESPmDNS.h>

// Configuration
DeviceConfig device_config;
bool config_loaded = false;

// WiFi retry settings
const int WIFI_MAX_ATTEMPTS = 5;
const unsigned long WIFI_BACKOFF_BASE = 5000; // 5 seconds base

// Heartbeat interval
const unsigned long HEARTBEAT_INTERVAL = 5000;
unsigned long lastHeartbeat = 0;

void setup()
{
  // Initialize serial communication
  Serial.begin(115200);
  delay(1000);
  
  // Print startup message
  Serial.println("\n\n=================================");
  Serial.printf("ESP32 WiFi Portal v%s\n", FIRMWARE_VERSION);
  Serial.println("=================================");
  Serial.print("Chip Model: ");
  Serial.println(ESP.getChipModel());
  Serial.print("Chip Revision: ");
  Serial.println(ESP.getChipRevision());
  Serial.print("CPU Frequency: ");
  Serial.print(ESP.getCpuFreqMHz());
  Serial.println(" MHz");
  Serial.print("Flash Size: ");
  Serial.print(ESP.getFlashChipSize() / (1024 * 1024));
  Serial.println(" MB");
  Serial.println("=================================\n");
  
  // Initialize configuration manager
  config_manager_init();
  
  // Try to load saved configuration
  config_loaded = config_manager_load(&device_config);
  
  if (!config_loaded) {
    // No config found - set default device name
    String default_name = config_manager_get_default_device_name();
    strlcpy(device_config.device_name, default_name.c_str(), CONFIG_DEVICE_NAME_MAX_LEN);
    device_config.magic = CONFIG_MAGIC;
  }
  
  // Start WiFi BEFORE initializing web server (critical for ESP32-C3)
  if (!config_loaded) {
    Serial.println("[Main] No configuration found - starting AP mode");
    web_portal_start_ap();
  } else {
    Serial.println("[Main] Configuration loaded - connecting to WiFi");
    if (connect_wifi()) {
      Serial.println("[Main] WiFi connected - starting mDNS");
      start_mdns();
    } else {
      Serial.println("[Main] WiFi connection failed - falling back to AP mode");
      web_portal_start_ap();
    }
  }
  
  // Initialize web portal AFTER WiFi is started
  web_portal_init(&device_config);
  
  lastHeartbeat = millis();
  Serial.println("[Main] Setup complete\n");
}

void loop()
{
  // Handle web portal (DNS for captive portal)
  web_portal_handle();
  
  // Check if it's time for heartbeat
  unsigned long currentMillis = millis();
  if (currentMillis - lastHeartbeat >= HEARTBEAT_INTERVAL) {
    Serial.print("[Heartbeat] Uptime: ");
    Serial.print(currentMillis / 1000);
    Serial.print("s | Free Heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.print(" bytes | WiFi: ");
    Serial.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
    
    lastHeartbeat = currentMillis;
  }
  
  delay(10);
}

// Connect to WiFi with exponential backoff
bool connect_wifi() {
  Serial.print("[WiFi] Connecting to ");
  Serial.println(device_config.wifi_ssid);
  
  WiFi.mode(WIFI_STA);
  
  // Configure fixed IP if provided
  if (strlen(device_config.fixed_ip) > 0) {
    Serial.println("[WiFi] Configuring fixed IP...");
    
    IPAddress local_ip, gateway, subnet, dns1, dns2;
    
    if (!local_ip.fromString(device_config.fixed_ip)) {
      Serial.println("[WiFi] ERROR: Invalid fixed IP address");
      return false;
    }
    
    if (!subnet.fromString(device_config.subnet_mask)) {
      Serial.println("[WiFi] ERROR: Invalid subnet mask");
      return false;
    }
    
    if (!gateway.fromString(device_config.gateway)) {
      Serial.println("[WiFi] ERROR: Invalid gateway");
      return false;
    }
    
    // DNS1: use provided, or default to gateway
    if (strlen(device_config.dns1) > 0) {
      dns1.fromString(device_config.dns1);
    } else {
      dns1 = gateway;
    }
    
    // DNS2: optional
    if (strlen(device_config.dns2) > 0) {
      dns2.fromString(device_config.dns2);
    } else {
      dns2 = IPAddress(0, 0, 0, 0);
    }
    
    if (!WiFi.config(local_ip, gateway, subnet, dns1, dns2)) {
      Serial.println("[WiFi] ERROR: Failed to configure fixed IP");
      return false;
    }
    
    Serial.print("[WiFi] Fixed IP: ");
    Serial.println(device_config.fixed_ip);
  }
  
  WiFi.begin(device_config.wifi_ssid, device_config.wifi_password);
  
  // Try to connect with exponential backoff
  for (int attempt = 0; attempt < WIFI_MAX_ATTEMPTS; attempt++) {
    unsigned long backoff = WIFI_BACKOFF_BASE * (attempt + 1);
    unsigned long start = millis();
    
    Serial.print("[WiFi] Attempt ");
    Serial.print(attempt + 1);
    Serial.print("/");
    Serial.print(WIFI_MAX_ATTEMPTS);
    Serial.print(" (waiting ");
    Serial.print(backoff / 1000);
    Serial.println("s)");
    
    while (millis() - start < backoff) {
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WiFi] Connected!");
        Serial.print("[WiFi] IP address: ");
        Serial.println(WiFi.localIP());
        Serial.print("[WiFi] Signal strength: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
        return true;
      }
      delay(100);
    }
  }
  
  Serial.println("[WiFi] Connection failed after all attempts");
  return false;
}

// Start mDNS service
void start_mdns() {
  char sanitized[CONFIG_DEVICE_NAME_MAX_LEN];
  config_manager_sanitize_device_name(device_config.device_name, sanitized, CONFIG_DEVICE_NAME_MAX_LEN);
  
  if (strlen(sanitized) == 0) {
    Serial.println("[mDNS] ERROR: Sanitized name is empty");
    return;
  }
  
  if (MDNS.begin(sanitized)) {
    Serial.print("[mDNS] Started as ");
    Serial.print(sanitized);
    Serial.println(".local");
    
    MDNS.addService("http", "tcp", 80);
  } else {
    Serial.println("[mDNS] ERROR: Failed to start");
  }
}