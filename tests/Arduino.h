#pragma once
// Arduino.h shim for host-native tests.
// Provides only types needed by the translation units under test.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

// Minimal String class for compilation only
// (tested functions use C strings; String is only needed for get_default_device_name)
#include <string>
struct String : public std::string {
    String() : std::string() {}
    String(const char *s) : std::string(s ? s : "") {}
    String(std::string s) : std::string(s) {}
    const char *c_str() const { return std::string::c_str(); }
    size_t length() const { return std::string::length(); }
};

inline unsigned long millis() { return 0; }
