# Development Scripts

This project includes several bash scripts to streamline ESP32 development workflow.

## config.sh

**Purpose:** Common configuration and helper functions used by all scripts.

**What it contains:**
- Project configuration (`PROJECT_NAME`, `SKETCH_PATH`, `BUILD_PATH`)
- Board targets (`FQBN_TARGETS` associative array)
- `find_arduino_cli()` - Locates arduino-cli (local or system-wide)
- `find_serial_port()` - Auto-detects `/dev/ttyUSB0` or `/dev/ttyACM0`
- `get_board_name()` - Returns board name (identity function for compatibility)
- `list_boards()` - Lists all configured boards
- `get_fqbn_for_board()` - Gets FQBN for a board name

**Usage:** Automatically sourced by other scripts. Do not run directly.

**Multi-Board Configuration:**
```bash
declare -A FQBN_TARGETS=(
    ["esp32"]="esp32:esp32:esp32"                                        # ESP32 Dev Module
    ["esp32c3"]="esp32:esp32:nologo_esp32c3_super_mini:CDCOnBoot=cdc"  # ESP32-C3 Super Mini
    ["esp32c3_ota_1_9mb"]="esp32:esp32:nologo_esp32c3_super_mini:CDCOnBoot=cdc,PartitionScheme=ota_1_9mb"  # ESP32-C3 w/ custom partitions
    ["cyd2usb-v2"]="esp32:esp32:esp32"                                   # CYD v2 (same FQBN, different board_overrides.h)
)
```

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
- Installs optional custom partition tables used by example board targets (if present)
- Installs libraries from `arduino-libraries.txt`

**When to use:** Run once when setting up the project, or after a clean checkout.

---

## build.sh

**Purpose:** Compile the Arduino sketch into ESP32 firmware for one or more board variants.

**Usage:**
```bash
./build.sh              # Build all configured boards
./build.sh esp32        # Build only ESP32 Dev Module
./build.sh esp32c3      # Build only ESP32-C3 Super Mini
./build.sh esp32c3_ota_1_9mb  # Build ESP32-C3 using custom partitions (example)
BOARD_PROFILE=psram ./build.sh esp32  # Optional build profile (if defined in config.sh)
```

**What it does:**
- Generates minified web assets (once for all builds)
- Compiles `src/app/app.ino` for specified board(s)
- Creates board-specific directories: `./build/esp32/`, `./build/esp32c3/`, etc.
- Generates `.bin`, `.bootloader.bin`, `.merged.bin`, and `.partitions.bin` files per board
- If `src/boards/<board>/` exists, adds it to include path and defines:
    - `BOARD_<BOARDNAME>` - Board name sanitized to valid C++ macro (alphanumeric + underscore only)
      - Examples: `cyd2usb-v2` → `BOARD_CYD2USB_V2`, `esp32c3` → `BOARD_ESP32C3`
    - `BOARD_HAS_OVERRIDE` (triggers inclusion of `board_overrides.h`)

**Build Output Structure:**
```
build/
├── esp32/
│   ├── app.ino.bin
│   ├── app.ino.bootloader.bin
│   ├── app.ino.merged.bin
│   └── app.ino.partitions.bin
└── esp32c3/
    ├── app.ino.bin
    └── ...
```

**Requirements:** Must run `setup.sh` first.

---

## tools/install-custom-partitions.sh

**Purpose:** Install/register template-provided custom partition tables into the Arduino ESP32 core.

Some FQBN options (like `PartitionScheme=ota_1_9mb`) only work if:
1. The partition CSV exists in the ESP32 core `tools/partitions/` directory, and
2. The scheme is registered in the ESP32 core `boards.txt`.

This script automates both steps.

**Usage:**
```bash
./tools/install-custom-partitions.sh
```

**When to use:**
- Automatically run by `./setup.sh`
- Run manually after upgrading the ESP32 Arduino core (the install path can change)

---

## upload.sh

**Purpose:** Upload compiled firmware to the ESP32 device.

**Usage:**

**Single Board Configuration:**
```bash
./upload.sh              # Auto-detects port
./upload.sh /dev/ttyUSB0 # Specify port
```

**Multiple Boards Configuration:**
```bash
./upload.sh esp32              # Upload ESP32 build, auto-detect port
./upload.sh esp32c3            # Upload ESP32-C3 build, auto-detect port
./upload.sh esp32c3_ota_1_9mb   # Upload ESP32-C3 build using custom partitions
./upload.sh esp32 /dev/ttyUSB0 # Upload ESP32 build to specific port
```

**What it does:**
- Validates board name (required when multiple boards configured)
- Detects connected ESP32 boards
- Uploads firmware from board-specific `./build/<board>/` directory to the device
- Auto-detects serial port if not specified

**Requirements:** 
- Must run `build.sh [board]` first
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
- Removes the `./build/` directory and all board subdirectories
- Lists board directories being removed
- Cleans up temporary files (*.tmp, *.bak, *~)
- Prepares for a fresh build

**When to use:** When you want to force a complete rebuild or clean up disk space.

---

## upload-erase.sh

**Purpose:** Completely erase the ESP32 flash memory.

**Usage:**

**Single Board Configuration:**
```bash
./upload-erase.sh              # Auto-detects port
./upload-erase.sh /dev/ttyUSB0 # Specify port
```

**Multiple Boards Configuration:**
```bash
./upload-erase.sh esp32              # Erase ESP32, auto-detect port
./upload-erase.sh esp32c3            # Erase ESP32-C3, auto-detect port
./upload-erase.sh esp32 /dev/ttyUSB0 # Erase ESP32 on specific port
```

**What it does:**
- Validates board name (required when multiple boards configured)
- Extracts chip type from FQBN for correct esptool invocation
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

**Single Board Project:**
```bash
# Initial setup (one time)
./setup.sh

# Add libraries as needed
./library.sh search bme280
./library.sh add "Adafruit BME280 Library"

# Development cycle
./build.sh              # Compile firmware
./upload.sh             # Upload to device
./monitor.sh            # View serial output

# Or use convenience scripts
./bum.sh                # Build + Upload + Monitor
./um.sh                 # Upload + Monitor

# Clean build when needed
./clean.sh
./build.sh

# Complete flash erase (when needed)
./upload-erase.sh
```

**Multi-Board Project:**
```bash
# Initial setup (one time)
./setup.sh

# Build all boards
./build.sh

# Or build specific board
./build.sh esp32
./build.sh esp32c3
./build.sh esp32c3_ota_1_9mb

# Upload to specific board
./upload.sh esp32       # Auto-detects port
./upload.sh esp32c3     # Auto-detects port
./upload.sh esp32c3_ota_1_9mb

# Full cycle for specific board
./bum.sh esp32          # Build + Upload + Monitor
./um.sh esp32c3         # Upload + Monitor

# Clean all board builds
./clean.sh
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
- Run `./build.sh [board]` before `./upload.sh [board]`
- Ensure board name matches configured boards in `config.sh`

**Partition scheme not found / build fails with PartitionScheme:**
- Re-run `./setup.sh`, or run `./tools/install-custom-partitions.sh`
- First flash after changing partition tables should be done over serial (USB)

**Board name required error:**
- When multiple boards are configured, specify board name: `./upload.sh esp32`
- List available boards by checking `config.sh` or running with invalid board name

For WSL-specific setup, see [WSL Development Guide](wsl-development.md).
