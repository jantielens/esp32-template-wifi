# Development Scripts

This project includes several bash scripts to streamline ESP32 development workflow.

## config.sh

**Purpose:** Common configuration and helper functions used by all scripts.

**What it contains:**
- Project paths (`SKETCH_PATH`, `BUILD_PATH`, `FQBN`)
- `find_arduino_cli()` - Locates arduino-cli (local or system-wide)
- `find_serial_port()` - Auto-detects `/dev/ttyUSB0` or `/dev/ttyACM0`

**Usage:** Automatically sourced by other scripts. Do not run directly.

**Note:** This file centralizes common code to avoid duplication across scripts.

---

## setup.sh

**Purpose:** Install and configure the ESP32 development environment.

**Usage:**
```bash
./setup.sh
```

**What it does:**
- Downloads and installs `arduino-cli` to `./bin/`
- Configures ESP32 board support
- Installs ESP32 core platform
- Installs libraries from `arduino-libraries.txt`

**When to use:** Run once when setting up the project, or after a clean checkout.

---

## build.sh

**Purpose:** Compile the Arduino sketch into ESP32 firmware.

**Usage:**
```bash
./build.sh
```

**What it does:**
- Compiles `src/app/app.ino` 
- Outputs firmware binaries to `./build/` directory
- Generates `.bin`, `.bootloader.bin`, `.merged.bin`, and `.partitions.bin` files

**Requirements:** Must run `setup.sh` first.

---

## upload.sh

**Purpose:** Upload compiled firmware to the ESP32 device.

**Usage:**
```bash
./upload.sh              # Auto-detects /dev/ttyUSB0 or /dev/ttyACM0
./upload.sh /dev/ttyUSB0 # Specify custom port
```

**What it does:**
- Detects connected ESP32 boards
- Uploads firmware from `./build/` to the device
- Auto-detects serial port if not specified

**Requirements:** 
- Must run `build.sh` first
- ESP32 device must be connected via USB
- User must be in `dialout` group (see [WSL Development Guide](wsl-development.md))

---

## monitor.sh

**Purpose:** Display serial output from the ESP32 device.

**Usage:**
```bash
./monitor.sh                  # Auto-detects port, 115200 baud
./monitor.sh /dev/ttyUSB0     # Custom port, default baud
./monitor.sh /dev/ttyUSB0 9600 # Custom port and baud rate
```

**What it does:**
- Opens serial monitor connection to ESP32
- Displays real-time output from `Serial.print()` statements
- Press `Ctrl+C` to exit

**Requirements:** 
- ESP32 device must be connected via USB
- User must be in `dialout` group

---

## clean.sh

**Purpose:** Remove build artifacts and temporary files.

**Usage:**
```bash
./clean.sh
```

**What it does:**
- Removes the `./build/` directory and all compiled binaries
- Cleans up temporary files (*.tmp, *.bak, *~)
- Prepares for a fresh build

**When to use:** When you want to force a complete rebuild or clean up disk space.

---

## upload-erase.sh

**Purpose:** Completely erase the ESP32 flash memory.

**Usage:**
```bash
./upload-erase.sh              # Auto-detects port
./upload-erase.sh /dev/ttyUSB0 # Specify custom port
```

**What it does:**
- Uses esptool.py to completely erase ESP32 flash
- Prompts for confirmation before erasing
- Auto-detects serial port if not specified

**Requirements:**
- ESP32 device must be connected via USB
- User must be in `dialout` group

**Warning:** This erases ALL data including firmware, WiFi credentials, and stored settings.

---

## library.sh

**Purpose:** Manage Arduino libraries for the project.

**Usage:**
```bash
./library.sh search <keyword>    # Search for libraries
./library.sh add <library>       # Add and install library
./library.sh remove <library>    # Remove from config
./library.sh list                # Show configured libraries
./library.sh install             # Install all from config
./library.sh installed           # Show currently installed
```

**What it does:**
- Searches arduino-cli library index
- Adds libraries to `arduino-libraries.txt` and installs them
- Removes libraries from configuration
- Lists and manages project dependencies

**Requirements:** Must run `setup.sh` first.

**See also:** [Library Management Guide](library-management.md) for detailed documentation.

---

## Typical Workflow

```bash
# Initial setup (one time)
./setup.sh

# Add libraries as needed
./library.sh search mqtt
./library.sh add PubSubClient

# Development cycle
./build.sh              # Compile firmware
./upload.sh             # Upload to device
./monitor.sh            # View serial output

# Or combine upload and monitor
./upload.sh && ./monitor.sh

# Clean build when needed
./clean.sh
./build.sh

# Complete flash erase (when needed)
./upload-erase.sh
```

## Troubleshooting

**Permission denied on /dev/ttyUSB0:**
```bash
sudo usermod -a -G dialout $USER
# Then fully restart WSL: wsl --terminate Ubuntu (in PowerShell)
```

**arduino-cli not found:**
- Run `./setup.sh` first
- Scripts use local `./bin/arduino-cli`, not system PATH

**Build directory not found:**
- Run `./build.sh` before `./upload.sh`

For WSL-specific setup, see [WSL Development Guide](wsl-development.md).
