#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

source "$REPO_ROOT/config.sh"

OUT_DIR="${1:-$REPO_ROOT/site}"

# Only deploy “latest” (site output is overwritten each deploy)
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR/manifests" "$OUT_DIR/firmware"

# Prevent GitHub Pages from invoking Jekyll processing
: > "$OUT_DIR/.nojekyll"

get_version() {
  local major minor patch
  major=$(grep -E '^#define[[:space:]]+VERSION_MAJOR' "$REPO_ROOT/src/version.h" | grep -oE '[0-9]+' | head -1)
  minor=$(grep -E '^#define[[:space:]]+VERSION_MINOR' "$REPO_ROOT/src/version.h" | grep -oE '[0-9]+' | head -1)
  patch=$(grep -E '^#define[[:space:]]+VERSION_PATCH' "$REPO_ROOT/src/version.h" | grep -oE '[0-9]+' | head -1)
  echo "${major:-0}.${minor:-0}.${patch:-0}"
}

get_chip_family_for_fqbn() {
  local fqbn="$1"

  # Prefer the board id (3rd FQBN field) over matching the full string.
  # Example FQBNs:
  #   esp32:esp32:esp32
  #   esp32:esp32:esp32s3:FlashSize=16M,...
  #   esp32:esp32:nologo_esp32c3_super_mini:PartitionScheme=...,CDCOnBoot=...
  local board_id=""
  IFS=':' read -r _pkg _arch board_id _rest <<< "$fqbn"
  board_id="${board_id,,}"

  if [[ "$board_id" == *"esp32s3"* ]]; then
    echo "ESP32-S3"
  elif [[ "$board_id" == *"esp32s2"* ]]; then
    echo "ESP32-S2"
  elif [[ "$board_id" == *"esp32c6"* ]]; then
    echo "ESP32-C6"
  elif [[ "$board_id" == *"esp32c3"* ]]; then
    echo "ESP32-C3"
  elif [[ "$board_id" == *"esp32c2"* ]]; then
    echo "ESP32-C2"
  elif [[ "$board_id" == *"esp32h2"* ]]; then
    echo "ESP32-H2"
  else
    # Fallback for odd FQBN formats (keep behavior compatible).
    if [[ "$fqbn" == *"esp32s3"* ]]; then
      echo "ESP32-S3"
    elif [[ "$fqbn" == *"esp32s2"* ]]; then
      echo "ESP32-S2"
    elif [[ "$fqbn" == *"esp32c6"* ]]; then
      echo "ESP32-C6"
    elif [[ "$fqbn" == *"esp32c3"* ]]; then
      echo "ESP32-C3"
    elif [[ "$fqbn" == *"esp32c2"* ]]; then
      echo "ESP32-C2"
    elif [[ "$fqbn" == *"esp32h2"* ]]; then
      echo "ESP32-H2"
      return
    fi
    echo "ESP32"
  fi
}

is_beta_board() {
  local board_name="$1"
  # Conservative filter: exclude anything explicitly tagged beta/experimental.
  shopt -s nocasematch
  if [[ "$board_name" == *"beta"* ]] || [[ "$board_name" == *"experimental"* ]]; then
    return 0
  fi
  return 1
}

VERSION="$(get_version)"
SHA_SHORT="${GITHUB_SHA:-local}"
SHA_SHORT="${SHA_SHORT:0:7}"
SITE_VERSION="$VERSION+$SHA_SHORT"

# Build list of boards for the index
boards=()
for board_name in "${!FQBN_TARGETS[@]}"; do
  if is_beta_board "$board_name"; then
    echo "Skipping beta board: $board_name" >&2
    continue
  fi
  boards+=("$board_name")
done

# Sort for stable output
IFS=$'\n' boards=($(sort <<<"${boards[*]}"))
unset IFS

# Copy firmware + generate manifests
for board_name in "${boards[@]}"; do
  fqbn="${FQBN_TARGETS[$board_name]}"
  chip_family="$(get_chip_family_for_fqbn "$fqbn")"

  src_dir="$REPO_ROOT/build/$board_name"
  merged_bin="$src_dir/app.ino.merged.bin"

  if [[ ! -f "$merged_bin" ]]; then
    echo "ERROR: Missing merged firmware for $board_name at $merged_bin" >&2
    echo "Hint: run ./build.sh $board_name first" >&2
    exit 1
  fi

  dst_dir="$OUT_DIR/firmware/$board_name"
  mkdir -p "$dst_dir"

  # Stable filename; add cache-busting query param in manifest.
  cp "$merged_bin" "$dst_dir/firmware.bin"

  manifest_path="$OUT_DIR/manifests/$board_name.json"

  cat > "$manifest_path" <<EOF
{
  "name": "${PROJECT_DISPLAY_NAME} (${board_name})",
  "version": "${SITE_VERSION}",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "${chip_family}",
      "parts": [
        { "path": "../firmware/${board_name}/firmware.bin?v=${SHA_SHORT}", "offset": 0 }
      ]
    }
  ]
}
EOF

done

# Minimal index page (UI intentionally simple for now)
cat > "$OUT_DIR/index.html" <<EOF
<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>${PROJECT_DISPLAY_NAME} Firmware Installer</title>
    <style>
      body { font-family: system-ui, -apple-system, Segoe UI, Roboto, sans-serif; margin: 24px; max-width: 900px; }
      h1 { margin: 0 0 8px 0; }
      .muted { color: #666; }
      .board { padding: 12px 0; border-bottom: 1px solid #eee; display: flex; align-items: center; gap: 16px; }
      .board-name { width: 360px; font-weight: 600; }
      code { background: #f6f6f6; padding: 2px 6px; border-radius: 6px; }
    </style>
    <script type="module" src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"></script>
  </head>
  <body>
    <h1>${PROJECT_DISPLAY_NAME} Firmware Installer</h1>
    <p class="muted">Version: <code>${SITE_VERSION}</code>. Use Chrome/Edge over HTTPS.</p>

    <h2>Choose your board</h2>
EOF

for board_name in "${boards[@]}"; do
  cat >> "$OUT_DIR/index.html" <<EOF
    <div class="board">
      <div class="board-name">${board_name}</div>
      <esp-web-install-button manifest="./manifests/${board_name}.json"></esp-web-install-button>
    </div>
EOF

done

cat >> "$OUT_DIR/index.html" <<'EOF'

    <p class="muted">If you flash the wrong board variant, reflash with the correct one.</p>
  </body>
</html>
EOF

echo "Built ESP Web Tools site at: $OUT_DIR" >&2
echo "Manifests: $OUT_DIR/manifests" >&2
echo "Firmware:  $OUT_DIR/firmware" >&2
