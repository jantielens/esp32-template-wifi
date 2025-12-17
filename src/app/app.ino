#include "../version.h"
#include "board_config.h"
#include "config_manager.h"
#include "web_portal.h"
#include "log_manager.h"
#include "mqtt_manager.h"
#include "device_telemetry.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <lwip/netif.h>

#if HAS_DISPLAY
#include <TFT_eSPI.h>
#include <SPI.h>
#include <lvgl.h>
#endif

// Configuration
DeviceConfig device_config;
bool config_loaded = false;

#if HAS_DISPLAY
TFT_eSPI tft = TFT_eSPI();
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[LVGL_BUFFER_SIZE];
static lv_disp_drv_t disp_drv;
#endif


#if HAS_MQTT
MqttManager mqtt_manager;
#endif

// WiFi retry settings
const unsigned long WIFI_BACKOFF_BASE = 3000; // 3 seconds base (DHCP typically needs 2-3s)

// Heartbeat interval
const unsigned long HEARTBEAT_INTERVAL = 60000; // 60 seconds
unsigned long lastHeartbeat = 0;

// WiFi watchdog for connection monitoring
const unsigned long WIFI_CHECK_INTERVAL = 10000; // 10 seconds
unsigned long lastWiFiCheck = 0;

#if HAS_DISPLAY
// ============================================================================
// Display Functions
// ============================================================================

// LVGL display flush callback
void lvgl_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();
  
  lv_disp_flush_ready(disp);
}

void init_display() {
  Logger.logBegin("Display Init");
  
  // Initialize TFT
  tft.init();
  tft.setRotation(DISPLAY_ROTATION);
  
  // Turn on backlight
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
  
  Logger.logLinef("Driver: ILI9341 (%dx%d)", DISPLAY_WIDTH, DISPLAY_HEIGHT);
  Logger.logLinef("Rotation: %d", DISPLAY_ROTATION);
  
  // Apply display-specific settings
  #ifdef DISPLAY_INVERSION_ON
  tft.invertDisplay(true);
  Logger.logLine("Inversion: ON");
  #endif
  
  #ifdef DISPLAY_INVERSION_OFF
  tft.invertDisplay(false);
  Logger.logLine("Inversion: OFF");
  #endif
  
  // Apply gamma fix (both v2 and v3 variants need this)
  // See: https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/blob/main/cyd.md
  // See: https://github.com/Bodmer/TFT_eSPI/issues/2985
  #ifdef DISPLAY_NEEDS_GAMMA_FIX
  Logger.logLine("Applying gamma correction fix...");
  // Gamma curve selection sequence - improves grayscale gradients
  tft.writecommand(0x26); // ILI9341_GAMMASET - Gamma curve selected
  tft.writedata(2);       // Gamma curve 2
  delay(120);
  tft.writecommand(0x26); // ILI9341_GAMMASET - Gamma curve selected
  tft.writedata(1);       // Gamma curve 1
  Logger.logLine("Gamma fix applied");
  #endif
  
  Logger.logEnd();
  
  // Initialize LVGL
  Logger.logBegin("LVGL Init");
  lv_init();
  
  // Set up display buffer
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, LVGL_BUFFER_SIZE);
  
  // Initialize display driver
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = DISPLAY_WIDTH;
  disp_drv.ver_res = DISPLAY_HEIGHT;
  disp_drv.flush_cb = lvgl_flush_cb;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);
  
  Logger.logLinef("Buffer: %d pixels (%d lines)", LVGL_BUFFER_SIZE, LVGL_BUFFER_SIZE / DISPLAY_WIDTH);
  Logger.logEnd();
  
  // Create test screen
  create_test_screen();
}

