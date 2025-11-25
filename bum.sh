#!/bin/bash

# ESP32 Build, Upload & Monitor Script
# Convenience script for the complete development cycle
# Usage: ./bum.sh [board-name] [port]
#   - board-name: Required when multiple boards configured
#   - port: Optional, auto-detected if not provided

set -e

# Load common configuration and functions
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/config.sh"

echo -e "${CYAN}=== ESP32 Full Cycle: Build, Upload & Monitor ===${NC}"
echo ""

# Parse arguments to determine board and port
BOARD_COUNT="${#FQBN_TARGETS[@]}"

if [[ $BOARD_COUNT -gt 1 ]]; then
    # Multiple boards: first arg is board name
    if [[ -z "$1" ]]; then
        echo -e "${RED}Error: Board name required when multiple boards are configured${NC}"
        echo ""
        list_boards
        echo ""
        echo "Usage: ${0##*/} <board-name> [port]"
        exit 1
    fi
    BOARD="$1"
    PORT="$2"
    
    # Run build → upload → monitor
    "$SCRIPT_DIR/build.sh" "$BOARD"
    echo ""
    "$SCRIPT_DIR/upload.sh" "$BOARD" "$PORT"
    echo ""
    "$SCRIPT_DIR/monitor.sh" "$PORT"
else
    # Single board: pass all args through
    "$SCRIPT_DIR/build.sh"
    echo ""
    "$SCRIPT_DIR/upload.sh" "$@"
    echo ""
    "$SCRIPT_DIR/monitor.sh" "$@"
fi
