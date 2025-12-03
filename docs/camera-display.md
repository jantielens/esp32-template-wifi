# Camera Image Display Feature

Display camera snapshots from Home Assistant on your ESP32 device when person detection is triggered.

## Overview

This feature allows your ESP32 device to receive camera images via MQTT and display them on the screen. It's designed to work with Home Assistant automations that trigger on person detection from IP cameras.

## How It Works

### Architecture

```
Home Assistant → MQTT Broker → ESP32 MQTT Client → Camera Screen → Display
   (Person        (Message         (Download          (JPEG           (360x360
   Detection)     Queue)           Image)             Decode)         Screen)
```

### Data Flow

1. **Person Detection**: Home Assistant detects a person via camera/sensor
2. **MQTT Message**: HA publishes JSON message with image URL to MQTT topic
3. **ESP32 Receives**: MQTT client parses JSON and extracts URL
4. **HTTP Download**: Camera screen downloads JPEG from URL
5. **Display**: Image is decoded and displayed full-screen (center-crop)

### Message Format

The ESP32 expects a JSON message on the configured MQTT topic:

```json
{
  "url": "http://homeassistant.local:8123/api/camera_proxy/camera.front_door"
}
```

**Key Points:**
- Image must be JPEG format
- Max size: 1MB (safety limit)
- ESP32 downloads image via HTTP GET request
- Image is stored in PSRAM temporarily

## Implementation Details

### Components

#### 1. MQTT Client (`src/app/mqtt_client.cpp/h`)

**Features:**
- Connection management with auto-reconnect (5s interval)
- Subscribes to configurable topic (default: `homeassistant/camera/person_detected`)
- JSON payload parsing
- Callback system for message handling
- 512-byte buffer (sufficient for JSON with URL)

**Configuration:**
- Broker host/IP
- Port (default: 1883)
- Optional username/password
- Topic name
- Enable/disable flag

**Error Handling:**
- WiFi dependency check
- Connection state monitoring
- Detailed error logging with MQTT error codes
- Automatic reconnection on disconnect

#### 2. Camera Screen (`src/app/ui/screens/camera_screen.cpp/h`)

**Features:**
- Full-screen LVGL image widget (360x360)
- HTTP download with 10s timeout
- PSRAM memory allocation (dynamic sizing up to 1MB)
- LVGL's native JPEG decoder
- Loading indicator during download
- Error messages on failure

**Image Handling:**
- Downloads image via `HTTPClient`
- Allocates PSRAM buffer dynamically based on content-length
- Fallback to internal RAM if PSRAM unavailable
- LVGL `LV_IMG_CF_RAW` format for JPEG
- Center-crop scaling to fill 360x360 display
- Automatic cleanup on navigation away

**Memory Management:**
- Checks content-length before download
- Rejects images > 1MB
- Uses PSRAM to preserve internal RAM
- Frees buffer when screen exits or new image loads

#### 3. Configuration Storage (`src/app/config_manager.cpp/h`)

**MQTT Settings in NVS:**
```cpp
struct DeviceConfig {
    char mqtt_host[64];           // Broker hostname/IP
    uint16_t mqtt_port;           // Port (default: 1883)
    char mqtt_username[32];       // Optional credentials
    char mqtt_password[64];
    char mqtt_topic[128];         // Topic to subscribe to
    bool mqtt_enabled;            // Enable/disable MQTT
    // ... other settings
};
```

**Default Values:**
- Port: `1883`
- Topic: `homeassistant/camera/person_detected`
- Enabled: `false` (must be explicitly enabled)

#### 4. Screen Manager Integration (`src/app/ui/screen_manager.cpp/h`)

**Public API:**
```cpp
void ScreenManager::showCameraImageFromUrl(const char* url);
```

**Behavior:**
- Navigates to camera screen with fade-in animation
- Triggers async image download
- User can swipe away to return to previous screen
- Image persists until user navigates or new image arrives

#### 5. REST API Endpoint (`src/app/web_portal.cpp`)

**Endpoint:** `POST /api/display-camera`

**Request:**
```json
{
  "url": "http://example.com/image.jpg"
}
```

**Response:**
```json
{
  "success": true
}
```

**Use Cases:**
- Testing without MQTT
- Manual image display trigger
- Integration with other systems

## Usage

### Home Assistant Configuration

#### 1. MQTT Automation

