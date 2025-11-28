#ifndef BASE_SCREEN_H
#define BASE_SCREEN_H

#include <lvgl.h>
#include "ui_events.h"

class BaseScreen {
 public:
  virtual ~BaseScreen() = default;
  // Build (if needed) and return the root object for this screen.
  virtual lv_obj_t* root() = 0;
  virtual void onEnter() {}
  virtual void onExit() {}
  virtual void handle(const UiEvent &evt) {(void)evt;}
};

#endif // BASE_SCREEN_H
