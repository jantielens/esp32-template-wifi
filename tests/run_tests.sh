#!/bin/bash
# run_tests.sh — build and run host-native unit tests
#
# Tests are compiled with g++ and run locally. No Arduino SDK or ESP32
# toolchain is required.
#
# Usage:
#   ./tests/run_tests.sh         # run all tests
#   ./tests/run_tests.sh -v      # verbose output

set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SRC="$REPO_ROOT/src/app"

VERBOSE=0
for arg in "$@"; do
    [[ "$arg" == "-v" ]] && VERBOSE=1
done

CXX="${CXX:-g++}"
CXXFLAGS="-std=c++17 -O0 -g -Wall -Wextra
    -I$SCRIPT_DIR
    -I$SRC
    -include $SCRIPT_DIR/log_manager.h
    -include $SCRIPT_DIR/board_config.h"

TMPDIR_="$(mktemp -d)"
trap 'rm -rf "$TMPDIR_"' EXIT

passed_suites=0
failed_suites=0

run_test() {
    local name="$1"
    local sources="${@:2}"
    local bin="$TMPDIR_/$name"

    if [[ "$VERBOSE" -eq 1 ]]; then
        echo "Compiling $name..."
    fi

    # shellcheck disable=SC2086
    if $CXX $CXXFLAGS -o "$bin" $sources "$SCRIPT_DIR/stubs.cpp" 2>&1; then
        local output
        output=$("$bin" 2>&1)
        local status=$?
        echo "$output"
        if [[ $status -eq 0 ]]; then
            passed_suites=$((passed_suites + 1))
        else
            failed_suites=$((failed_suites + 1))
        fi
    else
        echo "FAIL: $name — compile error"
        failed_suites=$((failed_suites + 1))
    fi
}

echo "=== Running unit tests ==="

run_test test_power_config \
    "$SCRIPT_DIR/test_power_config.cpp"

run_test test_config_sanitize \
    "$SCRIPT_DIR/test_config_sanitize.cpp"

echo ""
echo "=== Test suites: $passed_suites passed, $failed_suites failed ==="

[[ $failed_suites -eq 0 ]] && exit 0 || exit 1
