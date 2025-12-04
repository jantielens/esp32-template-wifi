#include "hello_screen.h"
#include <lvgl.h>
#include <string.h>

void HelloScreen::hello_btn_event_cb(lv_event_t *e) {
  // Use BaseScreen's touch tracking helper (no need to pass tracker!)
  if (processTouchEvent(e)) {
    // Touch didn't move much - process as valid button click
    lv_obj_t *btn_label_inner = (lv_obj_t *)lv_event_get_user_data(e);
    lv_label_set_text(btn_label_inner, "i'm alive");
  }
}

lv_obj_t* HelloScreen::root() {
  if (!root_) build();
  return root_;
}

void HelloScreen::build() {
  root_ = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(root_, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);

  // Title label
  lv_obj_t *title = lv_label_create(root_);
  lv_label_set_text(title, "hello round world");
  lv_obj_align(title, LV_ALIGN_CENTER, 0, -40);

  // Button
  lv_obj_t *btn = lv_btn_create(root_);
  lv_obj_align(btn, LV_ALIGN_CENTER, 0, 20);

  // Button caption
  btn_label_ = lv_label_create(btn);
  lv_label_set_text(btn_label_, "click me");
  lv_obj_center(btn_label_);

  // Callback: update caption on click (with touch tracking to avoid swipe activation)
  addButtonEventCallbacks(btn, hello_btn_event_cb, btn_label_);
}

void HelloScreen::handle(const UiEvent &evt) {
  if (evt.type == UiEventType::DemoCaption && btn_label_) {
    lv_label_set_text(btn_label_, evt.msg);
  }
}
