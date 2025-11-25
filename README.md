# ESP32 Development Template

A streamlined ESP32 Arduino development template using `arduino-cli` for headless builds. Optimized for WSL2/Linux environments with local toolchain installation and no system dependencies.

## ✨ Features

- **Zero System Dependencies**: Local `arduino-cli` installation (no sudo required)
- **Simple Build Scripts**: One-command compilation, upload, and monitoring
- **Web Configuration Portal**: WiFi setup via captive portal with REST API
- **Health Monitoring**: Real-time device stats with floating status widget
- **OTA Updates**: Over-the-air firmware updates via web interface
- **Version Tracking**: Built-in firmware version management
- **Clean Project Structure**: Organized directories with best practices
- **CI/CD Ready**: GitHub Actions workflow for automated validation

## 🚀 Quick Start

### Prerequisites

- Linux or WSL2 environment
- USB connection to ESP32 device
- Bash shell

### 1. Clone and Setup

```bash
git clone https://github.com/jantielens/esp32-template.git
cd esp32-template
./setup.sh
```

### 2. Build, Upload, Monitor

```bash
./build.sh              # Compile firmware
./upload.sh             # Upload to ESP32
./monitor.sh            # View serial output

# Or use convenience scripts:
./bum.sh                # Build + Upload + Monitor
./um.sh                 # Upload + Monitor
```

### 3. Start Developing

Edit `src/app/app.ino` with your code and repeat step 2.

## 📁 Project Structure

```
esp32-template-wifi/
├── bin/                    # Local arduino-cli installation
├── build/                  # Compiled firmware binaries
├── docs/                   # Documentation
│   ├── scripts.md         # Script usage guide
│   ├── web-portal.md      # Web portal guide
│   ├── wsl-development.md # WSL/USB setup
│   └── library-management.md # Library management
├── src/
│   ├── app/
│   │   ├── app.ino        # Main sketch file
│   │   ├── config_manager.cpp/h  # NVS config storage
│   │   ├── web_portal.cpp/h      # Web server & API
│   │   ├── web_assets.cpp/h      # Embedded HTML/CSS/JS
│   │   └── web/
│   │       ├── portal.html   # Portal interface
│   │       ├── portal.css    # Styles
│   │       └── portal.js     # Client logic
│   └── version.h          # Firmware version tracking
├── .github/
│   └── workflows/
│       └── build.yml      # CI/CD pipeline
├── config.sh              # Common configuration
├── build.sh               # Compile sketch
├── upload.sh              # Upload firmware
├── upload-erase.sh        # Erase flash memory
├── monitor.sh             # Serial monitor
├── clean.sh               # Clean build artifacts
├── library.sh             # Library management
├── bum.sh / um.sh         # Convenience scripts
├── setup.sh               # Environment setup
└── arduino-libraries.txt  # Library dependencies
```

## 🌐 Web Configuration Portal

The template includes a full-featured web portal for device configuration and monitoring.

### Portal Modes

**Core Mode (AP)**: Runs when WiFi is not configured
- Device creates Access Point: `ESP32-XXXXXX`
- Captive portal at: `http://192.168.4.1`
- Configure WiFi credentials and device settings

**Full Mode (WiFi)**: Runs when connected to WiFi
- Access at device IP or mDNS hostname
- All configuration options available
- Real-time health monitoring
- OTA firmware updates

### Features

- 🔧 **WiFi Configuration**: SSID, password, fixed IP settings
- 📊 **Health Monitoring**: CPU usage, temperature, memory, uptime
- 🔄 **OTA Updates**: Upload firmware via web interface
- 💾 **Config Management**: Save, reset, export settings
- 📱 **Responsive UI**: Works on desktop and mobile

### REST API Endpoints

| Method | Endpoint | Purpose |
|--------|----------|----------|
| GET | `/api/info` | Device info (firmware, chip, cores, flash, PSRAM) |
| GET | `/api/health` | Real-time health stats (CPU, memory, WiFi, uptime) |
| GET | `/api/config` | Current configuration |
| POST | `/api/config` | Save configuration (triggers reboot) |
| DELETE | `/api/config` | Reset to defaults (triggers reboot) |
| GET | `/api/mode` | Portal mode (core vs full) |
| POST | `/api/update` | OTA firmware upload |
| POST | `/api/reboot` | Reboot device |

See [docs/web-portal.md](docs/web-portal.md) for detailed guide.

