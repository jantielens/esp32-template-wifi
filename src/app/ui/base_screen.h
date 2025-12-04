#ifndef BASE_SCREEN_H
#define BASE_SCREEN_H

#include <lvgl.h>
#include "ui_events.h"
#include <cmath>

// Touch movement threshold (pixels) - ignore button clicks if touch moved this much
#define TOUCH_MOVE_THRESHOLD 30

class BaseScreen {
 public:
  virtual ~BaseScreen() = default;
  // Build (if needed) and return the root object for this screen.
  virtual lv_obj_t* root() = 0;
  virtual void onEnter() {}
  virtual void onExit() {}
  virtual void handle(const UiEvent &evt) {(void)evt;}

 protected:
  // Touch tracking for swipe detection to avoid unwanted button presses
  struct TouchTracker {
    bool is_pressed = false;
    bool was_swipe = false;  // Flag to indicate touch moved beyond threshold
    int16_t start_x = 0;
    int16_t start_y = 0;
    int16_t current_x = 0;
    int16_t current_y = 0;
    
    void reset() {
      is_pressed = false;
      was_swipe = false;
      start_x = start_y = current_x = current_y = 0;
    }
    
    int16_t distance() const {
      int32_t dx = current_x - start_x;
      int32_t dy = current_y - start_y;
      return (int16_t)sqrt(dx*dx + dy*dy);
    }
  };
  
  // Global touch tracker shared by all screens
  static TouchTracker touch_tracker_;
  
  // Helper to add button with touch-aware event handling
  // Automatically filters out clicks that moved more than TOUCH_MOVE_THRESHOLD
  // Child screens can call this directly without managing their own tracker
  static void addButtonEventCallbacks(lv_obj_t* btn, lv_event_cb_t callback, void* user_data) {
    lv_obj_add_event_cb(btn, callback, LV_EVENT_PRESSED, user_data);
    lv_obj_add_event_cb(btn, callback, LV_EVENT_PRESSING, user_data);
    lv_obj_add_event_cb(btn, callback, LV_EVENT_RELEASED, user_data);
    lv_obj_add_event_cb(btn, callback, LV_EVENT_CLICKED, user_data);
  }
  
  // Process touch events and update tracker
  // Returns true if CLICKED event should be processed (touch didn't move much)
  // Uses the global touch_tracker_ - no need to pass tracker parameter
  static bool processTouchEvent(lv_event_t* e) {
    TouchTracker& tracker = touch_tracker_;
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_PRESSED) {
      lv_indev_t* indev = lv_indev_get_act();
      lv_point_t point;
      lv_indev_get_point(indev, &point);
      tracker.is_pressed = true;
      tracker.was_swipe = false;
      tracker.start_x = point.x;
      tracker.start_y = point.y;
      tracker.current_x = point.x;
      tracker.current_y = point.y;
      return false;
    } else if (code == LV_EVENT_PRESSING) {
      lv_indev_t* indev = lv_indev_get_act();
      lv_point_t point;
      lv_indev_get_point(indev, &point);
      tracker.current_x = point.x;
      tracker.current_y = point.y;
      
      // Mark as swipe if movement exceeds threshold
      if (tracker.distance() >= TOUCH_MOVE_THRESHOLD) {
        tracker.was_swipe = true;
      }
      return false;
    } else if (code == LV_EVENT_RELEASED) {
      // Final check - mark as swipe if movement threshold exceeded
      if (tracker.distance() >= TOUCH_MOVE_THRESHOLD) {
        tracker.was_swipe = true;
      }
      return false;
    } else if (code == LV_EVENT_CLICKED) {
      // Only process click if it wasn't a swipe
      bool should_process = !tracker.was_swipe;
      tracker.reset();
      return should_process;
    }
    return false;
  }
};

#endif // BASE_SCREEN_H
