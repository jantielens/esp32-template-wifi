# Compile-Time Flags Report

This document is a template. Sections marked with `COMPILE_FLAG_REPORT` markers are auto-updated by `tools/compile_flags_report.py`.

## How to update

- Update this doc:
  - `python3 tools/compile_flags_report.py md --out docs/compile-time-flags.md`
- Print active flags during a build (example):
  - `python3 tools/compile_flags_report.py build --board cyd-v2`

## Build system notes

- Some Arduino libraries are compiled as separate translation units and may not see macros that are only defined in `src/boards/<board>/board_overrides.h`.
- The build script propagates a small allowlist of board override defines into global compiler flags so libraries are compiled with the same values.
  - Currently allowlisted:
    - `CONFIG_ASYNC_TCP_STACK_SIZE`
    - TFT_eSPI essentials for clean/CI builds (pins + SPI frequencies + controller/bus flags)
  - For TFT_eSPI specifically, `build.sh` also supports a per-board `src/boards/<board>/User_Setup.h` which is force-included for that board (so the build does not depend on a locally modified Arduino library install).

## Flags (generated)

<!-- BEGIN COMPILE_FLAG_REPORT:FLAGS -->
Total flags: 117

### Features (HAS_*)

- **HAS_BACKLIGHT** default: `false` — Enable backlight control (typically via PWM).
- **HAS_BLE** default: `false` — Enable BLE (NimBLE) advertising support.
- **HAS_BUILTIN_LED** default: `false` — Enable built-in status LED support.
- **HAS_BUTTON** default: `false` — User Button (optional)
- **HAS_DISPLAY** default: `false` — Enable display + LVGL UI support.
- **HAS_IMAGE_API** default: `false` — Enable Image API endpoints (JPEG upload/download/display).
- **HAS_MQTT** default: `true` — Enable MQTT and Home Assistant integration.
- **HAS_SENSOR_BME280** default: `false` — Enable BME280 (I2C) environmental sensor adapter.
- **HAS_SENSOR_DUMMY** default: `false` — Enable dummy sensor adapter (synthetic values for testing).
- **HAS_SENSOR_LD2410_OUT** default: `false` — Enable LD2410 OUT pin presence sensor adapter.
- **HAS_TOUCH** default: `false` — Enable touch input support.

### Selectors (*_DRIVER)

- **DISPLAY_DRIVER** default: `DISPLAY_DRIVER_TFT_ESPI` (values: DISPLAY_DRIVER_ARDUINO_GFX, DISPLAY_DRIVER_ESP_PANEL, DISPLAY_DRIVER_ST7789V2, DISPLAY_DRIVER_TFT_ESPI) — Select the display HAL backend (one of the DISPLAY_DRIVER_* constants).
- **ILI9341_2_DRIVER** default: `(no default)` — These macros are consumed by the TFT_eSPI library itself.
- **TOUCH_DRIVER** default: `TOUCH_DRIVER_XPT2046` (values: TOUCH_DRIVER_AXS15231B, TOUCH_DRIVER_CST816S_ESP_PANEL, TOUCH_DRIVER_XPT2046) — Select the touch HAL backend (one of the TOUCH_DRIVER_* constants).

### Hardware (Geometry)

- **DISPLAY_HEIGHT** default: `(no default)` — Panel height in pixels.
- **DISPLAY_ROTATION** default: `(no default)` — UI rotation (LVGL).
- **DISPLAY_WIDTH** default: `(no default)` — Panel width in pixels.

### Hardware (Pins)

