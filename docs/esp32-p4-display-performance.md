# ESP32-P4 Display Performance Analysis

> **Purpose:** Reference document comparing our Arduino_GFX-based DSI display driver
> against Waveshare's official ESP-IDF sample projects. Use this when investigating
> display instability, low FPS, visual tearing, or planning performance optimizations.
>
> **Date:** February 2026  
> **Board:** Waveshare ESP32-P4-WIFI6-Touch-LCD-4B  
> **Panel:** ST7703 MIPI-DSI 720×720 IPS, 2-lane 480 Mbps/lane

## Reference Implementations

| Project | Framework | LVGL | Source |
|---|---|---|---|
| **Ours** | Direct ESP-IDF DSI + Arduino (arduino-cli) | 8.4.0 | `src/app/drivers/st7703_dsi_driver.cpp` |
| **NINA Display** | Waveshare BSP (ESP-IDF 5.4.0) | 9.2.0 | `sample/ESP32-P4-NINA-Display/` |
| **Smart-86-Box** | Waveshare BSP (ESP-IDF 5.4.0) | 8.4.x | `sample/ESP32-P4-Smart-86-Box-Smarthome-Panel/` |

Both sample projects use the `waveshare/esp32_p4_wifi6_touch_lcd_4b` BSP component,
which abstracts all DSI panel initialization, backlight, and touch behind a
`bsp_display_start_with_config()` API.

---

## 1. Framebuffer Architecture

All three implementations share the same fundamental architecture:

```
LVGL partial render buffer (internal SRAM)
  → flush callback copies pixels to DPI framebuffer
    → esp_cache_msync() flushes dirty cache lines
      → DSI DMA reads clean data from PSRAM → panel
```

**Our driver** (`st7703_dsi_driver.cpp` — direct ESP-IDF, bypassing Arduino_GFX):
- Calls `esp_lcd_new_panel_dpi()` with `num_fbs = 1` (single DPI framebuffer)
- Sets `.flags.use_dma2d = true` and **`.flags.disable_lp = true`** (see §6)
- Framebuffer allocated in PSRAM by ESP-IDF internally (~1 MB for 720×720 RGB565)

**Pixel copy path** (our `pushColors()`):
- Row-by-row `memcpy()` from LVGL buffer → PSRAM framebuffer
- After copy: `esp_cache_msync()` with `ESP_CACHE_MSYNC_FLAG_DIR_C2M` flushes cache

**Why we bypassed Arduino_GFX for DSI:**
Arduino_GFX v1.6.5's `Arduino_ESP32DSIPanel` does not expose the `disable_lp` flag.
With `disable_lp=false` (default), the DSI host enters Low-Power mode during every
blanking interval, causing cyan/idle-color flashes on the ST7703 panel (see §6).
The direct ESP-IDF path also replaces the per-pixel copy loop with `memcpy()` per row,
contributing to the FPS improvement.

---

## 2. Configuration Differences

### LVGL Draw Buffer

| Setting | Ours | NINA | Smart-86-Box |
|---|---|---|---|
| **Buffer lines** | 40 | BSP default (KConfig) | BSP default (KConfig) |
| **Buffer size** | 720 × 40 = 57,600 px (112 KB) | `BSP_LCD_DRAW_BUFF_SIZE` | `BSP_LCD_DRAW_BUFF_SIZE` |
| **Memory** | **Internal RAM** | PSRAM (`buff_spiram=true`) | **Internal RAM** (`buff_spiram=false`) |
| **Double buffer** | No (single) | `BSP_LCD_DRAW_BUFF_DOUBLE` | `BSP_LCD_DRAW_BUFF_DOUBLE` |

**Key finding:** Smart-86-Box deliberately uses `buff_spiram=false` for internal RAM
LVGL draw buffers. Internal SRAM access is ~3–5× faster than PSRAM for random writes,
which matters during LVGL rendering. Trade-off: internal RAM is scarce (~500 KB total).
We now also use internal RAM draw buffers (`LVGL_BUFFER_PREFER_INTERNAL true`).

Our code supports double buffering via `LVGL_DRAW_BUF_COUNT == 2` in
`board_overrides.h`, but it is **not enabled** for the P4 board.  Empirical testing
showed that double buffering **hurts** FPS on this board (30 → 20 fps) because the
flush is a synchronous CPU memcpy — not an async DMA transfer.  Both LVGL buffers
and the 1 MB DPI framebuffer compete for L2 cache lines, causing thrashing.
See §9 for full test results.

### LVGL Refresh Period

