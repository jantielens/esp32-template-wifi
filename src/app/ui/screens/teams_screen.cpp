#include "teams_screen.h"
#include "../icons.h"
#include <lvgl.h>

lv_obj_t* TeamsScreen::root() {
  if (!root_) build();
  return root_;
}

void TeamsScreen::build() {
  root_ = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(root_, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);

  // Center position for 360x360 display
  const int CENTER_X = 180;
  const int CENTER_Y = 180;
  const int RADIUS = 140;  // Distance from center to outer buttons (maximized for 360px display)

  // ===== CENTER BUTTON: Mute/Unmute (LARGE) =====
  btn_mute_ = lv_btn_create(root_);
  lv_obj_set_size(btn_mute_, 120, 120);
  lv_obj_align(btn_mute_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(btn_mute_, lv_color_hex(0x303030), 0);  // Light gray background
  lv_obj_set_style_radius(btn_mute_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_shadow_width(btn_mute_, 0, 0);  // Remove shadow
  
  lv_obj_t* img_mute = lv_img_create(btn_mute_);
  lv_img_set_src(img_mute, &icon_mic_64dp_ffffff_fill0_wght400_grad0_opsz48);
  lv_obj_center(img_mute);

  // ===== TOP: Volume Up (12 o'clock) =====
  btn_vol_up_ = lv_btn_create(root_);
  lv_obj_set_size(btn_vol_up_, 80, 80);
  lv_obj_align(btn_vol_up_, LV_ALIGN_CENTER, 0, -RADIUS);
  lv_obj_set_style_bg_color(btn_vol_up_, lv_color_hex(0x303030), 0);  // Light gray background
  lv_obj_set_style_radius(btn_vol_up_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_shadow_width(btn_vol_up_, 0, 0);  // Remove shadow
  
  lv_obj_t* img_vol_up = lv_img_create(btn_vol_up_);
  lv_img_set_src(img_vol_up, &icon_volume_up_48dp_ffffff_fill0_wght400_grad0_opsz48);
  lv_obj_center(img_vol_up);

  // ===== RIGHT: Camera On/Off (3 o'clock) =====
  btn_camera_ = lv_btn_create(root_);
  lv_obj_set_size(btn_camera_, 80, 80);
  lv_obj_align(btn_camera_, LV_ALIGN_CENTER, RADIUS, 0);
  lv_obj_set_style_bg_color(btn_camera_, lv_color_hex(0x303030), 0);  // Light gray background
  lv_obj_set_style_radius(btn_camera_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_shadow_width(btn_camera_, 0, 0);  // Remove shadow
  
  lv_obj_t* img_camera = lv_img_create(btn_camera_);
  lv_img_set_src(img_camera, &icon_camera_video_48dp_ffffff_fill0_wght400_grad0_opsz48);
  lv_obj_center(img_camera);

  // ===== BOTTOM: Volume Down (6 o'clock) =====
  btn_vol_down_ = lv_btn_create(root_);
  lv_obj_set_size(btn_vol_down_, 80, 80);
  lv_obj_align(btn_vol_down_, LV_ALIGN_CENTER, 0, RADIUS);
  lv_obj_set_style_bg_color(btn_vol_down_, lv_color_hex(0x303030), 0);  // Light gray background
  lv_obj_set_style_radius(btn_vol_down_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_shadow_width(btn_vol_down_, 0, 0);  // Remove shadow
  
  lv_obj_t* img_vol_down = lv_img_create(btn_vol_down_);
  lv_img_set_src(img_vol_down, &icon_volume_down_48dp_ffffff_fill0_wght400_grad0_opsz48);
  lv_obj_center(img_vol_down);

  // ===== LEFT: End Call (9 o'clock) =====
  btn_end_call_ = lv_btn_create(root_);
  lv_obj_set_size(btn_end_call_, 80, 80);
  lv_obj_align(btn_end_call_, LV_ALIGN_CENTER, -RADIUS, 0);
  lv_obj_set_style_bg_color(btn_end_call_, lv_color_hex(0x303030), 0);  // Light gray background
  lv_obj_set_style_radius(btn_end_call_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_shadow_width(btn_end_call_, 0, 0);  // Remove shadow
  
  lv_obj_t* img_end_call = lv_img_create(btn_end_call_);
  lv_img_set_src(img_end_call, &icon_call_end_48dp_ffffff_fill0_wght400_grad0_opsz48);
  lv_obj_center(img_end_call);
  // Force no anti-aliasing to prevent rendering artifacts
  lv_obj_set_style_img_opa(img_end_call, LV_OPA_COVER, 0);
}

void TeamsScreen::handle(const UiEvent &evt) {
  // Event handling will be implemented later
  (void)evt;
}
