/*
 * Log Buffer for Web Portal Streaming
 * 
 * Circular buffer that stores recent log entries with timestamps.
 * Thread-safe for concurrent access from logging and web server tasks.
 */

#ifndef LOG_BUFFER_H
#define LOG_BUFFER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Configuration
#define LOG_BUFFER_SIZE 50            // Number of log entries to keep (reduced to save memory)
#define LOG_ENTRY_MAX_LENGTH 200      // Max length per log entry

// Log entry structure
struct LogEntry {
    unsigned long timestamp_ms;       // Timestamp in milliseconds
    char message[LOG_ENTRY_MAX_LENGTH];
    uint16_t length;                  // Actual message length
};

// Circular log buffer
class LogBuffer {
public:
    LogBuffer();
    ~LogBuffer();
    
    // Add a log entry (thread-safe)
    void add(const char* message, size_t length);
    
    // Get all entries in chronological order (thread-safe)
    // Returns number of entries copied
    int getAll(LogEntry* entries, int maxEntries);
    
    // Get total number of entries currently in buffer
    int getCount();
    
    // Clear all entries
    void clear();
    
private:
    LogEntry buffer[LOG_BUFFER_SIZE];
    int head;                         // Next write position
    int count;                        // Number of entries (0 to LOG_BUFFER_SIZE)
    SemaphoreHandle_t mutex;          // Mutex for thread safety
};

#endif // LOG_BUFFER_H
