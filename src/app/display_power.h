#ifndef DISPLAY_POWER_H
#define DISPLAY_POWER_H

#include <stdbool.h>
#include <stdint.h>

// Turn display/backlight off. Returns true if a display exists and was acted upon.
bool display_power_off();

// Turn display/backlight on (restoring prior brightness if available). Returns true if a display exists and was acted upon.
bool display_power_on();

// Returns true if the display/backlight is currently off (best effort for boards that support it).
bool display_power_is_off();

// Optional brightness control helpers (no-op on boards without support)
void display_power_set_brightness(uint8_t percent); // 0-100
uint8_t display_power_get_brightness(); // 0-100 (or 0 if unsupported)

#endif // DISPLAY_POWER_H
