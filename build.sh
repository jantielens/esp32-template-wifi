#!/bin/bash

# ESP32 Build Script
# Compiles the Arduino sketch to board-specific build directories
# Usage: ./build.sh [board-name]
#   - No parameter: Build all configured boards
#   - With board name: Build only that specific board

set -e

# Load common configuration and functions
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/config.sh"

# Get arduino-cli path
ARDUINO_CLI=$(find_arduino_cli)

# Parse command line arguments
TARGET_BOARD="$1"

# Build a single board
build_board() {
    local fqbn="$1"
    local board_name="$2"
    local board_build_path="$BUILD_PATH/$board_name"
    local board_override_dir="$SCRIPT_DIR/src/boards/$board_name"
    
    echo -e "${CYAN}=== Building for $board_name ===${NC}"
    echo "Board:     $board_name"
    echo "FQBN:      $fqbn"
    echo "Output:    $board_build_path"
    
    # Check for board-specific configuration overrides
    if [[ -d "$board_override_dir" ]]; then
        echo -e "${YELLOW}Config:    Using board-specific overrides from src/boards/$board_name/${NC}"
        EXTRA_FLAGS="--build-property compiler.cpp.extra_flags=-I\"$board_override_dir\""
    else
        echo "Config:    Using default configuration"
        EXTRA_FLAGS=""
    fi
    echo ""
    
    # Create board-specific build directory
    mkdir -p "$board_build_path"
    
    # Compile the sketch with optional board-specific includes
    if [[ -n "$EXTRA_FLAGS" ]]; then
        "$ARDUINO_CLI" compile \
            --fqbn "$fqbn" \
            $EXTRA_FLAGS \
            --output-dir "$board_build_path" \
            "$SKETCH_PATH"
    else
        "$ARDUINO_CLI" compile \
            --fqbn "$fqbn" \
            --output-dir "$board_build_path" \
            "$SKETCH_PATH"
    fi
    
    echo ""
    echo -e "${GREEN}✓ Build complete for $board_name${NC}"
    ls -lh "$board_build_path"/*.bin 2>/dev/null || echo "Binary files generated"
    echo ""
}

# Generate web assets (once for all builds)
echo "Generating web assets..."
"$SCRIPT_DIR/tools/minify-web-assets.sh" "$PROJECT_NAME" "$PROJECT_DISPLAY_NAME"
echo ""

# Determine which boards to build
if [[ -n "$TARGET_BOARD" ]]; then
    # Build specific board
    FQBN=$(get_fqbn_for_board "$TARGET_BOARD")
    if [[ -z "$FQBN" ]]; then
        echo -e "${RED}Error: Board '$TARGET_BOARD' not found${NC}"
        echo ""
        list_boards
        exit 1
    fi
    
    build_board "$FQBN" "$TARGET_BOARD"
else
    # Build all configured boards
    echo -e "${CYAN}=== Building ESP32 Firmware for All Boards ===${NC}"
    echo "Project:   $PROJECT_NAME"
    echo "Sketch:    $SKETCH_PATH"
    echo ""
    
    for fqbn in "${!FQBN_TARGETS[@]}"; do
        board_name=$(get_board_name "$fqbn")
        build_board "$fqbn" "$board_name"
    done
    
    echo -e "${GREEN}=== All Builds Complete ===${NC}"
fi
