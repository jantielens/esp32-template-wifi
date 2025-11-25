#!/bin/bash

# ESP32 Build Script
# Compiles the Arduino sketch to the /build directory

set -e

# Load common configuration and functions
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/config.sh"

# Get arduino-cli path
ARDUINO_CLI=$(find_arduino_cli)

echo -e "${CYAN}=== Building ESP32 Firmware ===${NC}"

# Generate web assets
echo "Generating web assets..."
"$SCRIPT_DIR/tools/minify-web-assets.sh"
echo ""

# Create build directory if it doesn't exist
mkdir -p "$BUILD_PATH"

# Compile the sketch
echo "Compiling sketch..."
echo "Sketch:    $SKETCH_PATH"
echo "Board:     $FQBN"
echo "Output:    $BUILD_PATH"
echo ""

"$ARDUINO_CLI" compile \
    --fqbn "$FQBN" \
    --output-dir "$BUILD_PATH" \
    "$SKETCH_PATH"

echo ""
echo -e "${GREEN}=== Build Complete ===${NC}"
echo "Firmware binary created in: $BUILD_PATH"
ls -lh "$BUILD_PATH"/*.bin 2>/dev/null || echo "Binary files generated"
