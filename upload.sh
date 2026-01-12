#!/bin/bash

# ESP32 Upload Script
# Uploads compiled firmware to ESP32 via serial port
# Usage: ./upload.sh [options] [board-name] [port]
#
# Options:
#   --full        Flash merged image at 0x0 (recommended when PartitionScheme is used)
#   --app-only    Flash only the app binary at the correct app offset (preserves partitions/NVS)
#   --erase-flash Erase entire flash before flashing (destructive)
#   --erase-nvs   Erase only the NVS partition before flashing (requires partition CSV)
#
# Board/port arguments:
#   - board-name: Required when multiple boards configured
#   - port: Optional, auto-detected if not provided

set -e

# Load common configuration and functions
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/config.sh"

# Get arduino-cli path
ARDUINO_CLI=$(find_arduino_cli)

usage() {
    cat <<EOF
Usage:
  ${0##*/} [options] [board-name] [port]

Options:
  --full        Flash merged image at 0x0 (recommended when PartitionScheme is used)
  --app-only    Flash only the app binary at the correct app offset
  --erase-flash Erase entire flash before flashing
  --erase-nvs   Erase only the NVS partition before flashing
  -h, --help    Show help
EOF
}

trim() {
    echo "$1" | xargs
}

get_fqbn_option() {
    local key="$1"
    local fqbn="$2"

    local _pkg _arch _board _opts
    IFS=':' read -r _pkg _arch _board _opts <<<"$fqbn"

    if [[ -z "${_opts:-}" ]]; then
        return 1
    fi

    local part
    IFS=',' read -ra parts <<<"$_opts"
    for part in "${parts[@]}"; do
        if [[ "$part" == "$key="* ]]; then
            echo "${part#*=}"
            return 0
        fi
    done
    return 1
}

get_chip_type_from_fqbn() {
    local fqbn="$1"

    # Extract chip type from the board segment of FQBN (3rd segment).
    # Examples:
    #  - esp32:esp32:esp32 -> esp32
    #  - esp32:esp32:esp32s3 -> esp32s3
    #  - esp32:esp32:nologo_esp32c3_super_mini -> esp32c3
    local board_seg
    board_seg=$(echo "$fqbn" | cut -d':' -f3)
    local chip
    chip=$(echo "$board_seg" | grep -oE 'esp32[a-z0-9]*' | head -n 1 || true)
    if [[ -z "$chip" ]]; then
        chip="esp32"
    fi
    echo "$chip"
}

find_esp32_core_dir() {
    local esp32_hw_base="$HOME/.arduino15/packages/esp32/hardware/esp32"
    if [[ ! -d "$esp32_hw_base" ]]; then
        return 1
    fi

    local esp32_dir
    esp32_dir="$(ls -1d "$esp32_hw_base"/*/ 2>/dev/null | sort -V | tail -n 1 || true)"
    esp32_dir="${esp32_dir%/}"
    if [[ -z "$esp32_dir" || ! -d "$esp32_dir" ]]; then
        return 1
    fi
    echo "$esp32_dir"
}

find_esptool() {
    local tools_dir="$HOME/.arduino15/packages/esp32/tools/esptool_py"

    # Newer ESP32 Arduino cores ship an `esptool` executable.
    local esptool_path
    esptool_path=$(find "$tools_dir" -name "esptool" -type f -executable 2>/dev/null | head -n 1 || true)
    if [[ -n "$esptool_path" ]]; then
        echo "$esptool_path"
        return 0
    fi

    # Older cores may ship `esptool.py`.
    esptool_path=$(find "$tools_dir" -name "esptool.py" -type f 2>/dev/null | head -n 1 || true)
    if [[ -n "$esptool_path" ]]; then
        echo "$esptool_path"
        return 0
    fi

    return 1
}

partition_csv_find_offsets() {
    local csv_path="$1"

    local app_offset=""
    local app_size=""
    local nvs_offset=""
    local nvs_size=""

    while IFS=',' read -r name type subtype offset size flags; do
        name=$(trim "${name:-}")
        type=$(trim "${type:-}")
        subtype=$(trim "${subtype:-}")
        offset=$(trim "${offset:-}")
        size=$(trim "${size:-}")

        [[ -z "$name" ]] && continue
        [[ "$name" == \#* ]] && continue

        if [[ -z "$app_offset" && "$type" == "app" ]]; then
            if [[ "$name" == "app0" || "$subtype" == "ota_0" || "$subtype" == "factory" ]]; then
                app_offset="$offset"
                app_size="$size"
            fi
        fi

        if [[ -z "$nvs_offset" ]]; then
            if [[ "$name" == "nvs" || ( "$type" == "data" && "$subtype" == "nvs" ) ]]; then
                nvs_offset="$offset"
                nvs_size="$size"
            fi
        fi
    done < "$csv_path"

    if [[ -z "$app_offset" ]]; then
        return 1
    fi

    echo "app_offset=$app_offset"
    echo "app_size=$app_size"
    if [[ -n "$nvs_offset" && -n "$nvs_size" ]]; then
        echo "nvs_offset=$nvs_offset"
        echo "nvs_size=$nvs_size"
    fi
}

get_partition_csv_path() {
    local board_build_path="$1"
    local fqbn="$2"

    # Prefer build output (if present) because it reflects exactly what was built.
    if [[ -f "$board_build_path/partitions.csv" ]]; then
        echo "$board_build_path/partitions.csv"
        return 0
    fi

    local scheme
    scheme=$(get_fqbn_option "PartitionScheme" "$fqbn" || true)
    if [[ -z "$scheme" ]]; then
        return 1
    fi

    local esp32_dir
    esp32_dir=$(find_esp32_core_dir || true)
    if [[ -z "$esp32_dir" ]]; then
        return 1
    fi

    local board_id
    board_id=$(echo "$fqbn" | cut -d':' -f3)

    local boards_txt="$esp32_dir/boards.txt"
    local partitions_dir="$esp32_dir/tools/partitions"

    if [[ -f "$boards_txt" ]]; then
        local partition_name_no_ext
        partition_name_no_ext=$(grep -E "^${board_id}\.menu\.PartitionScheme\.${scheme}\.build\.partitions=" "$boards_txt" | tail -n 1 | cut -d'=' -f2- | xargs || true)

        if [[ -n "$partition_name_no_ext" && -f "$partitions_dir/${partition_name_no_ext}.csv" ]]; then
            echo "$partitions_dir/${partition_name_no_ext}.csv"
            return 0
        fi
    fi

    # Fallback heuristics (some cores use scheme id as filename)
    if [[ -f "$partitions_dir/${scheme}.csv" ]]; then
        echo "$partitions_dir/${scheme}.csv"
        return 0
    fi
    if [[ -f "$partitions_dir/partitions_${scheme}.csv" ]]; then
        echo "$partitions_dir/partitions_${scheme}.csv"
        return 0
    fi

    return 1
}

parse_args() {
    MODE=""
    ERASE_FLASH=false
    ERASE_NVS=false

    REST_ARGS=()
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --full)
                MODE="full"
                shift
                ;;
            --app-only)
                MODE="app-only"
                shift
                ;;
            --erase-flash)
                ERASE_FLASH=true
                shift
                ;;
            --erase-nvs)
                ERASE_NVS=true
                shift
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            --)
                shift
                REST_ARGS+=("$@")
                break
                ;;
            -*)
                echo -e "${RED}Error: Unknown option: $1${NC}"
                usage
                exit 1
                ;;
            *)
                REST_ARGS+=("$1")
                shift
                ;;
        esac
    done
}

