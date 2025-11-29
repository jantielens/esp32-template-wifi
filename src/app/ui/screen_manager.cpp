#include "screen_manager.h"
#include "ui_events.h"
#include "screens/splash_screen.h"
#include "screens/hello_screen.h"
#include "screens/system_stats_screen.h"
#include "screens/teams_screen.h"
#include "../log_manager.h"

// Define swipeable screen sequence (Splash is not swipeable)
static const ScreenId SWIPEABLE_SCREENS[] = {
  ScreenId::SystemStats,
  ScreenId::Hello,
  ScreenId::Teams,
  // Add more screens here as needed
};
static const size_t SWIPEABLE_SCREEN_COUNT = sizeof(SWIPEABLE_SCREENS) / sizeof(SWIPEABLE_SCREENS[0]);

static BaseScreen* get_screen(ScreenId id) {
  switch (id) {
    case ScreenId::Splash: {
      static SplashScreen screen;
      return &screen;
    }
    case ScreenId::Hello: {
      static HelloScreen screen;
      return &screen;
    }
    case ScreenId::SystemStats: {
      static SystemStatsScreen screen;
      return &screen;
    }
    case ScreenId::Teams: {
      static TeamsScreen screen;
      return &screen;
    }
    default:
      return nullptr;
  }
}

static const char* screen_id_name(ScreenId id) {
  switch (id) {
    case ScreenId::Splash: return "Splash";
    case ScreenId::Hello: return "Hello";
    case ScreenId::SystemStats: return "SystemStats";
    case ScreenId::Teams: return "Teams";
    default: return "Unknown";
  }
}

void ScreenManager::gesture_event_cb(lv_event_t *e) {
  // Ignore gestures if navigation already pending
  if (UI.pending_nav_ != PendingNav::None) {
    return;
  }
  
  lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
  
  // Set pending navigation with delay - wait for LVGL to finish event processing
  if (dir == LV_DIR_LEFT) {
    Logger.logMessage("Gesture", "Swipe LEFT -> Next");
    UI.pending_nav_ = PendingNav::Next;
    UI.pending_nav_time_ = millis();
  } else if (dir == LV_DIR_RIGHT) {
    Logger.logMessage("Gesture", "Swipe RIGHT -> Previous");
    UI.pending_nav_ = PendingNav::Previous;
    UI.pending_nav_time_ = millis();
  }
}

ScreenManager UI;

void ScreenManager::begin(ScreenId initial) {
  current_id_ = initial;
  current_ = get_screen(initial);
  if (!current_) return;
  Logger.logMessagef("UI", "Screen begin: %s", screen_id_name(initial));
  current_root_ = current_->root();
  if (current_root_) {
    lv_scr_load(current_root_);
    current_->onEnter();
    
    // Setup gesture detection on screen root for swipeable screens
    if (initial != ScreenId::Splash) {
      lv_obj_add_event_cb(current_root_, gesture_event_cb, LV_EVENT_GESTURE, nullptr);
      lv_obj_clear_flag(current_root_, LV_OBJ_FLAG_GESTURE_BUBBLE);
      Logger.logMessage("UI", "Gesture detection enabled");
    }
  }
}

void ScreenManager::navigate(ScreenId id,
                             lv_scr_load_anim_t anim,
                             uint32_t time,
                             uint32_t delay) {
  if (id == current_id_) return;
  BaseScreen* next = get_screen(id);
  if (!next) return;

  if (current_) current_->onExit();

  lv_obj_t* root = next->root();
  if (root) {
    // DON'T auto-delete previous screen - let it finish event processing
    lv_scr_load_anim(root, anim, time, delay, false /*auto_del_prev*/);
  }

  current_ = next;
  current_id_ = id;
  current_root_ = root;
  Logger.logMessagef("UI", "Navigate -> %s", screen_id_name(id));
  current_->onEnter();

  // Setup gesture detection on new screen root
  if (id != ScreenId::Splash && root) {
    lv_obj_add_event_cb(root, gesture_event_cb, LV_EVENT_GESTURE, nullptr);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_GESTURE_BUBBLE);
    Logger.logMessage("UI", "Gesture detection enabled");
  }
}

void ScreenManager::navigateNext() {
  // Find current screen in swipeable sequence
  for (size_t i = 0; i < SWIPEABLE_SCREEN_COUNT; i++) {
    if (SWIPEABLE_SCREENS[i] == current_id_) {
      size_t next_idx = (i + 1) % SWIPEABLE_SCREEN_COUNT;
      navigate(SWIPEABLE_SCREENS[next_idx], LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0);
      return;
    }
  }
}

void ScreenManager::navigatePrevious() {
  // Find current screen in swipeable sequence
  for (size_t i = 0; i < SWIPEABLE_SCREEN_COUNT; i++) {
    if (SWIPEABLE_SCREENS[i] == current_id_) {
      size_t prev_idx = (i == 0) ? (SWIPEABLE_SCREEN_COUNT - 1) : (i - 1);
      navigate(SWIPEABLE_SCREENS[prev_idx], LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0);
      return;
    }
  }
}

void ScreenManager::loop() {
  if (!current_) return;
  
  // Process UI events first
  UiEvent evt;
  while (ui_poll(&evt)) {
    current_->handle(evt);
  }
  
  // Process pending navigation after delay (ensures LVGL fully completes event processing)
  if (pending_nav_ != PendingNav::None) {
    if (millis() - pending_nav_time_ >= 50) { // 50ms delay
      PendingNav nav = pending_nav_;
      pending_nav_ = PendingNav::None;
      
      if (nav == PendingNav::Next) {
        navigateNext();
      } else if (nav == PendingNav::Previous) {
        navigatePrevious();
      }
    }
  }
}
