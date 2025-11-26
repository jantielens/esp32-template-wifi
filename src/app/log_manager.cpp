/*
 * Log Manager Implementation
 * 
 * Indentation-based logger with nested blocks and automatic timing.
 * Routes output to both Serial and optional LogBuffer for web viewing.
 */

#include "log_manager.h"
#include <stdarg.h>

// Initialize static members
unsigned long LogManager::startTimes[3] = {0, 0, 0};
uint8_t LogManager::nestLevel = 0;

// Global LogManager instance
LogManager Logger;

// Constructor
LogManager::LogManager() : logBuffer(nullptr) {
    lineBuffer.reserve(MAX_LINE);
}

// Initialize (sets baud rate for Serial)
void LogManager::begin(unsigned long baud) {
    Serial.begin(baud);
}

// Set log buffer for web viewing
void LogManager::setLogBuffer(LogBuffer *buffer) {
    logBuffer = buffer;
}

// Get indentation string based on nesting level
const char* LogManager::indent() {
    static const char* indents[] = {
        "",         // Level 0: no indent
        "  ",       // Level 1: 2 spaces
        "    ",     // Level 2: 4 spaces
        "      "    // Level 3+: 6 spaces
    };
    
    uint8_t level = nestLevel;
    if (level > 3) level = 3; // Cap at 3 for indentation
    return indents[level];
}

// Begin a log block
void LogManager::logBegin(const char* module) {
    print(indent());
    print("[");
    print(module);
    println("] Starting...");
    
    // Save start time if we haven't exceeded max depth
    if (nestLevel < 3) {
        startTimes[nestLevel] = millis();
    }
    
    // Increment nesting level (but don't overflow)
    if (nestLevel < 255) {
        nestLevel++;
    }
}

// Add a line to current block
void LogManager::logLine(const char* message) {
    print(indent());
    println(message);
}

// Add a formatted line (printf-style)
void LogManager::logLinef(const char* format, ...) {
    char buffer[128];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    print(indent());
    println(buffer);
}

// End a log block
void LogManager::logEnd(const char* message) {
    // Decrement nesting level first (but don't underflow)
    if (nestLevel > 0) {
        nestLevel--;
    } else {
        // Extra end() calls are ignored gracefully
        return;
    }
    
    // Calculate elapsed time (0ms if we exceeded max depth)
    unsigned long elapsed = 0;
    if (nestLevel < 3) {
        elapsed = millis() - startTimes[nestLevel];
    }
    
    // Print end message with timing
    const char* msg = (message && strlen(message) > 0) ? message : "Done";
    print(indent());
    print(msg);
    print(" (");
    print(elapsed);
    println("ms)");
}

// Convenience methods for single-line messages
void LogManager::logMessage(const char* module, const char* msg) {
    logBegin(module);
    logLine(msg);
    logEnd();
}

void LogManager::logMessagef(const char* module, const char* format, ...) {
    logBegin(module);
    
    char buffer[128];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    logLine(buffer);
    logEnd();
}

// Write single byte (required by Print class)
size_t LogManager::write(uint8_t c) {
    // Always write to hardware Serial first
    size_t written = Serial.write(c);
    
    // Accumulate in line buffer for web logs
    if (c == '\n') {
        // Complete line - add to log buffer
        if (lineBuffer.length() > 0 && logBuffer) {
            logBuffer->add(lineBuffer.c_str(), lineBuffer.length());
            lineBuffer.clear();
        }
    } else if (c == '\r') {
        // Ignore carriage return
    } else {
        // Accumulate character
        if (lineBuffer.length() < 256) {
            lineBuffer += (char)c;
        }
    }
    
    return written;
}

// Write buffer of bytes (required by Print class)
size_t LogManager::write(const uint8_t *buffer, size_t size) {
    size_t written = 0;
    for (size_t i = 0; i < size; i++) {
        written += write(buffer[i]);
    }
    return written;
}