flash_with_esptool() {
    local chip_type="$1"
    local port="$2"
    local esptool_cmd="$3"
    shift 3

    if [[ "$esptool_cmd" == *.py ]]; then
        python3 "$esptool_cmd" --chip "$chip_type" --port "$port" "$@"
    else
        "$esptool_cmd" --chip "$chip_type" --port "$port" "$@"
    fi
}

parse_args "$@"

# Parse board and port arguments (positional after options)
parse_board_and_port_args "${REST_ARGS[@]}"

# Board-specific build directory
BOARD_BUILD_PATH="$BUILD_PATH/$BOARD"

# Auto-detect port if not specified
if [[ -z "$PORT" ]]; then
    if PORT=$(find_serial_port); then
        echo -e "${GREEN}Auto-detected port: $PORT${NC}"
    else
        echo -e "${RED}Error: No serial port detected${NC}"
        if [[ "${#FQBN_TARGETS[@]}" -gt 1 ]]; then
            echo "Usage: ${0##*/} <board-name> [port]"
            echo "Example: ${0##*/} $BOARD /dev/ttyUSB0"
        else
            echo "Usage: ${0##*/} [port]"
            echo "Example: ${0##*/} /dev/ttyUSB0"
        fi
        exit 1
    fi
fi

echo -e "${CYAN}=== Uploading ESP32 Firmware ===${NC}"

# Check if board build directory exists
if [ ! -d "$BOARD_BUILD_PATH" ]; then
    echo -e "${RED}Error: Build directory not found for board '$BOARD'${NC}"
    echo "Expected: $BOARD_BUILD_PATH"
    echo "Please run: ./build.sh $BOARD"
    exit 1
fi

# Display upload configuration
echo "Board: $BOARD"
echo "FQBN:  $FQBN"
echo "Port:  $PORT"
echo "Build: $BOARD_BUILD_PATH"
echo ""