| Setting | Ours | NINA | Smart-86-Box |
|---|---|---|---|
| **Refresh period** | 15 ms (override) | 15 ms | 15 ms |
| **Target FPS** | ~66 fps | ~66 fps | ~66 fps |

LVGL 8.4.0 default `LV_DISP_DEF_REFR_PERIOD` is 30 ms. Both sample repos explicitly
set 15 ms. We now override this at runtime via `LVGL_REFR_PERIOD_MS 15` in
`board_overrides.h` (applied in `display_manager.cpp` after display driver registration
using `lv_timer_set_period()`). This had **zero FPS cost** in testing.

Relevant config paths:
- **Ours:** `LVGL_REFR_PERIOD_MS 15` in `board_overrides.h` → runtime override
- **NINA:** `sdkconfig.defaults` → `CONFIG_LV_DEF_REFR_PERIOD=15`
- **Smart-86-Box:** `sdkconfig.defaults` → `CONFIG_LV_DISP_DEF_REFR_PERIOD=15`

### LVGL 9.x-Only Features (NINA)

These features are unavailable in LVGL 8.4.0:

| Feature | Setting | Impact |
|---|---|---|
| **PPA (Pixel Processing Accelerator)** | `CONFIG_LV_USE_PPA=y` | Hardware-accelerated blend/fill/rotate — significant CPU offload |
| **Multi-threaded SW rendering** | `CONFIG_LV_DRAW_SW_DRAW_UNIT_CNT=2` | Uses both RISC-V cores for parallel rendering |

### Build System / sdkconfig

Both IDF sample repos explicitly configure the ESP32-P4 memory subsystem:

```ini
# Memory subsystem (both NINA and Smart-86-Box)
CONFIG_SPIRAM_SPEED_200M=y           # PSRAM at 200 MHz (max)
CONFIG_SPIRAM_XIP_FROM_PSRAM=y       # Execute code from PSRAM (frees flash bandwidth)
CONFIG_CACHE_L2_CACHE_256KB=y        # Maximum L2 cache (256 KB)
CONFIG_CACHE_L2_CACHE_LINE_128B=y    # 128-byte cache lines (optimal for DMA reads)

# Compiler and runtime
CONFIG_COMPILER_OPTIMIZATION_PERF=y  # GCC -O2 (vs Arduino's -Os)
CONFIG_FREERTOS_HZ=1000              # 1 kHz tick (vs Arduino default 100 Hz on some cores)

# LVGL performance (both repos)
CONFIG_LV_ATTRIBUTE_FAST_MEM_USE_IRAM=y  # Hot LVGL functions in IRAM (single-cycle)
```

**Arduino build:** These are controlled by the ESP32 Arduino core's built-in
`sdkconfig` defaults for the P4 target. We do not have direct control without a
custom board JSON or forked Arduino core. The P4 Arduino support is relatively new
and defaults may not be display-optimized.

### Smart-86-Box Additional Tuning

```ini
CONFIG_LV_LAYER_SIMPLE_BUF_SIZE=102400   # 100 KB layer buffer
CONFIG_LV_IMG_CACHE_DEF_SIZE=5           # Image cache slots
CONFIG_LV_GRAD_CACHE_DEF_SIZE=10240      # 10 KB gradient cache
CONFIG_LV_USE_PERF_MONITOR=y             # Runtime perf overlay
```

---

## 3. Impact Summary (Ranked)

| # | Gap | Impact | Status |
|---|---|---|---|
| 1 | **No PPA acceleration** | HIGH | Requires LVGL 9.x migration |
| 2 | **Single-threaded rendering** | HIGH | Requires LVGL 9.x migration |
| 3 | **Arduino sdkconfig defaults** (PSRAM speed, L2 cache, `-Os`) | HIGH | Custom board JSON or Arduino core fork |
| 4 | ~~Double buffering~~ | ~~MEDIUM-HIGH~~ | **REJECTED** — caused 30→20 fps regression (cache thrashing) |
| 5 | **15 ms refresh period** | FREE | **APPLIED** — `LVGL_REFR_PERIOD_MS 15` in board_overrides.h |
| 6 | ~~120-line draw buffer~~ | ~~MEDIUM~~ | **REJECTED** — contributed to FPS regression with double buffering |
| 7 | ~~LVGL draw buffer in PSRAM~~ | ~~MEDIUM~~ | **APPLIED** — `LVGL_BUFFER_PREFER_INTERNAL true`, +4 fps |
| 8 | **No IRAM fast-mem for LVGL** | LOW-MEDIUM | Needs custom `lv_conf.h` |
| 9 | **`-Os` vs `-O2` optimization** | LOW-MEDIUM | Arduino platform limitation |
| 10 | ~~Pixel copy: for-loop vs memcpy~~ | ~~LOW~~ | **APPLIED** — direct ESP-IDF driver uses `memcpy()` per row |

