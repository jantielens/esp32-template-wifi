#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#include <lvgl.h>
#include "base_screen.h"

enum class ScreenId : uint8_t {
  Splash = 0,
  Hello,
  SystemStats,
  Teams,
  // Add new screens here
};

class ScreenManager {
 public:
  void begin(ScreenId initial = ScreenId::Splash);
  void loop();
  void navigate(ScreenId id,
                lv_scr_load_anim_t anim = LV_SCR_LOAD_ANIM_NONE,
                uint32_t time = 300,
                uint32_t delay = 0);

  // Navigate to next/previous screen in swipeable sequence
  void navigateNext();
  void navigatePrevious();

  ScreenId currentId() const { return current_id_; }

 private:
  BaseScreen* current_ = nullptr;
  ScreenId current_id_ = ScreenId::Splash;
  lv_obj_t* current_root_ = nullptr;
  
  enum class PendingNav : uint8_t { None, Next, Previous };
  PendingNav pending_nav_ = PendingNav::None;
  unsigned long pending_nav_time_ = 0;

  static void gesture_event_cb(lv_event_t *e);
};

extern ScreenManager UI;

#endif // SCREEN_MANAGER_H
