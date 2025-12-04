#include "teams_screen.h"
#include "../icons.h"
#include "../../BleKeyboard.h"
#include "../../log_manager.h"
#include <lvgl.h>

// HID Keyboard scan codes (layout-independent physical key positions)
// These represent the actual key positions, not characters
const uint8_t HID_KEY_M = 0x10;  // M key
const uint8_t HID_KEY_O = 0x12;  // O key
const uint8_t HID_KEY_H = 0x0B;  // H key
const uint8_t HID_KEY_E = 0x08;  // E key
const uint8_t HID_KEY_K = 0x0E;  // K key

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
  const int RADIUS_DIAG = (int)(RADIUS * 0.7071f + 0.5f); // ~R / sqrt(2)

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
  addButtonEventCallbacks(btn_mute_, button_event_cb, this);

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
  addButtonEventCallbacks(btn_vol_up_, button_event_cb, this);

  // ===== UPPER-RIGHT: Raise Hand (between 12 and 3 o'clock) =====
  btn_raise_hand_ = lv_btn_create(root_);
  lv_obj_set_size(btn_raise_hand_, 80, 80);
  lv_obj_align(btn_raise_hand_, LV_ALIGN_CENTER, RADIUS_DIAG, -RADIUS_DIAG);
  lv_obj_set_style_bg_color(btn_raise_hand_, lv_color_hex(0x303030), 0);
  lv_obj_set_style_radius(btn_raise_hand_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_shadow_width(btn_raise_hand_, 0, 0);

  lv_obj_t* img_raise = lv_img_create(btn_raise_hand_);
  lv_img_set_src(img_raise, &icon_person_raised_hand_48dp_ffffff_fill0_wght400_grad0_opsz48);
  lv_obj_center(img_raise);
  addButtonEventCallbacks(btn_raise_hand_, button_event_cb, this);

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
  addButtonEventCallbacks(btn_camera_, button_event_cb, this);

  // ===== UPPER-LEFT: Share Screen (between 12 and 9 o'clock) =====
  btn_share_screen_ = lv_btn_create(root_);
  lv_obj_set_size(btn_share_screen_, 80, 80);
  lv_obj_align(btn_share_screen_, LV_ALIGN_CENTER, -RADIUS_DIAG, -RADIUS_DIAG);
  lv_obj_set_style_bg_color(btn_share_screen_, lv_color_hex(0x303030), 0);
  lv_obj_set_style_radius(btn_share_screen_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_shadow_width(btn_share_screen_, 0, 0);

  lv_obj_t* img_share = lv_img_create(btn_share_screen_);
  lv_img_set_src(img_share, &icon_present_to_all_48dp_ffffff_fill0_wght400_grad0_opsz48);
  lv_obj_center(img_share);
  addButtonEventCallbacks(btn_share_screen_, button_event_cb, this);

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
  addButtonEventCallbacks(btn_vol_down_, button_event_cb, this);

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
  addButtonEventCallbacks(btn_end_call_, button_event_cb, this);
}

void TeamsScreen::button_event_cb(lv_event_t* e) {
  TeamsScreen* screen = static_cast<TeamsScreen*>(lv_event_get_user_data(e));
  
  // Use BaseScreen's touch tracking helper (no need to pass tracker!)
  if (processTouchEvent(e)) {
    // Touch didn't move much - process as valid button click
    lv_obj_t* btn = lv_event_get_target(e);
    screen->handleButtonPress(btn);
  }
}

void TeamsScreen::handleButtonPress(lv_obj_t* btn) {
  extern BleKeyboard bleKeyboard;
  
  if (!bleKeyboard.isConnected()) {
    Serial.println("[BLE Keyboard] Not connected - please pair device");
    return;
  }
  
  if (btn == btn_mute_) {
    // Microsoft Teams: Ctrl+Shift+M to toggle mute
    // Layout-agnostic fix: send both US 'M' (usage 0x10) and AZERTY 'M' (usage 0x33)
    // so Belgian/French AZERTY hosts receive the correct physical key.

    auto sendCtrlShift = [&](uint8_t usage) {
      KeyReport report = {};
      report.modifiers = 0x01 /*LCtrl*/ | 0x02 /*LShift*/;
      report.keys[0] = usage;
      bleKeyboard.sendReport(&report);
      delay(60);
      KeyReport release = {};
      bleKeyboard.sendReport(&release);
      delay(40);
    };

    Serial.println("[Teams] Mute button pressed - sending Ctrl+Shift+M (layout-agnostic)");
    sendCtrlShift(0x10); // US QWERTY 'M'
    sendCtrlShift(0x33); // BE/FR AZERTY 'M' (semicolon key)
    
  } else if (btn == btn_camera_) {
    // Microsoft Teams: Ctrl+Shift+O to toggle video
    // Use HID scan code directly (0x12 = O key position, layout-independent)
    Serial.println("[Teams] Camera button pressed - sending Ctrl+Shift+O (scan code)");
    bleKeyboard.press(KEY_LEFT_CTRL);
    bleKeyboard.press(KEY_LEFT_SHIFT);
    bleKeyboard.press(HID_KEY_O + 136);  // Add 136 to send as non-printing key
    delay(100);
    bleKeyboard.releaseAll();
    Serial.println("[BLE Keyboard] Sent Ctrl+Shift+O");
    
  } else if (btn == btn_end_call_) {
    // Microsoft Teams: Ctrl+Shift+H to hang up
    // Use HID scan code directly (0x0B = H key position, layout-independent)
    Serial.println("[Teams] End call button pressed - sending Ctrl+Shift+H (scan code)");
    bleKeyboard.press(KEY_LEFT_CTRL);
    bleKeyboard.press(KEY_LEFT_SHIFT);
    bleKeyboard.press(HID_KEY_H + 136);  // Add 136 to send as non-printing key
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

  } else if (btn == btn_raise_hand_) {
    // Microsoft Teams: Ctrl+Shift+K to raise/lower hand
    Serial.println("[Teams] Raise hand button pressed - sending Ctrl+Shift+K (scan code)");
    bleKeyboard.press(KEY_LEFT_CTRL);
    bleKeyboard.press(KEY_LEFT_SHIFT);
    bleKeyboard.press(HID_KEY_K + 136);  // K key position
    delay(100);
    bleKeyboard.releaseAll();
    Serial.println("[BLE Keyboard] Sent Ctrl+Shift+K");

  } else if (btn == btn_share_screen_) {
    // Microsoft Teams: Ctrl+Shift+E to share screen
    Serial.println("[Teams] Share screen button pressed - sending Ctrl+Shift+E (scan code)");
    bleKeyboard.press(KEY_LEFT_CTRL);
    bleKeyboard.press(KEY_LEFT_SHIFT);
    bleKeyboard.press(HID_KEY_E + 136);  // E key position
    delay(100);
    bleKeyboard.releaseAll();
    Serial.println("[BLE Keyboard] Sent Ctrl+Shift+E");
  }
}

void TeamsScreen::handle(const UiEvent &evt) {
  // Event handling will be implemented later
  (void)evt;
}
