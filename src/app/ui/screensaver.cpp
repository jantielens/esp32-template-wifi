#include "screensaver.h"
#include "../display_power.h"
#include "../log_manager.h"
#include <lvgl.h>

static constexpr uint32_t SCREENSAVER_TIMEOUT_MS = 20000; // 20s (testing)
static bool screensaver_active = false;
static uint32_t last_idle_bucket = 0;
static uint32_t screensaver_activated_ms = 0;

uint32_t screensaver_timeout_ms() {
  return SCREENSAVER_TIMEOUT_MS;
}

bool screensaver_is_active() {
  return screensaver_active;
}

bool screensaver_recently_activated(uint32_t ms) {
  if (!screensaver_active) return false;
  uint32_t now = millis();
  return (now - screensaver_activated_ms) < ms;
}

void screensaver_wake() {
  if (!screensaver_active) return;
  display_power_on();
  lv_disp_t *disp = lv_disp_get_default();
  if (disp) {
    // Reset LVGL inactivity so we don't immediately re-sleep
    lv_disp_trig_activity(disp);
  }
  screensaver_active = false;
  Logger.logQuickf("Screensaver", "wake");
}

void screensaver_update() {
  lv_disp_t *disp = lv_disp_get_default();
  if (!disp) return;

  uint32_t inactive_ms = lv_disp_get_inactive_time(disp);

  // Debug: log idle progress every 10s bucket before activation
  if (!screensaver_active) {
    uint32_t bucket = inactive_ms / 10000; // 10s buckets
    if (bucket != 0 && bucket != last_idle_bucket && inactive_ms < SCREENSAVER_TIMEOUT_MS) {
      Logger.logQuickf("Screensaver", "idle=%lus", (unsigned long)(inactive_ms / 1000));
      last_idle_bucket = bucket;
    }
  }

  if (!screensaver_active && inactive_ms >= SCREENSAVER_TIMEOUT_MS) {
    if (display_power_off()) {
      screensaver_active = true;
      screensaver_activated_ms = millis();
      Logger.logQuickf("Screensaver", "active (inactive=%lu)", (unsigned long)inactive_ms);
      last_idle_bucket = 0; // reset for next cycle
    } else {
      Logger.logQuickf("Screensaver", "power_off failed");
    }
  }
}
