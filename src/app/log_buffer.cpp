/*
 * Log Buffer Implementation
 */

#include "log_buffer.h"
#include <string.h>

LogBuffer::LogBuffer() : head(0), count(0) {
    mutex = xSemaphoreCreateMutex();
    memset(buffer, 0, sizeof(buffer));
}

LogBuffer::~LogBuffer() {
    if (mutex) {
        vSemaphoreDelete(mutex);
    }
}

void LogBuffer::add(const char* message, size_t length) {
    if (!message || length == 0) return;
    
    // Take mutex with shorter timeout (10ms instead of 100ms)
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return; // Timeout, skip this log entry
    }
    
    // Limit length to buffer size
    size_t copyLen = (length < LOG_ENTRY_MAX_LENGTH - 1) ? length : (LOG_ENTRY_MAX_LENGTH - 1);
    
    // Store entry
    buffer[head].timestamp_ms = millis();
    memcpy(buffer[head].message, message, copyLen);
    buffer[head].message[copyLen] = '\0';
    buffer[head].length = copyLen;
    
    // Advance head (circular)
    head = (head + 1) % LOG_BUFFER_SIZE;
    
    // Update count (max = LOG_BUFFER_SIZE)
    if (count < LOG_BUFFER_SIZE) {
        count++;
    }
    
    // Release mutex
    xSemaphoreGive(mutex);
}

int LogBuffer::getAll(LogEntry* entries, int maxEntries) {
    if (!entries || maxEntries <= 0) return 0;
    
    // Take mutex with shorter timeout (10ms instead of 100ms)
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return 0; // Timeout - log buffer busy
    }
    
    int copyCount = (count < maxEntries) ? count : maxEntries;
    
    if (copyCount == 0) {
        xSemaphoreGive(mutex);
        return 0;
    }
    
    // Calculate start position (oldest entry)
    int startPos;
    if (count < LOG_BUFFER_SIZE) {
        // Buffer not full yet, start from 0
        startPos = 0;
    } else {
        // Buffer full, oldest entry is at head position
        startPos = head;
    }
    
    // Copy entries in chronological order
    for (int i = 0; i < copyCount; i++) {
        int srcIdx = (startPos + i) % LOG_BUFFER_SIZE;
        memcpy(&entries[i], &buffer[srcIdx], sizeof(LogEntry));
    }
    
    xSemaphoreGive(mutex);
    return copyCount;
}

int LogBuffer::getCount() {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return 0;
    }
    int result = count;
    xSemaphoreGive(mutex);
    return result;
}

void LogBuffer::clear() {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return;
    }
    head = 0;
    count = 0;
    memset(buffer, 0, sizeof(buffer));
    xSemaphoreGive(mutex);
}
