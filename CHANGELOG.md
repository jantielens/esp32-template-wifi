# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

## [0.0.53] - 2026-02-17

### Added
- FPS benchmark screen: spinning arc animation with live panel FPS, present/render/frame timing (navigate via web portal screen API)
- Async present task for Buffered render mode (jc3248w535): decouples slow QSPI panel transfer from LVGL timer loop, allowing touch input and animations to process at ~50 Hz instead of ~4 Hz
- Dirty-row spinlock in Arduino_GFX driver: thread-safe tracking between LVGL flush and async present task

### Fixed
- Task watchdog crash on jc3248w535: present task now pinned to opposite core from LVGL task (both at priority 1 on same core starved IDLE0's WDT reset)

## [0.0.52] - 2026-02-13

### Changed
- Upgraded ESP32 Arduino core from 3.3.5 to 3.3.7 (resolves #58)
- Upgraded Arduino_GFX (GFX Library for Arduino) from 1.6.4 to 1.6.5 (fixes `spiFrequencyToClockDiv` SPI API breakage with core ≥3.3.6)
- Removed "pinned for library compatibility" comment in setup.sh — no longer needed

### Fixed
- GT911 touch: single taps registering as double taps on esp32-4848S040 (GT911 `bufferStatus=0` between scans was incorrectly interpreted as finger-up, causing a false RELEASED→PRESSED cycle that fired two CLICKED events)
- GT911 touch: `isTouched()` had no return statement (undefined behavior)

## [0.0.51] - 2026-02-13

### Changed
- **jc3636w518**: replaced ESP_Panel library with Arduino_GFX for ST77916 display (fixes I2C driver conflict, #67)
  - New `Arduino_GFX_ST77916_Driver`: direct rendering via `draw16bitRGBBitmap()` (no PSRAM framebuffer needed, saves ~253 KB)
  - New `Wire_CST816S_TouchDriver`: pure Wire.h I2C driver with auto-sleep disable
  - Removed `ESP32_Display_Panel@1.0.4` and `ESP32_IO_Expander@1.1.1` library dependencies
  - Deleted `esp_panel_st77916_driver.cpp/h` and `esp_panel_cst816s_touch_driver.cpp/h`
- Renamed `TOUCH_DRIVER_AXS15231B` → `TOUCH_DRIVER_AXS15231B_I2C` for consistency

### Documentation
- Updated `docs/display-touch-architecture.md`: replaced ESP_Panel references with Arduino_GFX/Wire
- Updated `docs/compile-time-flags.md`: regenerated flag report
- Regenerated `src/app/drivers/README.md` board→drivers table

## [0.0.50] - 2026-02-13

### Added
- jc3248w535: PSRAM framebuffer with driver-level rotation replaces Arduino_Canvas (eliminates GFX API overhead)
- jc3248w535: dirty-row tracking — `present()` sends only rows `0..maxDirtyRow` instead of full 480 rows

### Fixed
- AXS15231B touch driver: complete rewrite of I2C protocol (11-byte command per Espressif reference, correct response byte layout, event field state machine with `touchActive` flag to prevent double-tap artifacts from stale controller replays)
- Phantom CLICKED events after screen switch: `lv_indev_reset(NULL, NULL)` called after every deferred screen transition in DisplayManager
- Touch test screen: use LVGL runtime dimensions (`lv_disp_get_hor/ver_res`) instead of compile-time `DISPLAY_WIDTH/HEIGHT` (fixes canvas mismatch on rotated displays)
- Touch test screen: ghost touch suppression via `touch_manager_suppress_lvgl_input(200)` on show
- jc3636w518 build: migrate ESP32_Display_Panel API to v1.0.4 (class renames, namespace changes)

### Changed
- Bump `ESP32_Display_Panel` library from 0.1.4 to 1.0.4
- LVGL touch polling rate reduced from 30 ms to 10 ms (`LV_INDEV_DEF_READ_PERIOD`) for more responsive touch input
- Touch test screen canvas allocated in PSRAM on show, freed on hide (zero memory cost while inactive)
- Removed all diagnostic I2C logging from AXS15231B vendored touch driver (692 bytes smaller)

### Documentation
- Updated `docs/display-touch-architecture.md`: added AXS15231B, CST816S, GT911 touch driver docs; updated touch event flow and screen switching; added TouchTestScreen description; refreshed file organization and future enhancements

## [0.0.49] - 2026-02-11

### Added
- ESP32-4848S040 board support (Guition ESP32-S3, ST7701 RGB 480×480, 16 MB flash, 8 MB OPI PSRAM)
- ST7701 RGB display driver via Arduino_GFX delegation (SWSPI → ESP32RGBPanel → RGB_Display)
- GT911 I2C touch driver (vendored, Wire1 bus) with cached `isTouched()` to avoid redundant I2C reads
- Touch test screen (red dots + white connecting lines on LVGL canvas)
- Board-overridable `ST7701_PCLK_HZ` (default 6 MHz) and `ST7701_BOUNCE_BUFFER_LINES` defines
- `LV_USE_CANVAS` now board-overridable via `#ifndef` guard in lv_conf.h
- PWM backlight brightness control with board-configurable frequency and duty range
  - New defines: `TFT_BACKLIGHT_PWM_FREQ`, `TFT_BACKLIGHT_DUTY_MIN`, `TFT_BACKLIGHT_DUTY_MAX`
  - LEDC PWM attached before LCD panel init to prevent GPIO reconfiguration glitch
  - ESP32-4848S040 tuned to 3.5 kHz (no coil whine, smooth dimming, duty 77–252)

### Fixed
- Critical single-core bug: `rtos_create_task_psram_stack` passed string literal instead of task function pointer
- Unified LVGL flush callback to single code path (was three separate paths with dead direct-mode logic)

### Removed
- Dead display_driver.h methods: `getFramebuffer()`, `getLVGLDirectBuffers()`, `swapBuffers()`
- Per-board watchdog (unnecessary — Arduino default WDT already watches IDLE tasks)

### Documentation
- Added `docs/esp32-4848s040.md` board guide (display, touch, memory, LVGL integration)
- Updated `docs/compile-time-flags.md` with new ST7701 defines

## [0.0.48] - 2026-01-31

### Changed
- Moved BLE advertise cadence and Wi-Fi watchdog logic into their managers
- Standardized Arduino/C++ indentation to tabs with 4-space width via .editorconfig
- Clarified time-unit naming for heartbeat and Wi-Fi watchdog helpers

## [0.0.47] - 2026-01-31

### Added
- Power manager, duty-cycle flow, and Wi-Fi manager for power-optimized operation
- BLE BTHome advertising with configurable burst timing and optional transport selection
- Portal idle timeout handling for Config/AP modes
- Dummy sensor adapter for synthetic telemetry testing

### Changed
- Consolidated publish cadence under Cycle Interval (seconds) across BLE/MQTT
- MQTT publishing supports scoped payloads (sensors-only/diagnostics/all)
- Always-On BLE-only configs can run without Wi-Fi SSID (portal access via button/burst)
- Updated portal UI to expose power/transport settings

### Documentation
- Updated web portal, MQTT, and sensors docs for new power/transport behavior

## [0.0.46] - 2026-01-29

### Added
- Sensor framework with sample BME280 and LD2410 OUT adapters
- Event-driven MQTT publishing for presence sensors
- Home Assistant discovery helpers for event topics
- Sensors section in the web portal UI

### Changed
- /api/health now includes a `sensors` object for optional readings
- Added ESP32-C3 sensor sample board configuration
- Updated sensor developer guide and related docs

## [0.0.45] - 2026-01-29

### Fixed
- AXS15231B touch: avoid invalid INT pin crashes and add polling fallback when INT is not wired

## [0.0.44] - 2026-01-24

### Changed
- Scripts: infer chip family from FQBN board id (explicit esp32* token or small heuristic) for upload/erase
- Scripts: print resolved chip family during upload for debugging
- Bump Arduino_GFX (GFX Library for Arduino) to 1.6.4 to fix ESP32 core 3.3.6 SPI API build failures
- Pin ESP32 core to 3.3.5 in setup.sh to avoid Arduino_GFX SPI API incompatibility
- setup.sh now refreshes the library index and fails fast if any library install fails

### Documentation
- Documented chip-family inference behavior in scripts guide

### Fixed
- Arduino_GFX driver build: replace deprecated BLACK constant with RGB565_BLACK for Arduino_GFX 1.6.4

## [0.0.43] - 2026-01-19

### Changed
- Compile-time flags report now honors config.project.sh and fails fast when it exists but defines no boards
- build.sh now checks and updates docs/compile-time-flags.md during builds and reports whether it changed

## [0.0.42] - 2026-01-17

### Added
- Device Health Dashboard page for the GitHub Pages installer (Plotly-based live health chart + latest values)

## [0.0.41] - 2026-01-17

### Added
- OTA download retry with fresh connections plus richer failure logging (HTTP error string, WiFi status/RSSI)

## [0.0.40] - 2026-01-17

### Added
- GitHub Pages OTA manifests (`site/ota/<board>.json`) with size + sha256 metadata
- GitHub Pages updater flow that detects device board via `/api/info` and triggers OTA
- Wi-Fi update UX separation in the installer (dedicated device update panel)
- CORS handling for OTA API endpoints (restricted to GitHub Pages origin)

### Changed
- Firmware page now links to the GitHub Pages updater (replaces GitHub Releases OTA UI)
- OTA update API now accepts a direct firmware URL payload
- Repo slug header generated for GitHub Pages URL construction

## [0.0.39] - 2026-01-16

### Changed
- Logging: replace nested LogManager with flat LOGx macros (single-line, timestamped)

## [0.0.38] - 2026-01-16

### Changed
- Telemetry: move the cpu monitor FreeRTOS task stack to PSRAM on PSRAM-capable targets to free internal heap
- Build: make TFT_eSPI configuration reproducible in clean/CI builds via per-board `User_Setup.h` support (fixes CYD v2 white screen when flashed from web installer)

## [0.0.37] - 2026-01-15

### Added
- Web API: optional device-side health history endpoint `GET /api/health/history` for sparklines
- Health history: monotonic `uptime_ms` array to correlate samples with logs
- Logging: prefix all log lines with a monotonic `millis()` timestamp (e.g. `[123456ms] ...`)

### Changed
- Web portal: uses device-side history when available; falls back to point-in-time fields when unavailable
- Web API: `GET /api/info` advertises health history availability and parameters
- MQTT: `devices/<sanitized>/health/state` no longer includes `mqtt_*` self-report fields (kept in `/api/health` only)
- Home Assistant: expanded default MQTT discovery to cover more of the published health payload (adds `binary_sensor` discovery for boolean fields)

### Documentation
- Regenerated `docs/compile-time-flags.md`
- Updated `docs/home-assistant-mqtt.md` to reflect current default discovery + payload fields
