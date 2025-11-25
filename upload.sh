#!/bin/bash

# ESP32 Upload Script
# Uploads compiled firmware to ESP32 via serial port

set -e

# Load common configuration and functions
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/config.sh"

# Get arduino-cli path
ARDUINO_CLI=$(find_arduino_cli)

# Auto-detect port if not specified
if [ -z "$1" ]; then
    if PORT=$(find_serial_port); then
        echo -e "${GREEN}Auto-detected port: $PORT${NC}"
    else
        echo -e "${RED}Error: No serial port detected${NC}"
        echo "Usage: $0 [PORT]"
        echo "Example: $0 /dev/ttyUSB0"
        exit 1
    fi
else
    PORT="$1"
fi

echo -e "${CYAN}=== Uploading ESP32 Firmware ===${NC}"

# Check if build directory exists
if [ ! -d "$BUILD_PATH" ]; then
    echo -e "${RED}Error: Build directory not found${NC}"
    echo "Please run build.sh first"
    exit 1
fi

# Display upload configuration
echo "Port: $PORT"
echo "Board: $FQBN"
echo "Build directory: $BUILD_PATH"
echo ""

# Upload firmware
"$ARDUINO_CLI" upload \
    --fqbn "$FQBN" \
    --port "$PORT" \
    --input-dir "$BUILD_PATH"

echo ""
echo -e "${GREEN}=== Upload Complete ===${NC}"
echo "Run ./monitor.sh to view serial output"
