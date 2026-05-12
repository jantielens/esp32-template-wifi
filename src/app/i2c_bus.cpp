#include "i2c_bus.h"

#include "log_manager.h"

static SemaphoreHandle_t g_i2c_mutex = nullptr;

void i2c_bus_init() {
	if (!g_i2c_mutex) {
		g_i2c_mutex = xSemaphoreCreateMutex();
		if (!g_i2c_mutex) {
			LOGE("I2C", "Failed to create bus mutex");
		}
	}
}

bool i2c_bus_lock(TickType_t timeout) {
	if (!g_i2c_mutex) return true; // mutex not initialized, allow through
	return xSemaphoreTake(g_i2c_mutex, timeout) == pdTRUE;
}

void i2c_bus_unlock() {
	if (!g_i2c_mutex) return;
	xSemaphoreGive(g_i2c_mutex);
}
