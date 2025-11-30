#ifndef UI_SCREENSAVER_H
#define UI_SCREENSAVER_H

#include <stdint.h>
#include <stdbool.h>

// Call regularly (e.g., each loop) to manage screensaver state
void screensaver_update();

// Force wake (e.g., on user input while sleeping)
void screensaver_wake();

// Returns true if screensaver is currently active
bool screensaver_is_active();
bool screensaver_recently_activated(uint32_t ms);

// Returns the configured timeout (ms)
uint32_t screensaver_timeout_ms();

#endif // UI_SCREENSAVER_H
