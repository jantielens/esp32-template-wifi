#include "display_power.h"

// Include board-specific config when compiled with a board macro; fallback to default
#if defined(BOARD_JC3636W518)
#include "../boards/jc3636w518/board_config.h"
#include "../boards/jc3636w518/display_driver.h"
#else
#include "board_config.h"
#endif

bool display_power_off() {
#if defined(BOARD_JC3636W518)
  display_backlight_off();
  display_panel_off();
  return true;
#else
  return false;
#endif
}

bool display_power_on() {
#if defined(BOARD_JC3636W518)
  display_panel_on();
  display_backlight_on();
  return true;
#else
  return false;
#endif
}

bool display_power_is_off() {
#if defined(BOARD_JC3636W518)
  return !display_backlight_is_on();
#else
  return false;
#endif
}

void display_power_set_brightness(uint8_t percent) {
#if defined(BOARD_JC3636W518)
  display_backlight_set_brightness(percent);
#else
  (void)percent;
#endif
}

uint8_t display_power_get_brightness() {
#if defined(BOARD_JC3636W518)
  return display_backlight_get_brightness();
#else
  return 0;
#endif
}
