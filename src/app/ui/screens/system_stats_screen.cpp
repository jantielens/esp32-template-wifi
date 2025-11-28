#include "system_stats_screen.h"

#include <lvgl.h>
#include <stdio.h>

#include "../../health_stats.h"

namespace {

// Helpers
static void format_uptime(uint32_t seconds, char* out, size_t len) {
  uint32_t days = seconds / 86400;
  uint32_t hours = (seconds % 86400) / 3600;
  uint32_t minutes = (seconds % 3600) / 60;
  uint32_t secs = seconds % 60;

  if (days > 0) {
    snprintf(out, len, "%lud %luh %lum", (unsigned long)days, (unsigned long)hours, (unsigned long)minutes);
  } else if (hours > 0) {
    snprintf(out, len, "%luh %lum %lus", (unsigned long)hours, (unsigned long)minutes, (unsigned long)secs);
  } else if (minutes > 0) {
    snprintf(out, len, "%lum %lus", (unsigned long)minutes, (unsigned long)secs);
  } else {
    snprintf(out, len, "%lus", (unsigned long)secs);
  }
}

static void format_bytes_kb(uint32_t bytes, char* out, size_t len) {
  if (bytes >= 1024) {
    float kb = bytes / 1024.0f;
    snprintf(out, len, "%.1f KB", kb);
  } else {
    snprintf(out, len, "%lu B", (unsigned long)bytes);
  }
}

static const char* signal_strength_desc(int rssi) {
  if (rssi >= -50) return "Excellent";
  if (rssi >= -60) return "Good";
  if (rssi >= -70) return "Fair";
  if (rssi >= -80) return "Weak";
  return "Very Weak";
}

static lv_obj_t* create_row_split(lv_obj_t* parent, const char* label_text, lv_obj_t** value_out) {
  lv_obj_t* row = lv_obj_create(parent);
  lv_obj_remove_style_all(row);
  lv_obj_set_width(row, lv_pct(100));
  lv_obj_set_height(row, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_gap(row, 8, 0);
  lv_obj_set_style_pad_all(row, 0, 0);

  // Label (right-aligned)
  lv_obj_t* label = lv_label_create(row);
  lv_label_set_text(label, label_text);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_flex_grow(label, 1);

  // Value (left-aligned, bigger font)
  lv_obj_t* value = lv_label_create(row);
  lv_label_set_text(value, "--");
  lv_obj_set_style_text_color(value, lv_color_white(), 0);
  lv_obj_set_style_text_font(value, &lv_font_montserrat_22, 0);
  lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_flex_grow(value, 1);

  if (value_out) *value_out = value;
  return row;
}

}  // namespace

lv_obj_t* SystemStatsScreen::root() {
  if (!root_) build();
  return root_;
}

void SystemStatsScreen::build() {
  root_ = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(root_, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(root_, 0, 0);
  lv_obj_set_style_border_width(root_, 0, 0);

  // Column sized for a 360px round display (~90% width), packed from top
  lv_obj_t* col = lv_obj_create(root_);
  lv_obj_remove_style_all(col);
  lv_obj_set_size(col, lv_pct(92), lv_pct(100));
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_gap(col, 4, 0);
  lv_obj_set_style_pad_top(col, 20, 0);
  lv_obj_set_style_pad_bottom(col, 20, 0);
  lv_obj_align(col, LV_ALIGN_TOP_MID, 0, 0);

  // Rows (split layout: label right-aligned, value left-aligned)
  create_row_split(col, "Reset", &reset_label_);
  create_row_split(col, "CPU", &cpu_usage_label_);
  create_row_split(col, "Temp", &temp_label_);
  create_row_split(col, "Heap", &heap_label_);
  create_row_split(col, "Heap min", &heap_min_label_);
  create_row_split(col, "Heap frag", &heap_frag_label_);
  create_row_split(col, "Flash", &flash_label_);
  create_row_split(col, "WiFi", &wifi_rssi_label_);
  create_row_split(col, "IP", &wifi_ip_label_);

  // Optional uptime row at top for quick glance
  lv_obj_t* uptime_row = create_row_split(col, "Uptime", &uptime_label_);
  lv_obj_move_to_index(uptime_row, 0); // Place uptime as the first row
}

void SystemStatsScreen::onEnter() {
  // Immediate update and start 2-second refresh
  update_stats();
  if (!timer_) {
    timer_ = lv_timer_create(timer_cb, 2000, this);
  }
}

void SystemStatsScreen::onExit() {
  if (timer_) {
    lv_timer_del(timer_);
    timer_ = nullptr;
  }
}

void SystemStatsScreen::timer_cb(lv_timer_t *timer) {
  if (!timer) return;
  auto* self = static_cast<SystemStatsScreen*>(timer->user_data);
  if (self) self->update_stats();
}

void SystemStatsScreen::update_stats() {
  HealthStats stats;
  if (!collect_health_stats(stats)) return;

  char buf[64];

  if (uptime_label_) {
    format_uptime(stats.uptime_seconds, buf, sizeof(buf));
    lv_label_set_text(uptime_label_, buf);
  }

  if (reset_label_) {
    lv_label_set_text(reset_label_, stats.reset_reason.c_str());
  }

  if (cpu_usage_label_) {
    snprintf(buf, sizeof(buf), "%d%% @ %luMHz", stats.cpu_usage, (unsigned long)stats.cpu_freq);
    lv_label_set_text(cpu_usage_label_, buf);
  }

  if (temp_label_) {
    if (stats.temperature_valid) {
      snprintf(buf, sizeof(buf), "%d°C", stats.temperature_c);
    } else {
      snprintf(buf, sizeof(buf), "N/A");
    }
    lv_label_set_text(temp_label_, buf);
  }

  if (heap_label_) {
    format_bytes_kb(stats.heap_free, buf, sizeof(buf));
    lv_label_set_text(heap_label_, buf);
  }

  if (heap_min_label_) {
    format_bytes_kb(stats.heap_min, buf, sizeof(buf));
    lv_label_set_text(heap_min_label_, buf);
  }

  if (heap_frag_label_) {
    snprintf(buf, sizeof(buf), "%d%%", stats.heap_fragmentation);
    lv_label_set_text(heap_frag_label_, buf);
  }

  if (flash_label_) {
    unsigned long used_kb = stats.flash_used / 1024;
    unsigned long total_kb = stats.flash_total / 1024;
    snprintf(buf, sizeof(buf), "%lu / %lu KB", used_kb, total_kb);
    lv_label_set_text(flash_label_, buf);
  }

  if (wifi_rssi_label_) {
    if (stats.wifi_connected) {
      snprintf(buf, sizeof(buf), "%d dBm (%s)", stats.wifi_rssi, signal_strength_desc(stats.wifi_rssi));
    } else {
      snprintf(buf, sizeof(buf), "Not connected");
    }
    lv_label_set_text(wifi_rssi_label_, buf);
  }

  if (wifi_ip_label_) {
    if (stats.wifi_connected && stats.ip_address.length() > 0) {
      lv_label_set_text(wifi_ip_label_, stats.ip_address.c_str());
    } else {
      lv_label_set_text(wifi_ip_label_, "N/A");
    }
  }
}
