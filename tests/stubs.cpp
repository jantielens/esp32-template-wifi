// Minimal Arduino / ESP32 stubs for host-native unit tests.
// Provides only what the tested source files actually need.

#include <string.h>

// strlcpy is a BSD extension not present on all Linux glibc builds.
#ifndef HAVE_STRLCPY
size_t strlcpy(char *dst, const char *src, size_t size) {
    if (!dst || size == 0) return src ? strlen(src) : 0;
    size_t len = strlen(src);
    size_t copy = len < size - 1 ? len : size - 1;
    memcpy(dst, src, copy);
    dst[copy] = '\0';
    return len;
}
#endif