Create an automation that publishes camera URLs when person is detected:

```yaml
automation:
  - alias: "Display Person Detection on ESP32"
    description: "Show camera snapshot on ESP32 when person detected"
    trigger:
      - platform: state
        entity_id: binary_sensor.front_door_person_detected
        to: 'on'
    action:
      - service: mqtt.publish
        data:
          topic: "homeassistant/camera/person_detected"
          payload: >
            {
              "url": "{{ state_attr('camera.front_door', 'entity_picture') }}"
            }
```

#### 2. Camera Proxy URL

Home Assistant provides camera proxy URLs that work reliably:

```
http://homeassistant.local:8123/api/camera_proxy/camera.front_door
```

**Example with Auth Token:**
```yaml
action:
  - service: mqtt.publish
    data:
      topic: "homeassistant/camera/person_detected"
      payload: >
        {
          "url": "http://homeassistant.local:8123/api/camera_proxy/camera.front_door?token={{ state_attr('camera.front_door', 'access_token') }}"
        }
```

#### 3. Multiple Cameras

Handle multiple camera triggers:

```yaml
automation:
  - alias: "Display Any Camera Detection"
    trigger:
      - platform: state
        entity_id:
          - binary_sensor.front_door_person_detected
          - binary_sensor.back_yard_person_detected
          - binary_sensor.driveway_person_detected
        to: 'on'
    action:
      - service: mqtt.publish
        data:
          topic: "homeassistant/camera/person_detected"
          payload: >
            {
              "url": "http://homeassistant.local:8123/api/camera_proxy/{{ trigger.to_state.attributes.camera_entity }}"
            }
```

### ESP32 Configuration

#### Configure MQTT Settings (via REST API)

Currently MQTT settings must be configured via REST API:

```bash
# Get current config
curl http://esp32.local/api/config

# Update config with MQTT settings
curl -X POST http://esp32.local/api/config \
  -H "Content-Type: application/json" \
  -d '{
    "wifi_ssid": "YourNetwork",
    "wifi_password": "password",
    "device_name": "ESP32 Display",
    "mqtt_host": "homeassistant.local",
    "mqtt_port": 1883,
    "mqtt_username": "",
    "mqtt_password": "",
    "mqtt_topic": "homeassistant/camera/person_detected",
    "mqtt_enabled": true
  }'
```

**Note:** Device will reboot after configuration save.

### Testing

#### Test via REST API (Without MQTT)

```bash
# Display an image directly
curl -X POST http://esp32.local/api/display-camera \
  -H "Content-Type: application/json" \
  -d '{
    "url": "http://homeassistant.local:8123/api/camera_proxy/camera.front_door"
  }'
```

#### Test via MQTT

```bash
# Publish test message to MQTT broker
mosquitto_pub -h homeassistant.local -t "homeassistant/camera/person_detected" \
  -m '{"url":"http://homeassistant.local:8123/api/camera_proxy/camera.front_door"}'
```

## Image Requirements

### Recommended Specifications

- **Format:** JPEG (required)
- **Size:** < 1MB (hard limit)
- **Resolution:** 300KB typical (300x300 to 640x640 recommended)
- **Quality:** 70-80% JPEG quality is sufficient

### Image Optimization in Home Assistant

Home Assistant can automatically resize/compress images:

```yaml
# In configuration.yaml
camera:
  - platform: generic
    still_image_url: http://camera.local/snapshot.jpg
    name: Front Door Camera
    # These settings help reduce image size
    verify_ssl: false
    
# Use image_processing to resize
image_processing:
  - platform: generic
    source:
      - entity_id: camera.front_door
    # Resize to 360x360 before sending to ESP32
```

### Handling Large Images

If images exceed 1MB:

1. **Compress in Home Assistant:** Use automation to resize/compress before sending
2. **Adjust ESP32 limit:** Modify `MAX_IMAGE_SIZE` in `camera_screen.cpp` (not recommended)
3. **Use thumbnail URLs:** Many IP cameras provide thumbnail endpoints

## Troubleshooting

### MQTT Connection Issues

**Symptom:** ESP32 not receiving messages

**Check:**
1. MQTT broker accessible from ESP32 IP
2. MQTT credentials correct (if authentication enabled)
3. Topic name matches exactly (case-sensitive)
4. MQTT enabled in config (`mqtt_enabled: true`)

