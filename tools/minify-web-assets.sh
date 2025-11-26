#!/usr/bin/env bash
#
# Minify Web Assets and Generate web_assets.h
#
# This script dynamically discovers HTML, CSS, and JavaScript files in src/app/web/
# and generates a C header file with embedded assets for the ESP32 web server.
# CSS and JS files are minified; HTML files are embedded as-is.
#
# Usage: ./tools/minify-web-assets.sh
#

set -e  # Exit on error

# Resolve script directory for absolute paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Source and output paths
WEB_DIR="$PROJECT_ROOT/src/app/web"
OUTPUT_FILE="$PROJECT_ROOT/src/app/web_assets.h"

echo "=== Web Assets Minification ==="
echo "Project root: $PROJECT_ROOT"
echo "Web sources:  $WEB_DIR"
echo "Output:       $OUTPUT_FILE"
echo

# Check if web directory exists
if [ ! -d "$WEB_DIR" ]; then
    echo "Error: Web directory not found: $WEB_DIR"
    exit 1
fi

# Discover source files
HTML_FILES=($(find "$WEB_DIR" -maxdepth 1 -name "*.html" -type f | sort))
CSS_FILES=($(find "$WEB_DIR" -maxdepth 1 -name "*.css" -type f | sort))
JS_FILES=($(find "$WEB_DIR" -maxdepth 1 -name "*.js" -type f | sort))