- **BUTTON_PIN** default: `0` — GPIO pin for the optional user button (active level defined below).
- **LCD_BL_PIN** default: `(no default)` — LCD backlight pin.
- **LCD_CS_PIN** default: `(no default)` — LCD SPI CS pin.
- **LCD_DC_PIN** default: `(no default)` — LCD SPI DC pin.
- **LCD_MOSI_PIN** default: `(no default)` — LCD SPI MOSI pin.
- **LCD_QSPI_CS** default: `(no default)` — QSPI chip select pin.
- **LCD_QSPI_D0** default: `(no default)` — QSPI data line 0 pin.
- **LCD_QSPI_D1** default: `(no default)` — QSPI data line 1 pin.
- **LCD_QSPI_D2** default: `(no default)` — QSPI data line 2 pin.
- **LCD_QSPI_D3** default: `(no default)` — QSPI data line 3 pin.
- **LCD_QSPI_PCLK** default: `(no default)` — QSPI pixel clock pin.
- **LCD_QSPI_RST** default: `(no default)` — QSPI reset pin (-1 = none).
- **LCD_QSPI_TE** default: `(no default)` — Panel TE pin.
- **LCD_RST_PIN** default: `(no default)` — LCD reset pin.
- **LCD_SCK_PIN** default: `(no default)` — LCD SPI SCK pin.
- **LD2410_OUT_PIN** default: `-1` — LD2410 OUT pin (presence). Use -1 to disable.
- **LED_PIN** default: `2` — GPIO for the built-in LED (only used when HAS_BUILTIN_LED is true).
- **SENSOR_I2C_SCL** default: `-1` — I2C SCL pin for sensors.
- **SENSOR_I2C_SDA** default: `-1` — I2C pins for sensors. Use -1 to keep default Wire pins.
- **TFT_BL** default: `(no default)` — TFT_eSPI: backlight pin.
- **TFT_CS** default: `(no default)` — TFT_eSPI: CS pin.
- **TFT_DC** default: `(no default)` — TFT_eSPI: DC pin.
- **TFT_MISO** default: `(no default)` — TFT_eSPI: MISO pin.
- **TFT_MOSI** default: `(no default)` — TFT_eSPI: MOSI pin.
- **TFT_RST** default: `(no default)` — TFT_eSPI: RST pin (-1 = none).
- **TFT_SCK** default: `(no default)` — QSPI clock pin.
- **TFT_SCLK** default: `(no default)` — TFT_eSPI: SCLK pin.
- **TFT_SDA0** default: `(no default)` — QSPI data line 0 pin.
- **TFT_SDA1** default: `(no default)` — QSPI data line 1 pin.
- **TFT_SDA2** default: `(no default)` — QSPI data line 2 pin.
- **TFT_SDA3** default: `(no default)` — QSPI data line 3 pin.
- **TOUCH_CS** default: `(no default)` — TFT_eSPI touch: CS pin.
- **TOUCH_I2C_SCL** default: `(no default)` — I2C SCL pin.
- **TOUCH_I2C_SDA** default: `(no default)` — I2C SDA pin.
- **TOUCH_INT** default: `(no default)` — Touch interrupt pin.
- **TOUCH_IRQ** default: `(no default)` — TFT_eSPI touch: IRQ pin (optional).
- **TOUCH_MISO** default: `(no default)` — TFT_eSPI touch: MISO pin.
- **TOUCH_MOSI** default: `(no default)` — TFT_eSPI touch: MOSI pin.
- **TOUCH_RST** default: `(no default)` — Touch reset pin.
- **TOUCH_SCLK** default: `(no default)` — TFT_eSPI touch: SCLK pin.
- **XPT2046_CLK** default: `(no default)` — XPT2046 CLK pin.
- **XPT2046_CS** default: `(no default)` — XPT2046 CS pin.
- **XPT2046_IRQ** default: `(no default)` — XPT2046 IRQ pin.
- **XPT2046_MISO** default: `(no default)` — XPT2046 MISO pin.
- **XPT2046_MOSI** default: `(no default)` — XPT2046 MOSI pin.

### Limits & Tuning

