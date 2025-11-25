#!/bin/bash

# ESP32 Clean Script
# Removes build artifacts and temporary files

set -e

# Load common configuration and functions
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/config.sh"

echo -e "${CYAN}=== Cleaning ESP32 Build Artifacts ===${NC}"

# Remove build directory
if [ -d "$BUILD_PATH" ]; then
    echo "Removing build directory: $BUILD_PATH"
    rm -rf "$BUILD_PATH"
    echo -e "${GREEN}Build directory removed${NC}"
else
    echo -e "${YELLOW}Build directory does not exist: $BUILD_PATH${NC}"
fi

# Remove arduino-cli cache (optional - uncomment if needed)
# if [ -d "$SCRIPT_DIR/.arduino15" ]; then
#     echo "Removing arduino-cli cache: $SCRIPT_DIR/.arduino15"
#     rm -rf "$SCRIPT_DIR/.arduino15"
#     echo "Arduino CLI cache removed"
# fi

# Remove any temporary files
echo "Removing temporary files..."
find "$SCRIPT_DIR" -type f -name "*.tmp" -delete 2>/dev/null || true
find "$SCRIPT_DIR" -type f -name "*.bak" -delete 2>/dev/null || true
find "$SCRIPT_DIR" -type f -name "*~" -delete 2>/dev/null || true

echo ""
echo -e "${GREEN}=== Clean Complete ===${NC}"
echo "All build artifacts have been removed"
echo ""
echo "To rebuild: ./build.sh"
