# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **Automated Release Workflow**: GitHub Actions workflow for automated releases
  - `.github/workflows/release.yml` - Tag-triggered release pipeline
  - `tools/extract-changelog.sh` - Changelog parser for release notes
  - `create-release.sh` - Helper script for release preparation
  - Multi-board firmware artifacts with version naming
  - SHA256 checksums generation
  - Pre-release support (tags with hyphens)
  - Lightweight releases with only .bin files (debug symbols in workflow artifacts)

### Changed
- Updated README.md with comprehensive release process documentation
- Streamlined release artifacts to include only firmware binaries and checksums

---

## [0.0.4] - 2025-11-26

### Added
- **Log Management System**: New structured logging with nested blocks and automatic timing
  - `LogManager` class: Print-compatible wrapper for Serial with log buffer integration
  - `LogBuffer`: Thread-safe circular buffer (50 entries, 200 chars each)
  - Automatic routing to both Serial output and web buffer
  - Indentation-based nested log blocks with elapsed time tracking
- **Web Logs Viewer**: New full-screen logs interface at `/logs`
  - Real-time polling of device logs (2s interval)
  - Syntax highlighting for error/warning/info messages
  - Auto-scroll toggle and connection controls
  - Formatted timestamps (HH:MM:SS.milliseconds since boot)
  - Link from main portal in Advanced section
- **Memory Optimizations**: AsyncResponseStream for JSON responses to reduce stack usage
- **Build Tool Enhancement**: `minify-web-assets.sh` now generates size constants for PROGMEM assets

### Changed
- Replaced all `Serial.print/println` calls with `Logger` methods throughout codebase
- Reduced heartbeat interval from 5s to 60s for less verbose logging
- Improved web portal asset serving with explicit size constants (avoids `strlen_P()` overhead)
- Enhanced AsyncTCP stack size to 16KB for large web assets
- Updated `config.sh` with detailed CDCOnBoot documentation for USB serial support

### Fixed
- Improved error handling in `config.sh` board name resolution
- Cleaned up duplicate CSS rules in portal.css

---

## [0.0.3] - 2025-11-26

### Added
- Enhanced mDNS service discovery with 5 additional TXT records following RFC 6763 DNS-SD best practices:
  - `ty`: device type classification (`iot-device`)
  - `mf`: manufacturer identifier (`ESP32-Tmpl`)
  - `features`: device capabilities (`wifi,http,api`)
  - `note`: human-readable description
  - `url`: direct configuration URL

### Fixed
- Fixed missing `API_VERSION` constant that caused 404 errors during device reconnection polling
- Fixed OTA polling starting prematurely during firmware upload instead of after completion

### Changed
- Improved reconnection polling strategy: 2s initial delay, then 3s intervals (total 122s timeout)
- Unified reboot dialog for all scenarios (save config, OTA update, manual reboot, factory reset)
- Consolidated reconnection logic from two separate functions into single `startReconnection()`
- Better user feedback with best-effort messaging and manual fallback addresses
- Enhanced timeout handling with troubleshooting hints
- Progress display now shows elapsed time during reconnection attempts
- Updated documentation to reflect 8-field mDNS TXT record structure

### Removed
- Removed duplicate OTA overlay dialog (now uses unified reboot dialog)
- Removed deprecated `startOTAReconnect()` function
- Removed NetBIOS references from documentation (functionality removed earlier)

---

## [0.0.2] - 2025-11-25

### Added
- Multi-board build support with configurable FQBN targets
- Board-specific build directories: `build/esp32/`, `build/esp32c3/`, etc.
- `PROJECT_NAME` configuration variable for consistent firmware naming
- Helper functions in `config.sh`: `get_board_name()`, `list_boards()`, `get_fqbn_for_board()`
- Board parameter support in `build.sh`, `upload.sh`, `upload-erase.sh`, `bum.sh`, `um.sh`
- GitHub Actions matrix build strategy for parallel board compilation
- ESP32-C3 Super Mini board configuration
- Separate artifact uploads per board in CI/CD

### Changed
- Build system now creates board-specific directories instead of flat `build/` structure
- Scripts require board name parameter when multiple boards are configured
- `./build.sh` without parameters now builds all configured boards
- CI/CD workflow generates separate artifacts for each board variant
- Updated documentation to reflect multi-board workflow

---

## [0.0.1] - 2025-11-25

### Added
- Initial release of ESP32 Arduino development template
- Headless build system using `arduino-cli` with local installation
- Build automation scripts with auto-detection of serial ports
- Library management system
- Web configuration portal with captive portal support
- REST API for device configuration and monitoring
- Real-time health monitoring widget
- OTA firmware updates via web interface
- NVS-based configuration storage
- WSL2 USB/IP support
- CI/CD pipeline with GitHub Actions
- Comprehensive documentation

---

## Template for Future Releases

```markdown
## [X.Y.Z] - YYYY-MM-DD

### Added
- New features

### Changed
- Changes to existing features

### Deprecated
- Features marked for removal

### Removed
- Removed features

### Fixed
- Bug fixes

### Security
- Security patches
```
