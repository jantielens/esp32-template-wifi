#include "mqtt_wake.h"

#if HAS_MQTT && HAS_DISPLAY

#include "screen_saver_manager.h"
#include "log_manager.h"

#include <string.h>
#include <strings.h>

static bool isWakePayload(const uint8_t *payload, unsigned int len) {
		char buf[8];
		unsigned int copy = len < sizeof(buf) - 1 ? len : sizeof(buf) - 1;
		memcpy(buf, payload, copy);
		buf[copy] = '\0';
		return strcasecmp(buf, "ON") == 0
		       || strcmp(buf, "1") == 0
		       || strcasecmp(buf, "true") == 0;
}

static void onWakeMessage(const char *topic, const uint8_t *payload, unsigned int len) {
		(void)topic;
		if (isWakePayload(payload, len)) {
				LOGI("MqttWake", "Wake triggered by MQTT");
				screen_saver_manager_notify_activity(true);
		}
}

void mqtt_wake_begin(MqttManager *mqtt, const char *topic) {
		if (!mqtt || !topic || strlen(topic) == 0) return;
		if (mqtt->addSubscription(topic, onWakeMessage)) {
				LOGI("MqttWake", "Subscribed to wake topic: %s", topic);
		}
}

#endif // HAS_MQTT && HAS_DISPLAY