## 🛠️ Available Scripts

| Script | Purpose | Usage |
|--------|---------|-------|
| `config.sh` | Common configuration (sourced by scripts) | Sourced automatically |
| `setup.sh` | Install arduino-cli and ESP32 core | Run once during initial setup |
| `build.sh` | Compile the Arduino sketch | `./build.sh` |
| `upload.sh` | Upload firmware to ESP32 | `./upload.sh [port]` |
| `monitor.sh` | Display serial output | `./monitor.sh [port] [baud]` |
| `bum.sh` | Build + Upload + Monitor (full cycle) | `./bum.sh` |
| `um.sh` | Upload + Monitor | `./um.sh [port]` |
| `upload-erase.sh` | Completely erase ESP32 flash | `./upload-erase.sh [port]` |
| `clean.sh` | Remove build artifacts | `./clean.sh` |
| `library.sh` | Manage Arduino libraries | `./library.sh [command]` |

See [docs/scripts.md](docs/scripts.md) for detailed documentation.

## 🔧 Configuration

### Target Board

Default: `esp32:esp32:esp32` (ESP32 Dev Module)

To change the board, edit the `FQBN` variable in `build.sh` and `upload.sh`:

```bash
FQBN="esp32:esp32:esp32s3"  # For ESP32-S3
FQBN="esp32:esp32:esp32c3"  # For ESP32-C3
```

### Serial Port

Scripts auto-detect `/dev/ttyUSB0` or `/dev/ttyACM0`. Manually specify if needed:

```bash
./upload.sh /dev/ttyUSB1
./monitor.sh /dev/ttyACM0 115200
```

### Baud Rate

Default: 115200 (configured in `app.ino` and `monitor.sh`)

## 🖥️ WSL2 Development

For Windows users with WSL2, USB passthrough is required:

```powershell
# Windows PowerShell (Administrator)
usbipd list
usbipd bind --busid 1-4
usbipd attach --wsl --busid 1-4
```

```bash
# WSL Terminal
sudo usermod -a -G dialout $USER
# Restart WSL: wsl --terminate Ubuntu (in PowerShell)
```

See [docs/wsl-development.md](docs/wsl-development.md) for complete guide.

## 📚 Library Management

Libraries are managed via `arduino-libraries.txt` for consistency across local and CI environments.

### Quick Commands

```bash
./library.sh search mqtt          # Find libraries
./library.sh add PubSubClient     # Add and install
./library.sh list                 # Show configured libraries
./library.sh installed            # Show installed libraries
```

### Adding a Library

```bash
# Search for the library
./library.sh search "sensor"

# Add it to your project (installs + updates config)
./library.sh add "Adafruit BME280 Library"

# Commit the configuration
git add arduino-libraries.txt
git commit -m "Add BME280 sensor library"
```

Libraries in `arduino-libraries.txt` are automatically installed by `setup.sh` and in GitHub Actions.

**Note:** The template starts with no libraries configured. Uncomment or add libraries as needed for your project.

See [docs/library-management.md](docs/library-management.md) for detailed guide.

## 🔍 Version Management

Firmware versions are tracked in `src/version.h`:

```cpp
#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 0
```

Version is automatically displayed at startup. Update before releases.

## 🧪 Testing & CI/CD

GitHub Actions automatically validates builds on push:

- Compiles firmware for ESP32 Dev Module
- Caches arduino-cli and libraries for faster builds
- Uploads build artifacts (.bin and .elf files)

## 🐛 Troubleshooting

### Permission Denied on Serial Port

```bash
sudo usermod -a -G dialout $USER
# Logout and login, or restart WSL
```

### arduino-cli Not Found

```bash
./setup.sh  # Reinstall arduino-cli
```

### Build Directory Not Found

```bash
./build.sh  # Build before uploading
```

### Device Not Detected

```bash
# Check if device is connected
ls -l /dev/ttyUSB* /dev/ttyACM*

# For WSL, check Windows PowerShell
usbipd list
```

## 📖 Documentation

- [Web Portal Guide](docs/web-portal.md) - Configuration portal & REST API
- [Script Reference](docs/scripts.md) - Detailed script usage
- [WSL Development Guide](docs/wsl-development.md) - Windows/WSL setup
- [Library Management](docs/library-management.md) - Managing dependencies
- [Changelog](CHANGELOG.md) - Version history and release notes

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

**Made with ❤️ for ESP32 developers**
