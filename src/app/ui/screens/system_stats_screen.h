#ifndef SYSTEM_STATS_SCREEN_H
#define SYSTEM_STATS_SCREEN_H

#include "../base_screen.h"

class SystemStatsScreen : public BaseScreen {
 public:
  lv_obj_t* root() override;
  void onEnter() override;
  void onExit() override;

 private:
  void build();
  void update_stats();
  static void timer_cb(lv_timer_t *timer);

  lv_obj_t* root_ = nullptr;

  // Value labels
  lv_obj_t* uptime_label_ = nullptr;
  lv_obj_t* reset_label_ = nullptr;
  lv_obj_t* cpu_usage_label_ = nullptr;
  lv_obj_t* temp_label_ = nullptr;
  lv_obj_t* heap_label_ = nullptr;
  lv_obj_t* heap_min_label_ = nullptr;
  lv_obj_t* heap_frag_label_ = nullptr;
  lv_obj_t* flash_label_ = nullptr;
  lv_obj_t* wifi_rssi_label_ = nullptr;
  lv_obj_t* wifi_ip_label_ = nullptr;

  lv_timer_t* timer_ = nullptr;
};

#endif // SYSTEM_STATS_SCREEN_H