- **CONFIG_BT_NIMBLE_MAX_BONDS** default: `(no default)` — NimBLE max bonded devices (tuning for small footprint)
- **CONFIG_BT_NIMBLE_MAX_CCCDS** default: `(no default)` — NimBLE max CCCDs
- **CONFIG_BT_NIMBLE_MAX_CONNECTIONS** default: `(no default)` — NimBLE max connections
- **HEALTH_HISTORY_PERIOD_MS** default: `5000` — Sampling cadence for the device-side history (ms). Default aligns with UI poll.
- **IMAGE_API_DECODE_HEADROOM_BYTES** default: `(50 * 1024)` — Extra free RAM required for decoding (bytes).
- **IMAGE_API_DEFAULT_TIMEOUT_MS** default: `10000` — Default image display timeout in milliseconds.
- **IMAGE_API_MAX_SIZE_BYTES** default: `(100 * 1024)` — Max bytes accepted for full image uploads (JPEG).
- **IMAGE_API_MAX_TIMEOUT_MS** default: `(86400UL * 1000UL)` — Maximum image display timeout in milliseconds.
- **IMAGE_STRIP_BATCH_MAX_ROWS** default: `16` — Max rows batched per LCD transaction when decoding JPEG strips.
- **LVGL_BUFFER_PREFER_INTERNAL** default: `false` — Prefer internal RAM over PSRAM for LVGL draw buffer allocation.
- **LVGL_BUFFER_SIZE** default: `(DISPLAY_WIDTH * 10)` — LVGL draw buffer size in pixels (larger = faster, more RAM).
- **LVGL_TICK_PERIOD_MS** default: `5` — LVGL tick period in milliseconds.
- **MEMORY_TRIPWIRE_INTERNAL_MIN_BYTES** default: `0` — Default: disabled (0). Enable per-board if you want early warning logs.
- **SENSOR_I2C_FREQUENCY** default: `400000` — I2C clock for sensors (Hz).
- **SPI_FREQUENCY** default: `(no default)` — TFT_eSPI: SPI write frequency (Hz).
- **SPI_READ_FREQUENCY** default: `(no default)` — TFT_eSPI: SPI read frequency (Hz).
- **SPI_TOUCH_FREQUENCY** default: `(no default)` — TFT_eSPI: SPI touch frequency (Hz).
- **TFT_SPI_FREQUENCY** default: `(no default)` — TFT SPI clock frequency.
- **TFT_SPI_FREQ_HZ** default: `(no default)` — QSPI clock frequency (Hz).
- **TOUCH_I2C_FREQ_HZ** default: `(no default)` — I2C frequency (Hz).
- **WEB_PORTAL_CONFIG_BODY_TIMEOUT_MS** default: `5000` — Timeout for an incomplete /api/config upload (ms) before freeing the buffer.
- **WEB_PORTAL_CONFIG_MAX_JSON_BYTES** default: `4096` — Max JSON body size accepted by /api/config.
- **WIFI_MAX_ATTEMPTS** default: `3` — Maximum WiFi connection attempts at boot before falling back.

### Other

- **BME280_I2C_ADDR** default: `0x76` — BME280 I2C address (0x76 or 0x77).
- **BUTTON_ACTIVE_LOW** default: `true` — Button polarity: true when pressed = LOW.
- **CONFIG_BT_NIMBLE_LOG_LEVEL** default: `(no default)` — NimBLE host log level
- **CONFIG_BT_NIMBLE_MSYS1_BLOCK_COUNT** default: `(no default)` — NimBLE msys1 block count
- **CONFIG_BT_NIMBLE_ROLE_BROADCASTER** default: `(no default)` — NimBLE role: broadcaster
- **CONFIG_BT_NIMBLE_ROLE_CENTRAL** default: `(no default)` — NimBLE role: central
- **CONFIG_BT_NIMBLE_ROLE_OBSERVER** default: `(no default)` — NimBLE role: observer
- **CONFIG_BT_NIMBLE_ROLE_PERIPHERAL** default: `(no default)` — NimBLE role: peripheral (required)
- **CONFIG_NIMBLE_CPP_LOG_LEVEL** default: `(no default)` — NimBLE C++ wrapper log level
- **DISPLAY_COLOR_ORDER_BGR** default: `(no default)` — Panel uses BGR byte order.
- **DISPLAY_DRIVER_ILI9341_2** default: `(no default)` — Use the ILI9341_2 controller setup in TFT_eSPI.
- **DISPLAY_INVERSION_ON** default: `(no default)` — Enable display inversion (panel-specific).
- **DISPLAY_NEEDS_GAMMA_FIX** default: `(no default)` — Apply gamma correction fix for this panel variant.
- **ESP_PANEL_SWAPBUF_PREFER_INTERNAL** default: `true` — Default: true. Some panel buses are more reliable with internal/DMA-capable buffers.
- **HEALTH_HISTORY_ENABLED** default: `1` — Default: enabled.
- **HEALTH_HISTORY_SAMPLES** default: `((HEALTH_HISTORY_SECONDS * 1000) / HEALTH_HISTORY_PERIOD_MS)` — Derived number of samples.
- **HEALTH_HISTORY_SECONDS** default: `300` — How much client-side history (sparklines) to keep.
- **HEALTH_POLL_INTERVAL_MS** default: `5000` — How often the web UI polls /api/health.
- **LCD_QSPI_HOST** default: `(no default)` — QSPI host peripheral.
- **LD2410_OUT_DEBOUNCE_MS** default: `50` — Debounce for LD2410 OUT edge changes (ms).
- **LED_ACTIVE_HIGH** default: `true` — LED polarity: true if HIGH turns the LED on.
- **MEMORY_TRIPWIRE_CHECK_INTERVAL_MS** default: `5000` — How often to check tripwires from the main loop.
- **POWERON_CONFIG_BURST_ENABLED** default: `false` — Intended for boards WITHOUT a reliable user button.
- **PROJECT_DISPLAY_NAME** default: `"ESP32 Device"` — Human-friendly project name used in the web UI and device name (can be set by build system).
- **TFT_BACKLIGHT_ON** default: `(no default)` — Backlight "on" level.
- **TFT_BACKLIGHT_PWM_CHANNEL** default: `0` — LEDC channel used for backlight PWM.
- **TOUCH_CAL_X_MAX** default: `(no default)` — Touch calibration: X maximum.
- **TOUCH_CAL_X_MIN** default: `(no default)` — Touch calibration: X minimum.
- **TOUCH_CAL_Y_MAX** default: `(no default)` — Touch calibration: Y maximum.
- **TOUCH_CAL_Y_MIN** default: `(no default)` — Touch calibration: Y minimum.
- **TOUCH_I2C_PORT** default: `(no default)` — I2C controller index.
- **USE_HSPI_PORT** default: `(no default)` — CYD uses HSPI for the display.
<!-- END COMPILE_FLAG_REPORT:FLAGS -->

