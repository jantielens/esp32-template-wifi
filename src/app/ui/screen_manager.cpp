#include "screen_manager.h"
#include "ui_events.h"
#include "screens/hello_screen.h"

static BaseScreen* get_screen(ScreenId id) {
  switch (id) {
    case ScreenId::Hello: {
      static HelloScreen screen;
      return &screen;
    }
    default:
      return nullptr;
  }
}

ScreenManager UI;

void ScreenManager::begin(ScreenId initial) {
  current_id_ = initial;
  current_ = get_screen(initial);
  if (!current_) return;
  lv_obj_t* root = current_->root();
  if (root) {
    lv_scr_load(root);
    current_->onEnter();
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
    lv_scr_load_anim(root, anim, time, delay, true /*auto_del_prev*/);
  }

  current_ = next;
  current_id_ = id;
  current_->onEnter();
}

void ScreenManager::loop() {
  if (!current_) return;
  UiEvent evt;
  while (ui_poll(&evt)) {
    current_->handle(evt);
  }
}
