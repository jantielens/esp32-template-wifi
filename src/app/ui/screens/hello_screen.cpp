#include "hello_screen.h"
#include <lvgl.h>
#include <string.h>

static void hello_btn_event_cb(lv_event_t *e) {
  lv_obj_t *btn_label_inner = (lv_obj_t *)lv_event_get_user_data(e);
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
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

  // Callback: update caption on click
  lv_obj_add_event_cb(btn, hello_btn_event_cb, LV_EVENT_CLICKED, btn_label_);
}

void HelloScreen::handle(const UiEvent &evt) {
  if (evt.type == UiEventType::DemoCaption && btn_label_) {
    lv_label_set_text(btn_label_, evt.msg);
  }
}
