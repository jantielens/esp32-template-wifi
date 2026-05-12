#pragma once

#include "board_config.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Shared mutex for Wire bus 0 (I2C) thread safety.
// Use when multiple peripherals share the same I2C bus and are accessed
// from different FreeRTOS tasks (e.g., touch controller + other I2C device).

// Create the Wire bus 0 mutex. Call once during setup() before any
// multi-task I2C usage begins.
void i2c_bus_init();

// Acquire exclusive access to Wire bus 0.
// Returns true if the lock was obtained within the timeout (default 50 ms).
bool i2c_bus_lock(TickType_t timeout = pdMS_TO_TICKS(50));

// Release Wire bus 0 access.
void i2c_bus_unlock();