## Board Matrix: Features (generated)

Legend: ✅ = enabled/true, blank = disabled/false, ? = unknown/undefined

<!-- BEGIN COMPILE_FLAG_REPORT:MATRIX_FEATURES -->
| board-name | HAS_BACKLIGHT | HAS_BLE | HAS_BUILTIN_LED | HAS_BUTTON | HAS_DISPLAY | HAS_IMAGE_API | HAS_MQTT | HAS_SENSOR_BME280 | HAS_SENSOR_DUMMY | HAS_SENSOR_LD2410_OUT | HAS_TOUCH |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| esp32-nodisplay |  |  |  |  |  |  | ✅ |  |  |  |  |
| cyd-v2 | ✅ |  |  |  | ✅ | ✅ | ✅ |  |  |  | ✅ |
| esp32c3-waveshare-169-st7789v2 | ✅ |  | ✅ |  | ✅ | ✅ | ✅ |  |  |  |  |
| jc3248w535 | ✅ |  |  |  | ✅ | ✅ | ✅ |  |  |  | ✅ |
| jc3636w518 | ✅ |  |  |  | ✅ | ✅ | ✅ |  |  |  | ✅ |
| esp32c3-withsensors |  | ✅ |  | ✅ |  |  | ✅ |  | ✅ |  |  |
<!-- END COMPILE_FLAG_REPORT:MATRIX_FEATURES -->

## Board Matrix: Selectors (generated)

<!-- BEGIN COMPILE_FLAG_REPORT:MATRIX_SELECTORS -->
| board-name | DISPLAY_DRIVER | TOUCH_DRIVER |
| --- | --- | --- |
| esp32-nodisplay | — | — |
| cyd-v2 | DISPLAY_DRIVER_TFT_ESPI | TOUCH_DRIVER_XPT2046 |
| esp32c3-waveshare-169-st7789v2 | DISPLAY_DRIVER_ST7789V2 | — |
| jc3248w535 | DISPLAY_DRIVER_ARDUINO_GFX | TOUCH_DRIVER_AXS15231B |
| jc3636w518 | DISPLAY_DRIVER_ESP_PANEL | TOUCH_DRIVER_CST816S_ESP_PANEL |
| esp32c3-withsensors | — | — |
<!-- END COMPILE_FLAG_REPORT:MATRIX_SELECTORS -->

## Usage Map (preprocessor only, generated)

<!-- BEGIN COMPILE_FLAG_REPORT:USAGE -->
- **HAS_BACKLIGHT**
  - src/app/app.ino
  - src/app/board_config.h
  - src/app/display_manager.cpp
  - src/app/drivers/arduino_gfx_driver.cpp
  - src/app/drivers/tft_espi_driver.cpp
