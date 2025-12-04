# UI Screens Architecture (LVGL)

**Goal:** Simple, DRY multi-screen pattern with safe updates from callbacks (MQTT, timers, WiFi events). Screens are modular, navigable, and event-driven via a small FreeRTOS queue.

---

## 🧱 Anatomy

| Component | Location | Responsibility |
|-----------|----------|----------------|
| `UiEvent` | `src/app/ui/ui_events.{h,cpp}` | Small FreeRTOS queue; `ui_publish` from any task, `ui_poll` on LVGL task |
| `BaseScreen` | `src/app/ui/base_screen.h` | Interface: `root()`, `onEnter()`, `onExit()`, `handle(const UiEvent&)` |
| `ScreenManager` (`UI`) | `src/app/ui/screen_manager.{h,cpp}` | Keeps current screen, navigation, dispatches events |
| Screens | `src/app/ui/screens/*.cpp` | Implement `BaseScreen`; build LVGL UI and handle events |
| Wiring | `src/app/app.ino` | Initializes `ui_events`, `UI.begin(...)`, calls `UI.loop()`; includes UI `.cpp` files for Arduino CLI |

> Arduino CLI quirk: We `#include` UI `.cpp` files in `app.ino` to ensure they are compiled, similar to `display_driver.cpp`.

---

## 🔄 Lifecycle

1. **Setup** (`app.ino`)
   ```cpp
  ui_events_init();
  UI.begin(ScreenId::Splash); // Splash → SystemStats when WiFi ready (non-AP)
   ```
2. **Loop** (`app.ino`)
   ```cpp
   board_display_loop(); // runs lv_timer_handler()
   UI.loop();            // drains UiEvent queue, calls current->handle()
  screensaver_update(); // 120s idle -> backlight off; touch to wake
   ```
3. **Navigate**
   ```cpp
  UI.navigate(ScreenId::SystemStats, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0);
   ```

> Default flow: `Splash` → `SystemStats` after minimum dwell. `Hello` remains available for demos/tests but is no longer the post-splash default.

## 📴 Screensaver

- **Timeout:** 120 s of LVGL inactivity (`lv_disp_get_inactive_time`).
- **Action:** Turns off display backlight (JC3636W518 board).
- **Wake:** First touch wakes the screen (gesture is swallowed); subsequent touches behave normally.
- **Boards:** No-op on boards without a display.

---

## 📬 Event Flow (Thread-Safe)

**Publish from any task/ISR-safe context** (non-blocking):
```cpp
UiEvent evt{};
evt.type = UiEventType::DemoCaption;
strlcpy(evt.msg, "hello", sizeof(evt.msg));
ui_publish(evt);
```

**Handle on LVGL task (inside `UI.loop()`)**:
```cpp
void HelloScreen::handle(const UiEvent &evt) {
  if (evt.type == UiEventType::DemoCaption && btn_label_) {
    lv_label_set_text(btn_label_, evt.msg);
  }
}
```

> Only the LVGL task touches LVGL objects. Everything else funnels through `ui_publish` → `UI.loop()`.

---

## 🎯 Swipe-Resistant Buttons

Buttons automatically filter out swipe gestures to prevent accidental activation during navigation.

### Quick Pattern (2 steps)

**1. Use `addButtonEventCallbacks()` when creating buttons:**
```cpp
void MyScreen::build() {
  lv_obj_t* my_button = lv_btn_create(root_);
  // ... configure button appearance ...
  
  // This adds all needed events (PRESSED, PRESSING, RELEASED, CLICKED)
  addButtonEventCallbacks(my_button, button_event_cb, this);
}
```

**2. Call `processTouchEvent()` in your callback:**
```cpp
static void button_event_cb(lv_event_t* e) {
  MyScreen* screen = static_cast<MyScreen*>(lv_event_get_user_data(e));
  
  // Returns true only if touch moved < 30 pixels (valid tap)
  if (processTouchEvent(e)) {
    lv_obj_t* btn = lv_event_get_target(e);
    screen->handleButtonPress(btn);
  }
}
```

### How It Works

```
User touches button → PRESSED event
  ↓ tracks start position
User drags finger → PRESSING events (continuous)
  ↓ updates current position
  ↓ if distance ≥ 30px → marks as swipe
User lifts finger → RELEASED event
  ↓ final distance check
  ↓ CLICKED event fires (LVGL default)
  ↓ processTouchEvent() returns false if was_swipe
  ↓ button action ignored ✓
```

**Touch tracker managed globally by `BaseScreen`** - no need to declare or initialize in child screens.