---

## 4. Quick Wins (No Framework Migration)

Changes that can be made in `src/boards/esp32-p4-lcd4b/board_overrides.h`:

### ✅ Lower refresh period (APPLIED)
```cpp
#define LVGL_REFR_PERIOD_MS 15  // Runtime override, zero FPS cost
```
Applied via `lv_timer_set_period()` in `display_manager.cpp` after display driver
registration. No `lv_conf.h` needed.

### ❌ Enable double buffering (REJECTED)
```cpp
#define LVGL_DRAW_BUF_COUNT 2  // DO NOT USE — causes 30→20 fps regression
```
Double buffering helps when the flush is asynchronous (e.g., DMA to SPI panel).
Our flush is a synchronous CPU copy from PSRAM draw buffer → PSRAM DPI framebuffer.
The second buffer doubles PSRAM cache pressure without pipelining benefit, causing
L2 cache thrashing and a ~33% FPS drop.

### ❌ Increase draw buffer size (REJECTED)
```cpp
#define LVGL_BUFFER_SIZE (DISPLAY_WIDTH * 120)  // DO NOT USE — increases cache pressure
```
Tested alongside double buffering. Combined with the DPI framebuffer, larger PSRAM
buffers exceed the L2 cache working set and cause thrashing. Keep at 40 lines.

### ✅ Use internal RAM for draw buffers (APPLIED)
```cpp
#define LVGL_BUFFER_PREFER_INTERNAL true
#define LVGL_BUFFER_SIZE (DISPLAY_WIDTH * 40)   // 56 KB — fits easily in ~500 KB internal SRAM
```
Faster CPU writes during rendering. This is what Smart-86-Box does (`buff_spiram=false`).
Measured +4 fps improvement. ESP32-P4 has ~500 KB internal SRAM — 56 KB is well within budget.

---

## 5. DSI Timing Reference

```
DPI pixel clock:  38 MHz
DSI lane rate:    480 Mbps × 2 lanes
HSYNC:            pw=20, bp=50, fp=50
VSYNC:            pw=4,  bp=20, fp=20
```

Theoretical frame rate at 38 MHz:
```
H_total = 720 + 20 + 50 + 50 = 840 pixels
V_total = 720 + 4 + 20 + 20 = 764 lines
FPS = 38,000,000 / (840 × 764) ≈ 59.2 fps
```

The DSI panel hardware can display ~59 fps. With `LVGL_REFR_PERIOD_MS 15` applied,
the LVGL timer ceiling (~66 fps) exceeds the panel rate. The bottleneck is now
LVGL rendering throughput (measured at ~30 fps on the FPS test screen).

---

## 6. LP (Low Power) Transitions During Blanking — FIXED

Arduino_GFX v1.6.5 does **not** set `disable_lp = true` in `esp_lcd_dpi_panel_config_t.flags`.
With `disable_lp=false` (default), the DSI host enters Low-Power mode during every
horizontal and vertical blanking interval (HBP, HFP, VBP, VFP, VSA). Each LP↔HS
transition has D-PHY ramp latency. On the ST7703 panel this manifested as visible
cyan/idle-color flashes — hundreds of transitions per frame.

**Root cause confirmed:** Reading `Arduino_ESP32DSIPanel.cpp` source showed
`dpi_config.flags` only sets `.use_dma2d = true`, with no API to control `disable_lp`.

**Fix applied (February 2026):** Bypassed Arduino_GFX entirely for DSI init.
The driver (`st7703_dsi_driver.cpp`) now calls ESP-IDF APIs directly:
- `esp_lcd_new_dsi_bus()` → DSI bus
- `esp_lcd_new_panel_io_dbi()` → command channel
- `esp_lcd_new_panel_dpi()` → DPI panel with **`.flags.disable_lp = true`**
- `esp_lcd_panel_io_tx_param()` → vendor init commands
- `esp_lcd_panel_init()` → start DMA refresh
- `esp_lcd_dpi_panel_get_frame_buffer()` → framebuffer pointer for LVGL flush

The Arduino_GFX library is no longer used for the DSI display path. It is still
used by other boards (jc3248w535, jc3636w518) which use QSPI interfaces and are
not affected by DSI LP transitions.

