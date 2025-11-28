#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#include <lvgl.h>
#include "base_screen.h"

enum class ScreenId : uint8_t {
  Hello = 0,
  // Add new screens here
};

class ScreenManager {
 public:
  void begin(ScreenId initial = ScreenId::Hello);
  void loop();
  void navigate(ScreenId id,
                lv_scr_load_anim_t anim = LV_SCR_LOAD_ANIM_NONE,
                uint32_t time = 300,
                uint32_t delay = 0);

  ScreenId currentId() const { return current_id_; }

 private:
  BaseScreen* current_ = nullptr;
  ScreenId current_id_ = ScreenId::Hello;
};

extern ScreenManager UI;

#endif // SCREEN_MANAGER_H