**Configuration** (`src/app/lv_conf.h` and `src/app/ui/base_screen.h`):
- Gesture detection: 80 pixels minimum, velocity 5 (for screen swipe navigation)
- Button threshold: 30 pixels (for distinguishing taps from swipes)

**Screens using this pattern:**
- `TeamsScreen` - all 7 control buttons protected
- `HelloScreen` - demo button protected

---

## 🧩 Adding a Screen

1. **Enum** – add ID to `ScreenId` (`screen_manager.h`).
2. **Class** – create `screens/<name>_screen.{h,cpp}` inheriting `BaseScreen`.
3. **Map** – register in `get_screen(...)` (`screen_manager.cpp`).
4. **Include** – add `#include "ui/screens/<name>_screen.cpp"` to `app.ino`.

> **Note**: `base_screen.cpp` is also included in `app.ino` to ensure the global `TouchTracker` is properly linked.

**Screen skeleton:**
```cpp
// screens/config_screen.h
class ConfigScreen : public BaseScreen {
 public:
  lv_obj_t* root() override;
  void onEnter() override;
  void onExit() override;
  void handle(const UiEvent &evt) override;
 private:
  void build();
  static void button_event_cb(lv_event_t* e);  // If screen has buttons
  void handleButtonPress(lv_obj_t* btn);       // If screen has buttons
  
  lv_obj_t* root_ = nullptr;
  lv_timer_t* timer_ = nullptr;
};
```
```cpp
// screens/config_screen.cpp
lv_obj_t* ConfigScreen::root() { if (!root_) build(); return root_; }

void ConfigScreen::build() {
  root_ = lv_obj_create(nullptr);
  
  // For screens with buttons - use swipe-resistant pattern:
  lv_obj_t* btn = lv_btn_create(root_);
  lv_obj_t* label = lv_label_create(btn);
  lv_label_set_text(label, "Click Me");
  addButtonEventCallbacks(btn, button_event_cb, this);  // Protected from swipes
}

void ConfigScreen::button_event_cb(lv_event_t* e) {
  ConfigScreen* screen = static_cast<ConfigScreen*>(lv_event_get_user_data(e));
  if (processTouchEvent(e)) {  // Only true for taps, not swipes
    lv_obj_t* btn = lv_event_get_target(e);
    screen->handleButtonPress(btn);
  }
}

void ConfigScreen::handleButtonPress(lv_obj_t* btn) {
  // Button action logic here
}

void ConfigScreen::onEnter() {
  // example: periodic refresh
  timer_ = lv_timer_create([](lv_timer_t *t){ auto self = (ConfigScreen*)t->user_data; /* refresh */; }, 5000, this);
}

void ConfigScreen::onExit() {
  if (timer_) { lv_timer_del(timer_); timer_ = nullptr; }
}

void ConfigScreen::handle(const UiEvent &evt) {
  // react to events
}
```

---

## 🎛 Navigation & Animations

Use LVGL screen load animations:
```cpp
UI.navigate(ScreenId::Health, LV_SCR_LOAD_ANIM_FADE_ON, 250, 0);
```

> Future: add a back-stack if needed (vector of `ScreenId`), and a consistent nav bar widget.

---

## ⏱ Timers & Updates

- **Per-screen timers**: create in `onEnter()`, delete in `onExit()`.
- **MQTT/WiFi callbacks**: copy minimal payload into `UiEvent.msg` (64 bytes) and publish.
- **Health loop**: publish `UiEventType::Health` every 5–10s; let `HealthScreen` render.

Extend `UiEventType` cautiously to keep payload small.

---

## ⚠️ Memory & Partitions

Current `jc3636w518` build sits at ~99% of the 1.25 MB sketch partition. For more UI:
- Use `BOARD_BUILD_PROPS_jc3636w518_opi16m` (16 MB flash + PSRAM profile), or
- Adjust partition scheme to increase APP partition size.

---

## ✅ Checklist (when adding screens)

- [ ] `ScreenId` updated
- [ ] Screen class created & includes `root()`, `handle()`, `onEnter()/onExit()` as needed
- [ ] Registered in `get_screen()`
- [ ] `.cpp` force-included in `app.ino`
- [ ] Event types minimal; no heap allocations in `UiEvent`
- [ ] LVGL touches only on LVGL task
- [ ] Timers cleaned up in `onExit()`
- [ ] Build firmware (`./build.sh`) and verify

---

## 📚 References
- LVGL docs: https://docs.lvgl.io
- FreeRTOS queues: https://www.freertos.org/a00116.html
