#ifndef VERSION_H
#define VERSION_H

// Firmware version information
#define VERSION_MAJOR 0
#define VERSION_MINOR 0
#define VERSION_PATCH 3

// Build date (automatically set by compiler)
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__

// Convert version to string
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define VERSION_STRING TOSTRING(VERSION_MAJOR) "." TOSTRING(VERSION_MINOR) "." TOSTRING(VERSION_PATCH)
#define FIRMWARE_VERSION VERSION_STRING

// Function to print version information
inline void printVersionInfo() {
  Serial.println("=== Firmware Version ===");
  Serial.print("Version: ");
  Serial.println(VERSION_STRING);
  Serial.print("Build Date: ");
  Serial.println(BUILD_DATE);
  Serial.print("Build Time: ");
  Serial.println(BUILD_TIME);
  Serial.println("========================");
}

#endif // VERSION_H