PARTITION_SCHEME=$(get_fqbn_option "PartitionScheme" "$FQBN" || true)
if [[ -z "${MODE}" ]]; then
    if [[ -n "$PARTITION_SCHEME" ]]; then
        MODE="full"
    else
        MODE="arduino"
    fi
fi

echo "Mode:  $MODE"
if [[ -n "$PARTITION_SCHEME" ]]; then
    echo "PartitionScheme: $PARTITION_SCHEME"
fi

CHIP_TYPE=$(get_chip_type_from_fqbn "$FQBN")
ESPTOOL_CMD=""
if [[ "$MODE" == "full" || "$MODE" == "app-only" || "$ERASE_FLASH" == "true" || "$ERASE_NVS" == "true" ]]; then
    ESPTOOL_CMD=$(find_esptool || true)
    if [[ -z "$ESPTOOL_CMD" ]]; then
        echo -e "${RED}Error: esptool not found${NC}"
        echo "Please ensure ESP32 platform is installed (run setup.sh)"
        exit 1
    fi
fi

PARTITION_CSV=""
PART_INFO=""
if [[ "$MODE" == "app-only" || "$ERASE_NVS" == "true" ]]; then
    PARTITION_CSV=$(get_partition_csv_path "$BOARD_BUILD_PATH" "$FQBN" || true)
    if [[ -z "$PARTITION_CSV" ]]; then
        echo -e "${RED}Error: Could not locate partition CSV to derive offsets${NC}"
        echo "Tried build output and installed ESP32 core PartitionScheme mapping."
        echo "Hint: run ./tools/install-custom-partitions.sh and rebuild." 
        exit 1
    fi

    PART_INFO=$(partition_csv_find_offsets "$PARTITION_CSV" || true)
    if [[ -z "$PART_INFO" ]]; then
        echo -e "${RED}Error: Failed to parse partition CSV for app offset${NC}"
        echo "CSV: $PARTITION_CSV"
        exit 1
    fi
fi

if [[ "$ERASE_FLASH" == "true" ]]; then
    echo -e "${YELLOW}Erasing entire flash...${NC}"
    flash_with_esptool "$CHIP_TYPE" "$PORT" "$ESPTOOL_CMD" erase_flash
fi

if [[ "$ERASE_NVS" == "true" ]]; then
    local_nvs_offset=$(echo "$PART_INFO" | grep '^nvs_offset=' | cut -d'=' -f2- || true)
    local_nvs_size=$(echo "$PART_INFO" | grep '^nvs_size=' | cut -d'=' -f2- || true)
    if [[ -z "$local_nvs_offset" || -z "$local_nvs_size" ]]; then
        echo -e "${RED}Error: NVS partition not found in partition CSV${NC}"
        echo "CSV: $PARTITION_CSV"
        exit 1
    fi
    echo -e "${YELLOW}Erasing NVS region ($local_nvs_offset, $local_nvs_size)...${NC}"
    flash_with_esptool "$CHIP_TYPE" "$PORT" "$ESPTOOL_CMD" erase_region "$local_nvs_offset" "$local_nvs_size"
fi

if [[ "$MODE" == "arduino" ]]; then
    "$ARDUINO_CLI" upload \
        --fqbn "$FQBN" \
        --port "$PORT" \
        --input-dir "$BOARD_BUILD_PATH"
elif [[ "$MODE" == "full" ]]; then
    MERGED_BIN="$BOARD_BUILD_PATH/app.ino.merged.bin"
    if [[ ! -f "$MERGED_BIN" ]]; then
        echo -e "${RED}Error: merged firmware not found${NC}"
        echo "Expected: $MERGED_BIN"
        echo "Please run: ./build.sh $BOARD"
        exit 1
    fi

    echo -e "${YELLOW}Flashing merged image at 0x0 (full)...${NC}"
    flash_with_esptool "$CHIP_TYPE" "$PORT" "$ESPTOOL_CMD" write-flash -z 0x0 "$MERGED_BIN"
elif [[ "$MODE" == "app-only" ]]; then
    APP_BIN="$BOARD_BUILD_PATH/app.ino.bin"
    if [[ ! -f "$APP_BIN" ]]; then
        echo -e "${RED}Error: app binary not found${NC}"
        echo "Expected: $APP_BIN"
        echo "Please run: ./build.sh $BOARD"
        exit 1
    fi

    APP_OFFSET=$(echo "$PART_INFO" | grep '^app_offset=' | cut -d'=' -f2-)
    echo -e "${YELLOW}Flashing app at $APP_OFFSET (app-only)...${NC}"
    flash_with_esptool "$CHIP_TYPE" "$PORT" "$ESPTOOL_CMD" write-flash -z "$APP_OFFSET" "$APP_BIN"
else
    echo -e "${RED}Error: Unknown mode: $MODE${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}=== Upload Complete ===${NC}"
echo "Run ./monitor.sh to view serial output"
