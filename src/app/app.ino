#include "../version.h"

// Heartbeat interval in milliseconds
const unsigned long HEARTBEAT_INTERVAL = 5000;
unsigned long lastHeartbeat = 0;

void setup()
{
  // Initialize serial communication
  Serial.begin(115200);
  
  // Wait for serial port to connect
  delay(1000);
  
  // Print startup message
  Serial.println("\n=== ESP32 Application Started ===");
  printVersionInfo();
  Serial.println();
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
  
  lastHeartbeat = millis();
}

void loop()
{
  // Check if it's time for heartbeat
  unsigned long currentMillis = millis();
  
  if (currentMillis - lastHeartbeat >= HEARTBEAT_INTERVAL) {
    // Print heartbeat message
    Serial.print("[Heartbeat] Uptime: ");
    Serial.print(currentMillis / 1000);
    Serial.print(" seconds | Free Heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");
    
    lastHeartbeat = currentMillis;
  }
  
  // Small delay to prevent watchdog issues
  delay(10);
}