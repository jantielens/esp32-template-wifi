#ifndef HELLO_SCREEN_H
#define HELLO_SCREEN_H

#include "../base_screen.h"

class HelloScreen : public BaseScreen {
 public:
  lv_obj_t* root() override;
  void handle(const UiEvent &evt) override;

 private:
  void build();
  static void hello_btn_event_cb(lv_event_t *e);
  
  lv_obj_t* root_ = nullptr;
  lv_obj_t* btn_label_ = nullptr;
};

#endif // HELLO_SCREEN_H
