# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed
- Fixed missing `API_VERSION` constant that caused 404 errors during device reconnection polling

### Changed
- Improved reconnection polling strategy: 2s initial delay, then 3s intervals (total 122s timeout)
- Unified reboot dialog for all scenarios (save config, OTA update, manual reboot, factory reset)
- Consolidated reconnection logic from two separate functions into single `startReconnection()`
- Better user feedback with best-effort messaging and manual fallback addresses
- Enhanced timeout handling with troubleshooting hints
- Progress display now shows elapsed time during reconnection attempts

### Removed
- Removed duplicate OTA overlay dialog (now uses unified reboot dialog)
- Removed deprecated `startOTAReconnect()` function

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