**Result:** Cyan flashes eliminated. FPS improved by ~8 (30 → ~40 fps) due to
both the continuous HS mode and the switch from per-pixel copy to `memcpy()`.

---

## 7. Empirical Test Results (February 2026)

A/B testing on the FPS test screen to isolate the impact of each change:

| Test | Buffer | Lines | Double | Refresh | FPS | Notes |
|---|---|---|---|---|---|---|
| Baseline | PSRAM | 40 | No | 30 ms | **30** | Original configuration |
| All changes | PSRAM | 120 | Yes | 15 ms | **20** | All "quick wins" applied — 33% regression |
| Isolate A | PSRAM | 40 | Yes | 15 ms | **21** | Buffer size not the cause |
| Isolate B | PSRAM | 40 | No | 15 ms | **30** | Double buffering is the culprit |

**Conclusions:**
- **15 ms refresh period:** Free — no FPS cost. Kept.
- **Double buffering:** −33% FPS. The flush is a synchronous CPU memcpy between two
  PSRAM regions (draw buffer → DPI framebuffer). Both buffers + the 1 MB DPI
  framebuffer compete for L2 cache lines. With Arduino's sdkconfig defaults
  (L2 cache size unknown, likely < 256 KB), this causes thrashing. Reverted.
- **120-line buffer:** Not independently tested (only tested with double buffering).
  Likely increases cache pressure further. Reverted to 40 lines.
- **Cyan flash:** Root cause confirmed as DSI LP transitions (see §6). Fixed by
  bypassing Arduino_GFX and setting `disable_lp=true` via direct ESP-IDF calls.

### Internal RAM draw buffer (tested separately)

| Test | Buffer Mem | FPS | Notes |
|---|---|---|---|
| PSRAM buffer | PSRAM | **30** | Baseline (with 15ms refresh) |
| Internal RAM | Internal SRAM | **34** | +4 fps — less cache contention |

Applied: `LVGL_BUFFER_PREFER_INTERNAL true` in board_overrides.h.

### Direct ESP-IDF DSI driver (tested separately, cumulative)

| Test | DSI Driver | disable_lp | FPS | Notes |
|---|---|---|---|---|
| Arduino_GFX + internal buf | Arduino_GFX | false | **34** | LP flashes still present |
| Direct ESP-IDF + internal buf | ESP-IDF direct | true | **~40** | +6 fps, flashes eliminated |

---

## 8. Diagnostic Commands

### Check actual PSRAM speed at runtime
```cpp
Serial.printf("PSRAM size: %d bytes\n", ESP.getPsramSize());
Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
```

### Measure flush performance
The display manager's perf tracking (`g_perf.fps`, `g_perf.lv_timer_us`) reports:
- **fps:** Actual frames rendered per second
- **lv_timer_us:** Time spent in `lv_timer_handler()` per call (rendering + flush)

If `lv_timer_us` consistently exceeds 15 ms, LVGL can't sustain 66 fps even
with the refresh period lowered.

### LVGL built-in perf monitor (Smart-86-Box approach)
Requires `LV_USE_PERF_MONITOR=1` in `lv_conf.h` — shows FPS and CPU% overlay.
Not available without a custom `lv_conf.h` in Arduino builds.

---

## 9. File References

| File | Role |
|---|---|
| `src/app/drivers/st7703_dsi_driver.cpp` | Our DSI driver (direct ESP-IDF calls, disable_lp=true) |
| `src/app/drivers/st7703_dsi_driver.h` | Driver header (ESP-IDF handles, no Arduino_GFX) |
| `src/boards/esp32-p4-lcd4b/board_overrides.h` | P4 board config (buffer sizes, pins) |
| `src/app/display_manager.cpp` | LVGL init, buffer alloc, flush callback, task loop |
| `sample/ESP32-P4-NINA-Display/sdkconfig.defaults` | NINA display sdkconfig |
| `sample/ESP32-P4-NINA-Display/main/main.c` | NINA BSP display init |
| `sample/ESP32-P4-Smart-86-Box-Smarthome-Panel/ESP-IDF/Smarthome-Panel/sdkconfig.defaults` | Smart-86-Box sdkconfig |
| `sample/ESP32-P4-Smart-86-Box-Smarthome-Panel/ESP-IDF/Smarthome-Panel/main/main.c` | Smart-86-Box BSP display init |
| Arduino_GFX `src/databus/Arduino_ESP32DSIPanel.cpp` | Reference: DSI panel bus (disable_lp missing here) |
| Arduino_GFX `src/display/Arduino_DSI_Display.cpp` | Reference: Framebuffer drawing + cache sync |
