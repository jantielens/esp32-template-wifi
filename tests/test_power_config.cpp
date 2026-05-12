// Unit tests for power_config.cpp parse functions.
// Compiled and run natively on the host (no Arduino SDK required).

#include <cassert>
#include <cstring>
#include <cstdio>

// Inject test stubs before including the source under test
#include "board_config.h"
#include "log_manager.h"

// power_config.h includes board_config.h and config_manager.h (for DeviceConfig)
// config_manager.h includes <Arduino.h> — resolved by tests/Arduino.h
#include "../src/app/power_config.h"
#include "../src/app/power_config.cpp"

static DeviceConfig make_config(const char *power_mode, const char *transport, const char *scope) {
    DeviceConfig c = {};
    if (power_mode)  strncpy(c.power_mode, power_mode, sizeof(c.power_mode) - 1);
    if (transport)   strncpy(c.publish_transport, transport, sizeof(c.publish_transport) - 1);
    if (scope)       strncpy(c.mqtt_publish_scope, scope, sizeof(c.mqtt_publish_scope) - 1);
    return c;
}

static int passed = 0;
static int failed = 0;

#define CHECK_EQ(a, b, msg) do { \
    if ((a) == (b)) { passed++; } \
    else { failed++; printf("FAIL: %s\n", msg); } \
} while(0)

static void test_power_mode_parse() {
    DeviceConfig c;

    c = make_config("always_on", nullptr, nullptr);
    CHECK_EQ(power_config_parse_power_mode(&c), PowerMode::AlwaysOn,    "always_on -> AlwaysOn");

    c = make_config("ALWAYS_ON", nullptr, nullptr);
    CHECK_EQ(power_config_parse_power_mode(&c), PowerMode::AlwaysOn,    "ALWAYS_ON case-insensitive");

    c = make_config("duty_cycle", nullptr, nullptr);
    CHECK_EQ(power_config_parse_power_mode(&c), PowerMode::DutyCycle,   "duty_cycle -> DutyCycle");

    c = make_config("config", nullptr, nullptr);
    CHECK_EQ(power_config_parse_power_mode(&c), PowerMode::Config,      "config -> Config");

    c = make_config("ap", nullptr, nullptr);
    CHECK_EQ(power_config_parse_power_mode(&c), PowerMode::Ap,          "ap -> Ap");

    c = make_config("unknown_mode", nullptr, nullptr);
    CHECK_EQ(power_config_parse_power_mode(&c), PowerMode::AlwaysOn,    "unknown -> default AlwaysOn");

    c = make_config("", nullptr, nullptr);
    CHECK_EQ(power_config_parse_power_mode(&c), PowerMode::AlwaysOn,    "empty -> default AlwaysOn");

    CHECK_EQ(power_config_parse_power_mode(nullptr), PowerMode::AlwaysOn, "null -> default AlwaysOn");
}

static void test_transport_parse() {
    DeviceConfig c;

    c = make_config(nullptr, "ble", nullptr);
    CHECK_EQ(power_config_parse_publish_transport(&c), PublishTransport::Ble, "ble -> Ble");

    c = make_config(nullptr, "BLE", nullptr);
    CHECK_EQ(power_config_parse_publish_transport(&c), PublishTransport::Ble, "BLE case-insensitive");

    c = make_config(nullptr, "mqtt", nullptr);
    CHECK_EQ(power_config_parse_publish_transport(&c), PublishTransport::Mqtt, "mqtt -> Mqtt");

    c = make_config(nullptr, "ble_mqtt", nullptr);
    CHECK_EQ(power_config_parse_publish_transport(&c), PublishTransport::BleMqtt, "ble_mqtt -> BleMqtt");

    c = make_config(nullptr, "mqtt_ble", nullptr);
    CHECK_EQ(power_config_parse_publish_transport(&c), PublishTransport::BleMqtt, "mqtt_ble -> BleMqtt");

    c = make_config(nullptr, "unknown", nullptr);
    CHECK_EQ(power_config_parse_publish_transport(&c), PublishTransport::Ble, "unknown -> default Ble");

    c = make_config(nullptr, "", nullptr);
    CHECK_EQ(power_config_parse_publish_transport(&c), PublishTransport::Ble, "empty -> default Ble");

    CHECK_EQ(power_config_parse_publish_transport(nullptr), PublishTransport::Ble, "null -> default Ble");
}

static void test_mqtt_scope_parse() {
    DeviceConfig c;

    c = make_config(nullptr, nullptr, "sensors_only");
    CHECK_EQ(power_config_parse_mqtt_publish_scope(&c), MqttPublishScope::SensorsOnly, "sensors_only");

    c = make_config(nullptr, nullptr, "SENSORS_ONLY");
    CHECK_EQ(power_config_parse_mqtt_publish_scope(&c), MqttPublishScope::SensorsOnly, "SENSORS_ONLY case-insensitive");

    c = make_config(nullptr, nullptr, "diagnostics_only");
    CHECK_EQ(power_config_parse_mqtt_publish_scope(&c), MqttPublishScope::DiagnosticsOnly, "diagnostics_only");

    c = make_config(nullptr, nullptr, "all");
    CHECK_EQ(power_config_parse_mqtt_publish_scope(&c), MqttPublishScope::All, "all");

    c = make_config(nullptr, nullptr, "unknown");
    CHECK_EQ(power_config_parse_mqtt_publish_scope(&c), MqttPublishScope::SensorsOnly, "unknown -> default SensorsOnly");

    CHECK_EQ(power_config_parse_mqtt_publish_scope(nullptr), MqttPublishScope::SensorsOnly, "null -> default SensorsOnly");
}

static void test_transport_helpers() {
    CHECK_EQ(power_config_transport_includes_ble(PublishTransport::Ble), true, "Ble includes ble");
    CHECK_EQ(power_config_transport_includes_ble(PublishTransport::BleMqtt), true, "BleMqtt includes ble");
    CHECK_EQ(power_config_transport_includes_ble(PublishTransport::Mqtt), false, "Mqtt does not include ble");

    CHECK_EQ(power_config_transport_includes_mqtt(PublishTransport::Mqtt), true, "Mqtt includes mqtt");
    CHECK_EQ(power_config_transport_includes_mqtt(PublishTransport::BleMqtt), true, "BleMqtt includes mqtt");
    CHECK_EQ(power_config_transport_includes_mqtt(PublishTransport::Ble), false, "Ble does not include mqtt");
}

int main() {
    printf("=== power_config tests ===\n");
    test_power_mode_parse();
    test_transport_parse();
    test_mqtt_scope_parse();
    test_transport_helpers();
    printf("Results: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
