#ifndef UI_EVENTS_H
#define UI_EVENTS_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <stdint.h>

// Keep this lean; expand cautiously to avoid flash bloat.
enum class UiEventType : uint8_t {
  DemoCaption = 0,
  BootStatus,
  // Future: Mqtt, Timer, Wifi, Custom
};

struct UiEvent {
  UiEventType type;
  char msg[64]; // generic message payload (e.g., caption text)
};

// Initialize the UI event queue. Call once in setup.
bool ui_events_init(size_t capacity = 8);

// Publish an event from any task/ISR context (non-blocking). Returns false if queue is full/uninitialized.
bool ui_publish(const UiEvent &evt);

// Poll one event (non-blocking). Returns true if an event was dequeued.
bool ui_poll(UiEvent *out_evt);

#endif // UI_EVENTS_H