**Debug:**
```bash
# Monitor serial output for MQTT logs
./monitor.sh

# Look for:
# "MQTT Init" - Initialization successful
# "MQTT Connect" - Connection attempts
# "MQTT Connected!" - Connection successful
# "Subscribed successfully" - Topic subscription confirmed
```

### Image Download Failures

**Symptom:** Loading screen but no image appears

**Common Causes:**
1. **URL not accessible:** ESP32 can't reach the URL
   - Check firewall rules
   - Ensure HA allows connections from ESP32 IP
   
2. **Image too large:** > 1MB rejected
   - Check serial log for "Image too large" error
   - Compress image in Home Assistant
   
3. **Wrong format:** Not JPEG
   - Verify image format with `curl -I <url>`
   - Convert to JPEG in Home Assistant

4. **Timeout:** Download takes > 10 seconds
   - Check network speed
   - Use smaller images
   - Check WiFi signal strength

**Debug:**
```bash
# Test URL directly
curl -o /tmp/test.jpg "http://homeassistant.local:8123/api/camera_proxy/camera.front_door"

# Check image size
ls -lh /tmp/test.jpg

# Verify it's JPEG
file /tmp/test.jpg
```

### Memory Issues

**Symptom:** ESP32 reboots or crashes when displaying images

**Check:**
1. PSRAM enabled in board config
2. Image size within limits
3. Available heap memory

**Debug:**
```bash
# Monitor heap in serial output
# Look for "Heartbeat" logs showing free heap

# Check PSRAM allocation in logs:
# "PSRAM allocation failed" - PSRAM not available
# "Memory allocation failed" - Out of memory
```

## Performance

### Memory Usage

- **MQTT Client:** ~2KB static + 512 bytes buffer
- **Image Buffer:** Dynamic (matches image size, max 1MB PSRAM)
- **LVGL Screen:** ~5KB static structures
- **Total Typical:** ~250KB for 300KB JPEG

### Network Usage

- **MQTT Message:** < 1KB (just URL)
- **Image Download:** 100-500KB typical
- **Total per trigger:** ~300KB average

### Display Performance

- **Navigation to screen:** ~300ms fade-in
- **Download time:** 1-3s (depends on image size and network)
- **JPEG decode:** ~100-200ms (LVGL hardware-accelerated)
- **Total time to display:** 2-4s typical

## Future Enhancements

### Potential Improvements

1. **Web UI Configuration**
   - Add MQTT settings form to web portal
   - Test connection button
   - Topic validator

2. **Image Caching**
   - Store last N images in SPIFFS
   - Display cached image while downloading new one
   - Slideshow mode

3. **Motion Indicators**
   - Overlay timestamp/camera name
   - Animated border for live detection
   - Auto-dismiss after timeout

4. **Multi-Camera Support**
   - Queue multiple camera triggers
   - Slideshow through recent detections
   - Split-screen for multiple cameras

5. **Advanced Features**
   - Face recognition integration
   - Motion detection zones overlay
   - Recording trigger to SD card

## API Reference

### MQTT Client Functions

```cpp
// Initialize MQTT with configuration
bool mqtt_client_init(const DeviceConfig* config);

// Set callback for received messages
void mqtt_client_set_callback(MqttMessageCallback callback);

// Connect to broker (call after WiFi connected)
bool mqtt_client_connect();

// Disconnect from broker
void mqtt_client_disconnect();

// Check connection status
bool mqtt_client_is_connected();

// Process messages (call in main loop)
void mqtt_client_loop();

// Get status string for debugging
const char* mqtt_client_get_status();
```

### Screen Manager Functions

```cpp
// Display camera image from URL
void ScreenManager::showCameraImageFromUrl(const char* url);
```

### Camera Screen Functions

```cpp
// Load and display image from URL
bool CameraScreen::loadImageFromUrl(const char* url);

// Clear current image and free memory
void CameraScreen::clearImage();
```

## Related Documentation

- [Web Portal API](web-portal.md) - REST API endpoints
- [Configuration Management](library-management.md) - NVS storage system
- [Build Process](build-and-release-process.md) - Compilation and deployment

## Version History

- **v1.0** (2025-11-30)
  - Initial implementation
  - MQTT client with auto-reconnect
  - HTTP image download
  - JPEG display on 360x360 screen
  - REST API endpoint for testing
