# Display Architecture

This document describes the display subsystem architecture, design patterns, and extension points for adding new displays and screens.

## Table of Contents

- [Overview](#overview)
- [Architecture Layers](#architecture-layers)
- [Display Driver HAL](#display-driver-hal)
- [Screen Management](#screen-management)
- [Rendering System](#rendering-system)
- [Adding New Display Drivers](#adding-new-display-drivers)
- [Adding New Screens](#adding-new-screens)
- [Multi-Board Display Support](#multi-board-display-support)
- [Performance Considerations](#performance-considerations)

## Overview

The display subsystem is built on three main pillars:

1. **Hardware Abstraction Layer (HAL)** - Isolates display library specifics
2. **Screen Pattern** - Base class for creating reusable UI screens
3. **DisplayManager** - Centralized management of hardware, LVGL, and screen lifecycle

**Key Technologies:**
- **LVGL 8.4** - Embedded graphics library
- **TFT_eSPI** - Default display driver (supports ILI9341, ST7789, ST7735, etc.)
- **FreeRTOS** - Task-based continuous rendering

## Architecture Layers

```
┌─────────────────────────────────────────────────────┐
│  Application Code (app.ino)                        │
│  - Minimal display interaction                      │
│  - display_manager_init()                          │
│  - display_manager_show_info()                     │
└─────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────┐
│  DisplayManager                                     │
│  - Owns hardware, LVGL, screens, rendering task    │
│  - Navigation API (showSplash, showInfo, showTest) │
│  - Thread-safe via mutex                           │
└─────────────────────────────────────────────────────┘
                        ↓
        ┌───────────────┴───────────────┐
        ↓                               ↓
┌──────────────────┐          ┌──────────────────┐
│  Screen Pattern  │          │ DisplayDriver    │
│  (Base Class)    │          │  HAL Interface   │
│                  │          │                  │
│  - SplashScreen  │          │ - TFT_eSPI_Driver│
│  - InfoScreen    │          │ - LovyanGFX (fut)│
│  - TestScreen    │          │ - Custom drivers │
└──────────────────┘          └──────────────────┘
        ↓                               ↓
┌─────────────────────────────────────────────────────┐
│  LVGL 8.4                                           │
│  - Widget rendering                                 │
│  - Themes, fonts, animations                       │
└─────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────┐
│  Hardware (SPI, Display Panel)                      │
└─────────────────────────────────────────────────────┘
```

## Display Driver HAL

### Purpose

The DisplayDriver interface decouples LVGL from specific display libraries, allowing support for TFT_eSPI, LovyanGFX, or custom drivers without changing DisplayManager code.

### Interface Definition

```cpp
// src/app/display_driver.h
class DisplayDriver {
public:
    virtual ~DisplayDriver() = default;
    
    // Hardware initialization
    virtual void init() = 0;
    
    // Display configuration
    virtual void setRotation(uint8_t rotation) = 0;
    virtual void setBacklight(bool on) = 0;
    virtual void applyDisplayFixes() = 0;
    
    // LVGL flush interface (hot path - called frequently)
    virtual void startWrite() = 0;
    virtual void endWrite() = 0;
    virtual void setAddrWindow(int16_t x, int16_t y, uint16_t w, uint16_t h) = 0;
    virtual void pushColors(uint16_t* data, uint32_t len, bool swap_bytes = true) = 0;
};
```

### Performance Impact

**Measured overhead:** +640 bytes flash (0.046%), negligible runtime cost

The hot path (`pushColors`) is called 24 times per full screen refresh:
- Virtual call overhead: ~0.01 µs
- SPI transfer time: 640-1280 µs (at 40-80 MHz)
- **Total overhead: 0.01%** (completely negligible)

### Selecting a Driver

In `board_config.h` or `board_overrides.h`:

```cpp
// Available drivers
#define DISPLAY_DRIVER_TFT_ESPI 1
#define DISPLAY_DRIVER_LOVYANGFX 2

// Select driver (defaults to TFT_eSPI)
#define DISPLAY_DRIVER DISPLAY_DRIVER_TFT_ESPI
```

## Screen Management

### Screen Base Class

All screens inherit from the `Screen` interface:

```cpp
// src/app/screens/screen.h
class Screen {
public:
    virtual void create() = 0;   // Create LVGL objects
    virtual void destroy() = 0;  // Free LVGL objects
    virtual void show() = 0;     // Load screen (lv_scr_load)
    virtual void hide() = 0;     // Hide screen
    virtual void update() = 0;   // Update dynamic content (called every 5ms)
    virtual ~Screen() = default;
};
```

### Lifecycle

1. **Create** - Called once during `DisplayManager::init()`
   - Allocate LVGL objects
   - Set initial content
   - Position widgets
   
2. **Show** - Called when navigating to screen
   - `lv_scr_load(screen)` to make visible
   
3. **Update** - Called continuously by rendering task (every 5ms)
   - Refresh dynamic data (uptime, WiFi status, etc.)
   - Only update if screen is active
   
4. **Hide** - Called when navigating away
   - LVGL handles screen unloading automatically
   
5. **Destroy** - Called in DisplayManager destructor
   - Free all LVGL objects

### Included Screens

**SplashScreen** (`splash_screen.h/cpp`)
- Boot screen with animated spinner
- Status text updates during initialization
- Optimized layout for 240x240 round displays

**InfoScreen** (`info_screen.h/cpp`)
- Device information and real-time stats
- Round display compatible (all text centered)
- Shows: device name, uptime, memory, WiFi, IP, version, chip info
- Device name as hero element with separator lines

**TestScreen** (`test_screen.h/cpp`)
- Display calibration and color testing
- RGB and CMY color bars
- Centered grayscale gradient (black to white)
- Resolution info display

## Rendering System

### FreeRTOS Task-Based Architecture

DisplayManager creates a dedicated task for LVGL rendering:

```cpp
void DisplayManager::lvglTask(void* pvParameter) {
    while (true) {
        mgr->lock();                        // Acquire mutex
        lv_timer_handler();                 // LVGL rendering
        if (mgr->currentScreen) {
            mgr->currentScreen->update();   // Screen data refresh
        }
        mgr->unlock();                      // Release mutex
        
        vTaskDelay(pdMS_TO_TICKS(5));      // 5ms interval
    }
}
```

**Benefits:**
- Continuous LVGL updates (animations, timers work automatically)
- No manual `update()` calls needed in `loop()`
- Works on both single-core and dual-core ESP32
- Thread-safe via mutex protection

**Core Assignment:**
- **Dual-core:** Task pinned to Core 0, Arduino `loop()` on Core 1
- **Single-core:** Task time-sliced with Arduino `loop()` on Core 0

### Thread Safety

All display operations from outside the rendering task must be protected:

```cpp
displayManager->lock();
// LVGL operations here
displayManager->unlock();
```

DisplayManager API methods handle locking automatically (`showSplash()`, `showInfo()`, etc.).

## Adding New Display Drivers

### Step 1: Create Driver Class

Create `src/app/drivers/my_driver.h` and `.cpp`:

```cpp
#include "../display_driver.h"
#include <MyDisplayLib.h>

class MyDisplayDriver : public DisplayDriver {
private:
    MyDisplayLib display;
    
public:
    void init() override {
        display.begin();
    }
    
    void setRotation(uint8_t rotation) override {
        display.setRotation(rotation);
    }
    
    void setBacklight(bool on) override {
        // Control backlight
    }
    
    void applyDisplayFixes() override {
        // Board-specific fixes (inversion, gamma, etc.)
    }
    
    void startWrite() override { display.startWrite(); }
    void endWrite() override { display.endWrite(); }
    void setAddrWindow(int16_t x, int16_t y, uint16_t w, uint16_t h) override {
        display.setWindow(x, y, w, h);
    }
    void pushColors(uint16_t* data, uint32_t len, bool swap_bytes) override {
        display.writePixels(data, len);
    }
};
```

### Step 2: Register Driver

In `board_config.h`:

```cpp
#define DISPLAY_DRIVER_MY_DRIVER 3

#ifndef DISPLAY_DRIVER
#define DISPLAY_DRIVER DISPLAY_DRIVER_MY_DRIVER
#endif
```

### Step 3: Integrate in DisplayManager

In `display_manager.cpp`:

```cpp
#if DISPLAY_DRIVER == DISPLAY_DRIVER_MY_DRIVER
#include "drivers/my_driver.h"
#endif

// In constructor:
#if DISPLAY_DRIVER == DISPLAY_DRIVER_MY_DRIVER
driver = new MyDisplayDriver();
#endif
```

### Step 4: Compile Driver

Add to `src/app/screens.cpp` (conditional compilation ensures only selected driver is compiled):

```cpp
// Include display driver implementations (conditional based on selection)
#if DISPLAY_DRIVER == DISPLAY_DRIVER_TFT_ESPI
#include "drivers/tft_espi_driver.cpp"
#elif DISPLAY_DRIVER == DISPLAY_DRIVER_MY_DRIVER
#include "drivers/my_driver.cpp"
#endif
```

## Adding New Screens

### Step 1: Create Screen Files

Create `src/app/screens/my_screen.h`:

```cpp
#ifndef MY_SCREEN_H
#define MY_SCREEN_H

#include "screen.h"
#include <lvgl.h>

class MyScreen : public Screen {
private:
    lv_obj_t* screen;
    lv_obj_t* label;
    
public:
    MyScreen();
    ~MyScreen();
    
    void create() override;
    void destroy() override;
    void show() override;
    void hide() override;
    void update() override;
};

#endif
```

Create `src/app/screens/my_screen.cpp`:

```cpp
#include "my_screen.h"
#include "../board_config.h"

MyScreen::MyScreen() : screen(nullptr), label(nullptr) {}

MyScreen::~MyScreen() {
    destroy();
}

void MyScreen::create() {
    if (screen) return;
    
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    
    label = lv_label_create(screen);
    lv_label_set_text(label, "My Screen");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

void MyScreen::destroy() {
    if (screen) {
        lv_obj_del(screen);
        screen = nullptr;
        label = nullptr;
    }
}

void MyScreen::show() {
    if (screen) {
        lv_scr_load(screen);
    }
}

void MyScreen::hide() {
    // LVGL handles screen switching
}

void MyScreen::update() {
    // Update dynamic content here
}
```

### Step 2: Add to DisplayManager

In `display_manager.h`:

```cpp
#include "screens/my_screen.h"

class DisplayManager {
private:
    MyScreen myScreen;
    
public:
    void showMyScreen();
};
```

In `display_manager.cpp`:

```cpp
void DisplayManager::init() {
    myScreen.create();
    // ...
}

void DisplayManager::showMyScreen() {
    lock();
    if (currentScreen) {
        currentScreen->hide();
    }
    currentScreen = &myScreen;
    currentScreen->show();
    unlock();
}
```

### Step 3: Compile Screen

Add to `src/app/screens.cpp`:

```cpp
#include "screens/my_screen.cpp"
```

## Multi-Board Display Support

### Board-Specific Configuration

Each board can define display settings in `src/boards/[board-name]/board_overrides.h`:

```cpp
// Enable display
#define HAS_DISPLAY 1

// TFT_eSPI driver selection
#define ILI9341_DRIVER  // or ST7789_DRIVER, ST7735_DRIVER, etc.

// Display dimensions
#define TFT_WIDTH  320
#define TFT_HEIGHT 240
#define DISPLAY_WIDTH  320
#define DISPLAY_HEIGHT 240
#define DISPLAY_ROTATION 1

// Pin configuration
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  4
#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH

// Display-specific fixes
#define DISPLAY_INVERSION_ON      // or DISPLAY_INVERSION_OFF
#define DISPLAY_NEEDS_GAMMA_FIX   // Apply gamma correction

// LVGL buffer size (board-specific optimization)
#undef LVGL_BUFFER_SIZE
#define LVGL_BUFFER_SIZE (DISPLAY_WIDTH * 10)
```

### Round Display Support

All included screens are designed for **240x240 minimum round displays**:

- All text uses `LV_ALIGN_CENTER` for horizontal/vertical centering
- Widgets positioned with Y offsets from center
- Important content kept within ±90px of center
- Full-width elements (gradients, bars) work on rectangular displays too

**Layout Guidelines:**
- Use centered alignment: `lv_obj_align(obj, LV_ALIGN_CENTER, 0, y_offset)`
- Keep critical content within ±90px from center (Y=0)
- Test on both 240x240 round and 320x240 rectangular displays

### Build System Integration

The build system automatically detects board-specific display configurations:

```bash
./build.sh cyd2usb-v2  # Builds with CYD display config
./build.sh esp32       # Builds without display (HAS_DISPLAY=false)
```

Each board compiles with its own display driver and settings.

## Performance Considerations

### Memory Usage

- **DisplayDriver HAL:** +64 bytes RAM (vtable), +640 bytes flash
- **LVGL buffers:** `DISPLAY_WIDTH * 10 * 2` bytes (e.g., 6.4 KB for 320x240)
- **Per screen:** ~200-500 bytes (depends on widget count)
- **Total:** ~50-60 KB for display subsystem

### Rendering Performance

**Typical metrics (320x240 @ 40 MHz SPI):**
- Full screen refresh: ~30-40ms (24 buffer flushes)
- LVGL task overhead: ~1-2% CPU (5ms interval)
- Virtual call overhead: <0.01% (negligible)

**Optimization tips:**
- Use larger LVGL buffers if RAM allows (reduces flushes)
- Increase SPI speed to 80 MHz if display supports it
- Minimize widget updates in `update()` method
- Use LVGL's dirty rectangle optimization (automatic)

### LVGL Configuration

Key settings in `src/app/lv_conf.h`:

```cpp
#define LV_COLOR_DEPTH 16              // RGB565
#define LV_MEM_SIZE (48 * 1024U)       // 48 KB LVGL heap
#define LV_FONT_MONTSERRAT_14 1        // Enable fonts
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_THEME_DEFAULT_DARK 1        // Dark theme enabled
```

## File Organization

```
src/app/
├── display_driver.h              # HAL interface
├── display_manager.h/cpp         # Manager (owns everything)
├── screens.cpp                   # Compilation unit
├── lv_conf.h                     # LVGL configuration
├── drivers/
│   └── tft_espi_driver.h/cpp    # TFT_eSPI implementation
└── screens/
    ├── screen.h                  # Base class
    ├── splash_screen.h/cpp       # Boot screen
    ├── info_screen.h/cpp         # Device info screen
    └── test_screen.h/cpp         # Display test/calibration
```

## Best Practices

1. **Always use the HAL interface** - Don't access TFT_eSPI directly
2. **Keep screens stateless** - Reload data in `update()`, don't cache
3. **Test on round displays** - Verify 240x240 compatibility
4. **Use LVGL themes** - Leverage default theme for consistent styling
5. **Protect LVGL calls** - Use `lock()`/`unlock()` from outside rendering task
6. **Optimize update()** - Only update changed values, avoid full redraws
7. **Follow naming conventions** - snake_case for files, PascalCase for classes

## Future Enhancements

- LovyanGFX driver implementation
- Touch screen support (calibration, gestures)
- Screen navigation with buttons
- Settings screen for WiFi configuration
- Graph widgets for sensor data visualization
- Multi-language support with LVGL's text engine
