# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

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