if [ ${#HTML_FILES[@]} -eq 0 ] && [ ${#CSS_FILES[@]} -eq 0 ] && [ ${#JS_FILES[@]} -eq 0 ]; then
    echo "Error: No HTML, CSS, or JS files found in $WEB_DIR"
    exit 1
fi

echo "Found files:"
echo "  HTML: ${#HTML_FILES[@]} file(s)"
echo "  CSS:  ${#CSS_FILES[@]} file(s)"
echo "  JS:   ${#JS_FILES[@]} file(s)"
echo

# Check for Python 3
if ! command -v python3 &> /dev/null; then
    echo "Error: python3 is required but not installed"
    exit 1
fi

# Check/install required Python packages
if [ ${#HTML_FILES[@]} -gt 0 ] || [ ${#CSS_FILES[@]} -gt 0 ] || [ ${#JS_FILES[@]} -gt 0 ]; then
    echo "Checking Python dependencies..."
    
    if [ ${#CSS_FILES[@]} -gt 0 ] && ! python3 -c "import csscompressor" 2>/dev/null; then
        echo "Installing csscompressor..."
        python3 -m pip install --user csscompressor
    fi
    
    if [ ${#JS_FILES[@]} -gt 0 ] && ! python3 -c "import rjsmin" 2>/dev/null; then
        echo "Installing rjsmin..."
        python3 -m pip install --user rjsmin
    fi
    echo
fi

# Helper function to format bytes with percentage
format_size_savings() {
    local original=$1
    local processed=$2
    local type=$3
    
    if [ $original -eq $processed ]; then
        echo "  $type: $processed bytes (no compression)"
    else
        local saved=$((original - processed))
        local percent=$((saved * 100 / original))
        echo "  $type: $original → $processed bytes (saved $saved bytes, -${percent}%)"
    fi
}

# Arrays to store processed content and statistics
declare -A HTML_CONTENTS
declare -A CSS_CONTENTS
declare -A JS_CONTENTS
declare -A ORIGINAL_SIZES
declare -A PROCESSED_SIZES

# Process HTML files (basic minification without external dependencies)
for html_file in "${HTML_FILES[@]}"; do
    filename=$(basename "$html_file" .html)
    echo "Processing HTML: $filename.html..."
    content=$(cat "$html_file")
    original_size=$(echo -n "$content" | wc -c)
    
    # Basic minification: remove comments and normalize whitespace
    minified=$(python3 -c "
import re
with open('$html_file', 'r') as f:
    html = f.read()
    # Remove HTML comments
    html = re.sub(r'<!--.*?-->', '', html, flags=re.DOTALL)
    # Collapse multiple spaces/newlines to single space
    html = re.sub(r'\s+', ' ', html)
    # Remove spaces around tags
    html = re.sub(r'>\s+<', '><', html)
    # Trim
    html = html.strip()
    print(html, end='')
")
    
    HTML_CONTENTS["$filename"]="$minified"
    minified_size=$(echo -n "$minified" | wc -c)
    ORIGINAL_SIZES["html_$filename"]=$original_size
    PROCESSED_SIZES["html_$filename"]=$minified_size
done

# Process CSS files (minify)
for css_file in "${CSS_FILES[@]}"; do
    filename=$(basename "$css_file" .css)
    echo "Minifying CSS: $filename.css..."
    content=$(cat "$css_file")
    original_size=$(echo -n "$content" | wc -c)
    
    minified=$(python3 -c "
import csscompressor
with open('$css_file', 'r') as f:
    css = f.read()
    minified = csscompressor.compress(css)
    print(minified, end='')
")
    
    CSS_CONTENTS["$filename"]="$minified"
    minified_size=$(echo -n "$minified" | wc -c)
    ORIGINAL_SIZES["css_$filename"]=$original_size
    PROCESSED_SIZES["css_$filename"]=$minified_size
done

# Process JS files (minify)
for js_file in "${JS_FILES[@]}"; do
    filename=$(basename "$js_file" .js)
    echo "Minifying JS: $filename.js..."
    content=$(cat "$js_file")
    original_size=$(echo -n "$content" | wc -c)
    
    minified=$(python3 -c "
import rjsmin
with open('$js_file', 'r') as f:
    js = f.read()
    minified = rjsmin.jsmin(js)
    print(minified, end='')
")
    
    JS_CONTENTS["$filename"]="$minified"
    minified_size=$(echo -n "$minified" | wc -c)
    ORIGINAL_SIZES["js_$filename"]=$original_size
    PROCESSED_SIZES["js_$filename"]=$minified_size
done

echo

# Generate the header file
echo "Generating $OUTPUT_FILE..."

# Start header file
cat > "$OUTPUT_FILE" << 'HEADER_START'
/*
 * Web Assets - Embedded HTML/CSS/JS for ESP32 Web Server
 * 
 * *** AUTO-GENERATED FILE - DO NOT EDIT MANUALLY ***
 * 
 * This file is automatically generated by tools/minify-web-assets.sh
 * Source files are dynamically discovered in src/app/web/ directory.
 * 
 * Processing:
 *   - HTML files: basic minification (comments and whitespace removed)
 *   - CSS files:  minified using csscompressor
 *   - JS files:   minified using rjsmin
 * 
 * To modify web assets:
 *   1. Edit source files in src/app/web/
 *   2. Run ./build.sh (automatically runs minification)
 *   3. Upload new firmware to device
 */

#ifndef WEB_ASSETS_H
#define WEB_ASSETS_H

#include <Arduino.h>

HEADER_START

# Generate HTML sections
for filename in "${!HTML_CONTENTS[@]}"; do
    cat >> "$OUTPUT_FILE" << EOF
// HTML content from src/app/web/${filename}.html
const char ${filename}_html[] PROGMEM = R"rawliteral(
${HTML_CONTENTS[$filename]}
)rawliteral";

EOF
done

# Generate CSS sections
for filename in "${!CSS_CONTENTS[@]}"; do
    cat >> "$OUTPUT_FILE" << EOF
// CSS styles (minified) from src/app/web/${filename}.css
const char ${filename}_css[] PROGMEM = R"rawliteral(
${CSS_CONTENTS[$filename]}
)rawliteral";

EOF
done

# Generate JS sections
for filename in "${!JS_CONTENTS[@]}"; do
    cat >> "$OUTPUT_FILE" << EOF
// JavaScript (minified) from src/app/web/${filename}.js
const char ${filename}_js[] PROGMEM = R"rawliteral(
${JS_CONTENTS[$filename]}
)rawliteral";

EOF
done

# Add size constants (avoids strlen_P() overhead at runtime)
cat >> "$OUTPUT_FILE" << 'SIZE_CONSTANTS'

// Asset sizes (calculated at compile time, avoids strlen_P() overhead)
SIZE_CONSTANTS

for filename in "${!HTML_CONTENTS[@]}"; do
    echo "const size_t ${filename}_html_len = sizeof(${filename}_html) - 1;" >> "$OUTPUT_FILE"
done

for filename in "${!CSS_CONTENTS[@]}"; do
    echo "const size_t ${filename}_css_len = sizeof(${filename}_css) - 1;" >> "$OUTPUT_FILE"
done

for filename in "${!JS_CONTENTS[@]}"; do
    echo "const size_t ${filename}_js_len = sizeof(${filename}_js) - 1;" >> "$OUTPUT_FILE"
done

# Close header file
cat >> "$OUTPUT_FILE" << 'HEADER_END'

#endif // WEB_ASSETS_H
HEADER_END

# Display summary with statistics
echo "✓ Successfully generated web_assets.h"
echo
echo "Asset Summary:"

# Calculate totals
total_original=0
total_processed=0

for filename in "${!HTML_CONTENTS[@]}"; do
    key="html_$filename"
    format_size_savings ${ORIGINAL_SIZES[$key]} ${PROCESSED_SIZES[$key]} "HTML ${filename}.html"
    total_original=$((total_original + ORIGINAL_SIZES[$key]))
    total_processed=$((total_processed + PROCESSED_SIZES[$key]))
done

for filename in "${!CSS_CONTENTS[@]}"; do
    key="css_$filename"
    format_size_savings ${ORIGINAL_SIZES[$key]} ${PROCESSED_SIZES[$key]} "CSS  ${filename}.css"
    total_original=$((total_original + ORIGINAL_SIZES[$key]))
    total_processed=$((total_processed + PROCESSED_SIZES[$key]))
done

for filename in "${!JS_CONTENTS[@]}"; do
    key="js_$filename"
    format_size_savings ${ORIGINAL_SIZES[$key]} ${PROCESSED_SIZES[$key]} "JS   ${filename}.js"
    total_original=$((total_original + ORIGINAL_SIZES[$key]))
    total_processed=$((total_processed + PROCESSED_SIZES[$key]))
done

echo "  ───────────────────────────────────────────────────"
format_size_savings $total_original $total_processed "TOTAL"
echo
