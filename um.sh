#!/bin/bash

# ESP32 Upload & Monitor Script
# Convenience script for upload and monitor cycle

set -e

# Load common configuration and functions
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/config.sh"

echo -e "${CYAN}=== ESP32 Upload & Monitor ===${NC}"
echo ""

# Run upload
"$SCRIPT_DIR/upload.sh" "$@"

echo ""

# Run monitor
"$SCRIPT_DIR/monitor.sh" "$@"
