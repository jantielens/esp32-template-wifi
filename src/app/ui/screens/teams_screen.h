#ifndef TEAMS_SCREEN_H
#define TEAMS_SCREEN_H

#include "../base_screen.h"

class TeamsScreen : public BaseScreen {
 public:
  lv_obj_t* root() override;
  void handle(const UiEvent &evt) override;

 private:
  void build();
  lv_obj_t* root_ = nullptr;
  
  // Button references (for future event handling)
  lv_obj_t* btn_mute_ = nullptr;
  lv_obj_t* btn_camera_ = nullptr;
  lv_obj_t* btn_end_call_ = nullptr;
  lv_obj_t* btn_vol_up_ = nullptr;
  lv_obj_t* btn_vol_down_ = nullptr;
};

#endif // TEAMS_SCREEN_H
