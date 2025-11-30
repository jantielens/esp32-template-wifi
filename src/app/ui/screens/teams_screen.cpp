#include "teams_screen.h"
#include "../icons.h"
#include "../../BleKeyboard.h"
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
  lv_obj_add_event_cb(btn_mute_, button_event_cb, LV_EVENT_CLICKED, this);

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
  lv_obj_add_event_cb(btn_vol_up_, button_event_cb, LV_EVENT_CLICKED, this);

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
  lv_obj_add_event_cb(btn_camera_, button_event_cb, LV_EVENT_CLICKED, this);

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
  lv_obj_add_event_cb(btn_vol_down_, button_event_cb, LV_EVENT_CLICKED, this);

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
  lv_obj_add_event_cb(btn_end_call_, button_event_cb, LV_EVENT_CLICKED, this);
}

void TeamsScreen::button_event_cb(lv_event_t* e) {
  TeamsScreen* screen = static_cast<TeamsScreen*>(lv_event_get_user_data(e));
  lv_obj_t* btn = lv_event_get_target(e);
  screen->handleButtonPress(btn);
}

void TeamsScreen::handleButtonPress(lv_obj_t* btn) {
  extern BleKeyboard bleKeyboard;
  
  if (!bleKeyboard.isConnected()) {
    Serial.println("[BLE Keyboard] Not connected - please pair device");
    return;
  }
  
  if (btn == btn_mute_) {
    // Microsoft Teams: Ctrl+Shift+M to toggle mute
    Serial.println("[Teams] Mute button pressed - sending Ctrl+Shift+M");
    bleKeyboard.press(KEY_LEFT_CTRL);
    bleKeyboard.press(KEY_LEFT_SHIFT);
    bleKeyboard.press('m');
    delay(100);
    bleKeyboard.releaseAll();
    Serial.println("[BLE Keyboard] Sent Ctrl+Shift+M");
    
  } else if (btn == btn_camera_) {
    // Microsoft Teams: Ctrl+Shift+O to toggle video
    Serial.println("[Teams] Camera button pressed - sending Ctrl+Shift+O");
    bleKeyboard.press(KEY_LEFT_CTRL);
    bleKeyboard.press(KEY_LEFT_SHIFT);
    bleKeyboard.press('o');
    delay(100);
    bleKeyboard.releaseAll();
    Serial.println("[BLE Keyboard] Sent Ctrl+Shift+O");
    
  } else if (btn == btn_end_call_) {
    // Microsoft Teams: Ctrl+Shift+H to hang up
    Serial.println("[Teams] End call button pressed - sending Ctrl+Shift+H");
    bleKeyboard.press(KEY_LEFT_CTRL);
    bleKeyboard.press(KEY_LEFT_SHIFT);
    bleKeyboard.press('h');
    delay(100);
    bleKeyboard.releaseAll();
    Serial.println("[BLE Keyboard] Sent Ctrl+Shift+H");
    
  } else if (btn == btn_vol_up_) {
    // Volume Up media key
    Serial.println("[Teams] Volume up button pressed");
    bleKeyboard.write(KEY_MEDIA_VOLUME_UP);
    Serial.println("[BLE Keyboard] Sent Volume Up");
    
  } else if (btn == btn_vol_down_) {
    // Volume Down media key
    Serial.println("[Teams] Volume down button pressed");
    bleKeyboard.write(KEY_MEDIA_VOLUME_DOWN);
    Serial.println("[BLE Keyboard] Sent Volume Down");
  }
}

void TeamsScreen::handle(const UiEvent &evt) {
  // Event handling will be implemented later
  (void)evt;
}
