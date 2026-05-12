#pragma once
// Minimal board_config shim for host-native tests.
// Only defines what the tested translation units require.
#define HAS_MQTT false
#define HAS_BLE  false
#define HAS_DISPLAY false
#define HAS_TOUCH false
#define HEALTH_HISTORY_ENABLED 0
