#pragma once
// Minimal log_manager shim for host-native tests.
// Silences all logging macros without requiring Arduino headers.
#include <cstdio>
#define LOGI(tag, fmt, ...) do { (void)(tag); } while(0)
#define LOGD(tag, fmt, ...) do { (void)(tag); } while(0)
#define LOGW(tag, fmt, ...) do { (void)(tag); } while(0)
#define LOGE(tag, fmt, ...) do { (void)(tag); } while(0)