- **HAS_BLE**
  - src/app/app.ino
  - src/app/board_config.h
- **HAS_BUILTIN_LED**
  - src/app/app.ino
  - src/app/board_config.h
- **HAS_BUTTON**
  - src/app/app.ino
  - src/app/board_config.h
- **HAS_DISPLAY**
  - src/app/app.ino
  - src/app/board_config.h
  - src/app/config_manager.cpp
  - src/app/config_manager.h
  - src/app/device_telemetry.cpp
  - src/app/display_drivers.cpp
  - src/app/display_manager.cpp
  - src/app/ha_discovery.cpp
  - src/app/image_api.cpp
  - src/app/lvgl_jpeg_decoder.cpp
  - src/app/lvgl_jpeg_decoder.h
  - src/app/screen_saver_manager.cpp
  - src/app/screen_saver_manager.h
  - src/app/screens.cpp
  - src/app/screens/lvgl_image_screen.cpp
  - src/app/screens/lvgl_image_screen.h
  - src/app/touch_manager.cpp
  - src/app/web_portal.cpp
  - src/app/web_portal_config.cpp
  - src/app/web_portal_device_api.cpp
  - src/app/web_portal_display.cpp
  - src/app/web_portal_display.h
  - src/app/web_portal_routes.cpp
- **HAS_IMAGE_API**
  - src/app/app.ino
  - src/app/board_config.h
  - src/app/display_manager.cpp
  - src/app/display_manager.h
  - src/app/image_api.cpp
  - src/app/image_api.h
  - src/app/jpeg_preflight.cpp
  - src/app/jpeg_preflight.h
  - src/app/lv_conf.h
  - src/app/lvgl_jpeg_decoder.cpp
  - src/app/lvgl_jpeg_decoder.h
  - src/app/screens.cpp
  - src/app/screens/direct_image_screen.cpp
  - src/app/screens/direct_image_screen.h
  - src/app/screens/lvgl_image_screen.cpp
  - src/app/screens/lvgl_image_screen.h
  - src/app/strip_decoder.cpp
  - src/app/strip_decoder.h
  - src/app/web_portal.cpp
  - src/app/web_portal.h
- **HAS_MQTT**
  - src/app/app.ino
  - src/app/board_config.h
  - src/app/config_manager.cpp
  - src/app/device_telemetry.cpp
  - src/app/ha_discovery.cpp
  - src/app/ha_discovery.h
  - src/app/mqtt_manager.cpp
  - src/app/mqtt_manager.h
  - src/app/sensors/bme280_sensor.cpp
  - src/app/sensors/bme280_sensor.h
  - src/app/sensors/ld2410_out_sensor.cpp
  - src/app/sensors/sensor_manager.cpp
  - src/app/sensors/sensor_manager.h
- **HAS_SENSOR_BME280**
  - src/app/board_config.h
  - src/app/sensors.cpp
  - src/app/sensors/bme280_sensor.cpp
- **HAS_SENSOR_DUMMY**
  - src/app/board_config.h
  - src/app/sensors.cpp
- **HAS_SENSOR_LD2410_OUT**
  - src/app/board_config.h
  - src/app/sensors.cpp
  - src/app/sensors/ld2410_out_sensor.cpp
- **HAS_TOUCH**
  - src/app/app.ino
  - src/app/board_config.h
  - src/app/config_manager.cpp
  - src/app/screen_saver_manager.cpp
  - src/app/touch_drivers.cpp
  - src/app/touch_manager.cpp
  - src/app/touch_manager.h
- **DISPLAY_DRIVER**
  - src/app/board_config.h
  - src/app/display_drivers.cpp
  - src/app/display_manager.cpp
- **TOUCH_DRIVER**
  - src/app/board_config.h
  - src/app/touch_drivers.cpp
  - src/app/touch_manager.cpp
- **BME280_I2C_ADDR**
  - src/app/board_config.h
- **BUTTON_ACTIVE_LOW**
  - src/app/board_config.h
- **BUTTON_PIN**
  - src/app/board_config.h
- **DISPLAY_INVERSION_ON**
  - src/app/drivers/tft_espi_driver.cpp
- **DISPLAY_NEEDS_GAMMA_FIX**
  - src/app/drivers/tft_espi_driver.cpp
- **DISPLAY_ROTATION**
  - src/app/touch_manager.cpp
