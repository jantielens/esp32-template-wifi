#ifndef JC3636W518_DISPLAY_DRIVER_H
#define JC3636W518_DISPLAY_DRIVER_H

#include <lvgl.h>

void board_display_init();
void board_display_loop();

// Demo UI helpers
void display_register_demo_button_label(lv_obj_t *label);
void display_set_demo_caption(const char *text);

#endif // JC3636W518_DISPLAY_DRIVER_H
