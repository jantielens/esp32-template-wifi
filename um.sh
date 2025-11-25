#!/bin/bash

# ESP32 Upload & Monitor Script
# Convenience script for upload and monitor cycle
# Usage: ./um.sh [board-name] [port]
#   - board-name: Required when multiple boards configured
#   - port: Optional, auto-detected if not provided

set -e

# Load common configuration and functions
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/config.sh"

echo -e "${CYAN}=== ESP32 Upload & Monitor ===${NC}"
echo ""

# Parse arguments to determine board and port
BOARD_COUNT="${#FQBN_TARGETS[@]}"

if [[ $BOARD_COUNT -gt 1 ]]; then
    # Multiple boards: first arg is board name, second is optional port
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
    
    # Run upload → monitor
    "$SCRIPT_DIR/upload.sh" "$BOARD" "$PORT"
    echo ""
    "$SCRIPT_DIR/monitor.sh" "$PORT"
else
    # Single board: pass all args through
    "$SCRIPT_DIR/upload.sh" "$@"
    echo ""
    "$SCRIPT_DIR/monitor.sh" "$@"
fi