- **ESP_PANEL_SWAPBUF_PREFER_INTERNAL**
  - src/app/board_config.h
- **HEALTH_HISTORY_ENABLED**
  - src/app/app.ino
  - src/app/board_config.h
  - src/app/health_history.cpp
  - src/app/web_portal_device_api.cpp
  - src/app/web_portal_routes.cpp
- **HEALTH_HISTORY_PERIOD_MS**
  - src/app/board_config.h
- **HEALTH_HISTORY_SAMPLES**
  - src/app/board_config.h
- **HEALTH_HISTORY_SECONDS**
  - src/app/board_config.h
- **HEALTH_POLL_INTERVAL_MS**
  - src/app/board_config.h
- **IMAGE_API_DECODE_HEADROOM_BYTES**
  - src/app/board_config.h
- **IMAGE_API_DEFAULT_TIMEOUT_MS**
  - src/app/board_config.h
- **IMAGE_API_MAX_SIZE_BYTES**
  - src/app/board_config.h
- **IMAGE_API_MAX_TIMEOUT_MS**
  - src/app/board_config.h
- **IMAGE_STRIP_BATCH_MAX_ROWS**
  - src/app/board_config.h
- **LCD_BL_PIN**
  - src/app/drivers/arduino_gfx_driver.cpp
- **LCD_QSPI_CS**
  - src/app/drivers/arduino_gfx_driver.cpp
- **LD2410_OUT_DEBOUNCE_MS**
  - src/app/board_config.h
- **LD2410_OUT_PIN**
  - src/app/board_config.h
- **LED_ACTIVE_HIGH**
  - src/app/board_config.h
- **LED_PIN**
  - src/app/board_config.h
- **LVGL_BUFFER_PREFER_INTERNAL**
  - src/app/board_config.h
- **LVGL_BUFFER_SIZE**
  - src/app/board_config.h
- **LVGL_TICK_PERIOD_MS**
  - src/app/board_config.h
- **MEMORY_TRIPWIRE_CHECK_INTERVAL_MS**
  - src/app/board_config.h
- **MEMORY_TRIPWIRE_INTERNAL_MIN_BYTES**
  - src/app/board_config.h
  - src/app/device_telemetry.cpp
- **POWERON_CONFIG_BURST_ENABLED**
  - src/app/board_config.h
- **PROJECT_DISPLAY_NAME**
  - src/app/board_config.h
- **SENSOR_I2C_FREQUENCY**
  - src/app/board_config.h
- **SENSOR_I2C_SCL**
  - src/app/board_config.h
- **SENSOR_I2C_SDA**
  - src/app/board_config.h
- **TFT_BACKLIGHT_ON**
  - src/app/drivers/arduino_gfx_driver.cpp
  - src/app/drivers/tft_espi_driver.cpp
- **TFT_BACKLIGHT_PWM_CHANNEL**
  - src/app/board_config.h
- **TFT_BL**
  - src/app/drivers/tft_espi_driver.cpp
- **TFT_SPI_FREQ_HZ**
  - src/app/drivers/esp_panel_st77916_driver.cpp
- **TOUCH_CAL_X_MAX**
  - src/app/touch_manager.cpp
- **TOUCH_CAL_X_MIN**
  - src/app/touch_manager.cpp
- **TOUCH_CAL_Y_MAX**
  - src/app/touch_manager.cpp
- **TOUCH_CAL_Y_MIN**
  - src/app/touch_manager.cpp
- **TOUCH_I2C_SCL**
  - src/app/drivers/axs15231b_touch_driver.cpp
- **TOUCH_INT**
  - src/app/drivers/axs15231b_touch_driver.cpp
- **TOUCH_MISO**
  - src/app/drivers/xpt2046_driver.cpp
- **TOUCH_MOSI**
  - src/app/drivers/xpt2046_driver.cpp
- **TOUCH_SCLK**
  - src/app/drivers/xpt2046_driver.cpp
- **WEB_PORTAL_CONFIG_BODY_TIMEOUT_MS**
  - src/app/board_config.h
- **WEB_PORTAL_CONFIG_MAX_JSON_BYTES**
  - src/app/board_config.h
- **WIFI_MAX_ATTEMPTS**
  - src/app/board_config.h
<!-- END COMPILE_FLAG_REPORT:USAGE -->
