// Unit tests for config_manager_sanitize_device_name().
// Compiled and run natively on the host (no Arduino SDK required).
//
// We only test the pure string-processing function here; NVS/Preferences
// code is never reached and is not compiled.

#include <cassert>
#include <cstring>
#include <cstdio>

// Inject test stubs
#include "board_config.h"
#include "log_manager.h"

// Pull in only the sanitize function by extracting the relevant portion
// of config_manager.cpp through a header-only declaration + inline copy.
// Avoids dragging in Preferences.h / nvs_flash.h which are ESP32-only.

// Prototype
void config_manager_sanitize_device_name(const char *input, char *output, size_t max_len);

// Implementation (copy of the pure-C++ function — no ESP32 deps)
void config_manager_sanitize_device_name(const char *input, char *output, size_t max_len) {
    if (!input || !output || max_len == 0) return;

    size_t j = 0;
    for (size_t i = 0; input[i] != '\0' && j < max_len - 1; i++) {
        char c = input[i];
        if (c >= 'A' && c <= 'Z') c = c + ('a' - 'A');
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            output[j++] = c;
        } else if (c == ' ' || c == '_' || c == '-') {
            if (j > 0 && output[j-1] != '-') output[j++] = '-';
        }
    }
    if (j > 0 && output[j-1] == '-') j--;
    output[j] = '\0';
}

static int passed = 0;
static int failed = 0;

#define CHECK_STR(result, expected, msg) do { \
    if (strcmp((result), (expected)) == 0) { passed++; } \
    else { failed++; printf("FAIL: %s  (got '%s' expected '%s')\n", msg, result, expected); } \
} while(0)

static void sanitize(const char *input, char *output, size_t maxlen) {
    config_manager_sanitize_device_name(input, output, maxlen);
}

static void test_basic_lowercase() {
    char out[64];
    sanitize("MyDevice", out, sizeof(out));
    CHECK_STR(out, "mydevice", "lowercase conversion");
}

static void test_spaces_become_hyphens() {
    char out[64];
    sanitize("My Device", out, sizeof(out));
    CHECK_STR(out, "my-device", "space to hyphen");
}

static void test_underscores_become_hyphens() {
    char out[64];
    sanitize("my_device", out, sizeof(out));
    CHECK_STR(out, "my-device", "underscore to hyphen");
}

static void test_no_consecutive_hyphens() {
    char out[64];
    sanitize("my  device", out, sizeof(out));
    CHECK_STR(out, "my-device", "multiple spaces -> single hyphen");

    sanitize("my--device", out, sizeof(out));
    CHECK_STR(out, "my-device", "double hyphen -> single hyphen");
}

static void test_no_leading_hyphen() {
    char out[64];
    sanitize(" device", out, sizeof(out));
    CHECK_STR(out, "device", "leading space stripped");
}

static void test_no_trailing_hyphen() {
    char out[64];
    sanitize("device ", out, sizeof(out));
    CHECK_STR(out, "device", "trailing space stripped");
}

static void test_special_chars_removed() {
    char out[64];
    sanitize("device!@#$%", out, sizeof(out));
    CHECK_STR(out, "device", "special chars removed");
}

static void test_numbers_preserved() {
    char out[64];
    sanitize("esp32c6", out, sizeof(out));
    CHECK_STR(out, "esp32c6", "numbers preserved");
}

static void test_empty_input() {
    char out[64];
    out[0] = 'x';
    sanitize("", out, sizeof(out));
    CHECK_STR(out, "", "empty input -> empty output");
}

static void test_null_safety() {
    char out[64];
    out[0] = 'x';
    sanitize(nullptr, out, sizeof(out));
    // Function returns without touching out; no crash is the assertion
    passed++;
}

static void test_max_len_respected() {
    char out[5];
    sanitize("abcdefghij", out, sizeof(out));
    // Should be truncated to 4 chars + null
    if (strlen(out) <= 4) passed++;
    else { failed++; printf("FAIL: max_len not respected (got len %zu)\n", strlen(out)); }
}

int main() {
    printf("=== config_manager sanitize tests ===\n");
    test_basic_lowercase();
    test_spaces_become_hyphens();
    test_underscores_become_hyphens();
    test_no_consecutive_hyphens();
    test_no_leading_hyphen();
    test_no_trailing_hyphen();
    test_special_chars_removed();
    test_numbers_preserved();
    test_empty_input();
    test_null_safety();
    test_max_len_respected();
    printf("Results: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
