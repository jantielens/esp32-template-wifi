#!/bin/bash

# ESP32 Template Project Configuration
# This file contains common configuration and helper functions used by all scripts
# Source this file at the beginning of each script

# Get script directory (works when sourced)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Color definitions
BLUE='\033[0;34m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Project configuration
SKETCH_PATH="$SCRIPT_DIR/src/app/app.ino"
BUILD_PATH="$SCRIPT_DIR/build"
# Board configuration (FQBN - Fully Qualified Board Name)
# For ESP32 Dev Module (default):
FQBN="esp32:esp32:esp32"
# For ESP32-C3 Super Mini (with USB CDC enabled):
#FQBN="esp32:esp32:nologo_esp32c3_super_mini:CDCOnBoot=cdc"
# Note: ESP32-C3 uses built-in USB CDC and appears as /dev/ttyACM0 (not /dev/ttyUSB0)

# Find arduino-cli executable
# Checks for local installation first, then falls back to system-wide
find_arduino_cli() {
    if [ -f "$SCRIPT_DIR/bin/arduino-cli" ]; then
        echo "$SCRIPT_DIR/bin/arduino-cli"
    elif command -v arduino-cli &> /dev/null; then
        echo "arduino-cli"
    else
        echo "Error: arduino-cli is not found" >&2
        echo "Please run setup.sh or install arduino-cli system-wide" >&2
        exit 1
    fi
}

# Auto-detect serial port
# Returns /dev/ttyUSB0 if exists, otherwise /dev/ttyACM0
# Returns exit code 1 if no port found
find_serial_port() {
    if [ -e /dev/ttyUSB0 ]; then
        echo "/dev/ttyUSB0"
        return 0
    elif [ -e /dev/ttyACM0 ]; then
        echo "/dev/ttyACM0"
        return 0
    else
        return 1
    fi
}