void create_test_screen() {
  // Create main screen container
  lv_obj_t *scr = lv_obj_create(NULL);
  lv_scr_load(scr);
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
  
  // Title banner with navy background
  lv_obj_t *title_banner = lv_obj_create(scr);
  lv_obj_set_size(title_banner, DISPLAY_WIDTH, 40);
  lv_obj_set_pos(title_banner, 0, 0);
  lv_obj_set_style_bg_color(title_banner, lv_color_make(0, 0, 128), 0);
  lv_obj_set_style_border_width(title_banner, 0, 0);
  lv_obj_set_style_pad_all(title_banner, 0, 0);
  
  lv_obj_t *title = lv_label_create(title_banner);
  lv_label_set_text(title, "ESP32 Display Test");
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
  
  // Hello World label
  lv_obj_t *hello = lv_label_create(scr);
  lv_label_set_text(hello, "Hello World!");
  lv_obj_set_style_text_color(hello, lv_color_make(0, 255, 0), 0);
  lv_obj_set_style_text_font(hello, &lv_font_montserrat_24, 0);
  lv_obj_set_pos(hello, 20, 55);
  
  // Device info
  char info_text[128];
  snprintf(info_text, sizeof(info_text), "Firmware: v%s\nChip: %s Rev %d", 
           FIRMWARE_VERSION, ESP.getChipModel(), ESP.getChipRevision());
  lv_obj_t *info = lv_label_create(scr);
  lv_label_set_text(info, info_text);
  lv_obj_set_style_text_color(info, lv_color_make(0, 255, 255), 0);
  lv_obj_set_pos(info, 20, 95);
  
  // Color test bars (RGB)
  int barHeight = 20;
  int yStart = 135;
  
  lv_obj_t *red_bar = lv_obj_create(scr);
  lv_obj_set_size(red_bar, DISPLAY_WIDTH/3, barHeight);
  lv_obj_set_pos(red_bar, 0, yStart);
  lv_obj_set_style_bg_color(red_bar, lv_color_make(255, 0, 0), 0);
  lv_obj_set_style_border_width(red_bar, 0, 0);
  
  lv_obj_t *green_bar = lv_obj_create(scr);
  lv_obj_set_size(green_bar, DISPLAY_WIDTH/3, barHeight);
  lv_obj_set_pos(green_bar, DISPLAY_WIDTH/3, yStart);
  lv_obj_set_style_bg_color(green_bar, lv_color_make(0, 255, 0), 0);
  lv_obj_set_style_border_width(green_bar, 0, 0);
  
  lv_obj_t *blue_bar = lv_obj_create(scr);
  lv_obj_set_size(blue_bar, DISPLAY_WIDTH/3, barHeight);
  lv_obj_set_pos(blue_bar, (DISPLAY_WIDTH/3)*2, yStart);
  lv_obj_set_style_bg_color(blue_bar, lv_color_make(0, 0, 255), 0);
  lv_obj_set_style_border_width(blue_bar, 0, 0);
  
  // Gradient label
  lv_obj_t *grad_label = lv_label_create(scr);
  lv_label_set_text(grad_label, "Grayscale Gradient (256 levels):");
  lv_obj_set_style_text_color(grad_label, lv_color_white(), 0);
  lv_obj_set_pos(grad_label, 10, yStart + barHeight + 8);
  
  // Grayscale gradient using individual rectangles (memory efficient)
  int gradientY = yStart + barHeight + 25;
  int gradientHeight = 30;
  
  // Draw gradient in 32 steps to reduce memory usage
  int numSteps = 32;
  int stepWidth = DISPLAY_WIDTH / numSteps;
  for (int i = 0; i < numSteps; i++) {
    uint8_t gray = map(i, 0, numSteps - 1, 0, 255);
    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_set_size(bar, stepWidth + 1, gradientHeight);  // +1 to avoid gaps
    lv_obj_set_pos(bar, i * stepWidth, gradientY);
    lv_obj_set_style_bg_color(bar, lv_color_make(gray, gray, gray), 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
  }
  
  // Border around gradient
  lv_obj_t *grad_border = lv_obj_create(scr);
  lv_obj_set_size(grad_border, DISPLAY_WIDTH, gradientHeight + 2);
  lv_obj_set_pos(grad_border, 0, gradientY - 1);
  lv_obj_set_style_bg_opa(grad_border, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(grad_border, lv_color_white(), 0);
  lv_obj_set_style_border_width(grad_border, 1, 0);
  
  // Board variant footer
  #if defined(BOARD_CYD2USB_V2)
  const char* variant = "CYD v2 (1 USB)";
  #elif defined(BOARD_CYD2USB_V3)
  const char* variant = "CYD v3 (2 USB)";
  #else
  const char* variant = "Unknown";
  #endif
  
  lv_obj_t *footer = lv_label_create(scr);
  lv_label_set_text_fmt(footer, "Board: %s", variant);
  lv_obj_set_style_text_color(footer, lv_color_make(255, 255, 0), 0);
  lv_obj_set_pos(footer, 10, DISPLAY_HEIGHT - 15);
}

#endif // HAS_DISPLAY

// WiFi event handlers for connection lifecycle monitoring
void onWiFiConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Logger.logMessage("WiFi", "Connected to AP - waiting for IP");
}

void onWiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info) {
  Logger.logMessagef("WiFi", "Got IP: %s", WiFi.localIP().toString().c_str());
}

void onWiFiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  uint8_t reason = info.wifi_sta_disconnected.reason;
  Logger.logMessagef("WiFi", "Disconnected - reason: %d", reason);
  
  // Common disconnect reasons:
  // 2 = AUTH_EXPIRE, 3 = AUTH_LEAVE, 4 = ASSOC_EXPIRE
  // 8 = ASSOC_LEAVE, 15 = 4WAY_HANDSHAKE_TIMEOUT
  // 201 = NO_AP_FOUND, 202 = AUTH_FAIL, 205 = HANDSHAKE_TIMEOUT
}


void setup()
{
  // Initialize log manager (wraps Serial for web streaming)
  Logger.begin(115200);
  delay(1000);
  
  // Register WiFi event handlers for connection lifecycle
  WiFi.onEvent(onWiFiConnected, ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(onWiFiGotIP, ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(onWiFiDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  
  Logger.logBegin("System Boot");
  Logger.logLinef("Firmware: v%s", FIRMWARE_VERSION);
  Logger.logLinef("Chip: %s (Rev %d)", ESP.getChipModel(), ESP.getChipRevision());
  Logger.logLinef("CPU: %d MHz", ESP.getCpuFreqMHz());
  Logger.logLinef("Flash: %d MB", ESP.getFlashChipSize() / (1024 * 1024));
  Logger.logLinef("MAC: %s", WiFi.macAddress().c_str());
  #if HAS_BUILTIN_LED
  Logger.logLinef("LED: GPIO%d (active %s)", LED_PIN, LED_ACTIVE_HIGH ? "HIGH" : "LOW");
  #endif
  // Example: Call board-specific function if available
  // #ifdef HAS_CUSTOM_IDENTIFIER
  // Logger.logLinef("Board: %s", board_get_custom_identifier());
  // #endif
  Logger.logEnd();
  
  // Initialize board-specific hardware
  #if HAS_BUILTIN_LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_ACTIVE_HIGH ? LOW : HIGH); // LED off initially
  #endif
  
  #if HAS_DISPLAY
  init_display();
  #endif
  
  // Initialize configuration manager
  config_manager_init();

  // Cache flash/sketch metadata early to avoid concurrent access from different tasks later
  // (e.g., MQTT publish + web API calls).
  device_telemetry_init();
  
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
    Logger.logMessage("Main", "No config - starting AP mode");
    web_portal_start_ap();
  } else {
    Logger.logMessage("Main", "Config loaded - connecting to WiFi");
    if (connect_wifi()) {
      start_mdns();
    } else {
      // Hard reset retry - WiFi hardware may be in bad state
      Logger.logMessage("Main", "WiFi failed - attempting hard reset");
      Logger.logBegin("WiFi Hard Reset");
      WiFi.mode(WIFI_OFF);
      delay(1000);  // Longer delay to fully reset hardware
      WiFi.mode(WIFI_STA);
      delay(500);
      Logger.logEnd("Reset complete");
      
      // One more attempt after hard reset
      if (connect_wifi()) {
        start_mdns();
      } else {
        Logger.logMessage("Main", "WiFi failed after reset - fallback to AP");
        web_portal_start_ap();
      }
    }
  }
  
  // Initialize web portal AFTER WiFi is started
  web_portal_init(&device_config);

  #if HAS_MQTT
  // Initialize MQTT manager (will only connect/publish when configured)
  char sanitized[CONFIG_DEVICE_NAME_MAX_LEN];
  config_manager_sanitize_device_name(device_config.device_name, sanitized, sizeof(sanitized));
  mqtt_manager.begin(&device_config, device_config.device_name, sanitized);
  #endif
  
  lastHeartbeat = millis();
  Logger.logMessage("Main", "Setup complete");
}

void loop()
{
  // Handle web portal (DNS for captive portal)
  web_portal_handle();

  #if HAS_MQTT
  mqtt_manager.loop();
  #endif
  
  #if HAS_DISPLAY
  lv_timer_handler();  // LVGL task handler
  #endif
  
  unsigned long currentMillis = millis();
  
  // WiFi watchdog - monitor connection and reconnect if needed
  // Only run if we're not in AP mode (AP mode is the fallback, should stay active)
  if (config_loaded && !web_portal_is_ap_mode() && currentMillis - lastWiFiCheck >= WIFI_CHECK_INTERVAL) {
    if (WiFi.status() != WL_CONNECTED && strlen(device_config.wifi_ssid) > 0) {
      Logger.logMessage("WiFi Watchdog", "Connection lost - attempting reconnect");
      if (connect_wifi()) {
        start_mdns();
      }
    }
    lastWiFiCheck = currentMillis;
  }
  
  // Check if it's time for heartbeat
  if (currentMillis - lastHeartbeat >= HEARTBEAT_INTERVAL) {
    if (WiFi.status() == WL_CONNECTED) {
      Logger.logQuickf("Heartbeat", "Up: %ds | Heap: %d | WiFi: %s (%s)", 
        currentMillis / 1000, ESP.getFreeHeap(), 
        WiFi.localIP().toString().c_str(), WiFi.getHostname());
    } else {
      Logger.logQuickf("Heartbeat", "Up: %ds | Heap: %d | WiFi: Disconnected", 
        currentMillis / 1000, ESP.getFreeHeap());
    }
    
    lastHeartbeat = currentMillis;
  }
  
  delay(10);
}

// Connect to WiFi with exponential backoff
bool connect_wifi() {
  Logger.logBegin("WiFi Connection");
  Logger.logLinef("SSID: %s", device_config.wifi_ssid);
  
  // === WiFi Hardware Reset Sequence ===
  // Disable persistent WiFi config (we manage our own via NVS)
  WiFi.persistent(false);
  
  // Full WiFi reset to clear stale state and prevent hardware corruption
  WiFi.disconnect(true);  // Disconnect + erase stored credentials
  delay(100);
  WiFi.mode(WIFI_OFF);    // Turn off WiFi hardware
  delay(500);             // Wait for hardware to settle
  WiFi.mode(WIFI_STA);    // Back to station mode
  delay(100);
  
  // Enable auto-reconnect at WiFi stack level
  WiFi.setAutoReconnect(true);
  
  // Prepare sanitized hostname
  char sanitized[CONFIG_DEVICE_NAME_MAX_LEN];
  config_manager_sanitize_device_name(device_config.device_name, sanitized, CONFIG_DEVICE_NAME_MAX_LEN);
  
  // Set WiFi hostname for mDNS and internal use
  // NOTE: ESP32's lwIP stack has limited DHCP Option 12 (hostname) support
  // The hostname may not always appear in router DHCP tables due to ESP-IDF/lwIP limitations
  // Use mDNS (.local) or NetBIOS for reliable device discovery instead
  if (strlen(sanitized) > 0) {
    // Set via WiFi library
    WiFi.setHostname(sanitized);
    Logger.logLinef("Hostname: %s", sanitized);
    
    // Also set via esp_netif API (for compatibility)
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif != NULL) {
      esp_netif_set_hostname(netif, sanitized);
    }
  }
  
  // Configure fixed IP if provided
  if (strlen(device_config.fixed_ip) > 0) {
    Logger.logBegin("Fixed IP Config");
    
    IPAddress local_ip, gateway, subnet, dns1, dns2;
    
    if (!local_ip.fromString(device_config.fixed_ip)) {
      Logger.logEnd("Invalid IP address");
      Logger.logEnd("Connection failed");
      return false;
    }
    
    if (!subnet.fromString(device_config.subnet_mask)) {
      Logger.logEnd("Invalid subnet mask");
      Logger.logEnd("Connection failed");
      return false;
    }
    
    if (!gateway.fromString(device_config.gateway)) {
      Logger.logEnd("Invalid gateway");
      Logger.logEnd("Connection failed");
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
      Logger.logEnd("Configuration failed");
      Logger.logEnd("Connection failed");
      return false;
    }
    
    Logger.logLinef("IP: %s", device_config.fixed_ip);
    Logger.logEnd();
  }
  
  WiFi.begin(device_config.wifi_ssid, device_config.wifi_password);
  
  // Try to connect with exponential backoff
  for (int attempt = 0; attempt < WIFI_MAX_ATTEMPTS; attempt++) {
    unsigned long backoff = WIFI_BACKOFF_BASE * (attempt + 1);
    unsigned long start = millis();
    
    Logger.logLinef("Attempt %d/%d (timeout %ds)", attempt + 1, WIFI_MAX_ATTEMPTS, backoff / 1000);
    
    while (millis() - start < backoff) {
      wl_status_t status = WiFi.status();
      if (status == WL_CONNECTED) {
        Logger.logLinef("IP: %s", WiFi.localIP().toString().c_str());
        Logger.logLinef("Hostname: %s", WiFi.getHostname());
        Logger.logLinef("MAC: %s", WiFi.macAddress().c_str());
        Logger.logLinef("Signal: %d dBm", WiFi.RSSI());
        Logger.logLine("");
        Logger.logLine("Access via:");
        Logger.logLinef("  http://%s", WiFi.localIP().toString().c_str());
        Logger.logLinef("  http://%s.local", WiFi.getHostname());
        Logger.logEnd("Connected");
        
        return true;
      }
      delay(100);
    }
    
    // Log detailed failure reason for diagnostics
    wl_status_t status = WiFi.status();
    if (status != WL_CONNECTED) {
      const char* reason = 
        (status == WL_NO_SSID_AVAIL) ? "SSID not found" :
        (status == WL_CONNECT_FAILED) ? "Connect failed (wrong password?)" :
        (status == WL_CONNECTION_LOST) ? "Connection lost" :
        (status == WL_DISCONNECTED) ? "Disconnected" :
        "Unknown";
      Logger.logLinef("Status: %s (%d)", reason, status);
    }
  }
  
  Logger.logEnd("All attempts failed");
  return false;
}

// Start mDNS service with enhanced TXT records
void start_mdns() {
  Logger.logBegin("mDNS");
  
  char sanitized[CONFIG_DEVICE_NAME_MAX_LEN];
  config_manager_sanitize_device_name(device_config.device_name, sanitized, CONFIG_DEVICE_NAME_MAX_LEN);
  
  if (strlen(sanitized) == 0) {
    Logger.logEnd("Empty hostname");
    return;
  }
  
  if (MDNS.begin(sanitized)) {
    Logger.logLinef("Name: %s.local", sanitized);
    
    // Add HTTP service
    MDNS.addService("http", "tcp", 80);
    
    // Add TXT records with device metadata (per RFC 6763)
    // Keep keys ≤9 chars, total TXT record <400 bytes
    
    // Core device identification
    MDNS.addServiceTxt("http", "tcp", "version", FIRMWARE_VERSION);
    MDNS.addServiceTxt("http", "tcp", "model", ESP.getChipModel());
    
    // MAC address (last 4 hex digits for identification)
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    String mac_short = mac.substring(mac.length() - 4);
    MDNS.addServiceTxt("http", "tcp", "mac", mac_short.c_str());
    
    // Device type and manufacturer
    MDNS.addServiceTxt("http", "tcp", "ty", "iot-device");
    MDNS.addServiceTxt("http", "tcp", "mf", "ESP32-Tmpl");
    
    // Capabilities
    MDNS.addServiceTxt("http", "tcp", "features", "wifi,http,api");
    
    // User-friendly description
    MDNS.addServiceTxt("http", "tcp", "note", "WiFi Portal Device");
    
    // Configuration URL
    String config_url = "http://";
    config_url += sanitized;
    config_url += ".local";
    MDNS.addServiceTxt("http", "tcp", "url", config_url.c_str());
    
    Logger.logLine("TXT records: version, model, mac, ty, features");
    Logger.logEnd();
  } else {
    Logger.logEnd("Failed to start");
  }
}