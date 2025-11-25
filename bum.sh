#!/bin/bash

# ESP32 Build, Upload & Monitor Script
# Convenience script for the complete development cycle

set -e

# Load common configuration and functions
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/config.sh"

echo -e "${CYAN}=== ESP32 Full Cycle: Build, Upload & Monitor ===${NC}"
echo ""

# Run build
"$SCRIPT_DIR/build.sh"

echo ""

# Run upload
"$SCRIPT_DIR/upload.sh" "$@"

echo ""

# Run monitor
"$SCRIPT_DIR/monitor.sh" "$@"
