# Memory Optimizations (ESP32 / No-PSRAM + Display)

Target context: this repo running **all subsystems enabled** (LVGL UI + Async web portal + MQTT + OTA + image API) with the worst-case action being **download + decode + display a JPEG over HTTPS** on **no-PSRAM display boards** (e.g. `cyd-v2`).

This document is deliberately opinionated and allows breaking changes (APIs, behavior, libraries).

---

## How to measure (so “estimated gains” become real)

Before/after every change, capture at least:

- **Free heap**: `ESP.getFreeHeap()`
- **Largest free block** (fragmentation proxy): `heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)`
- **Internal heap free**: `heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)`
- **PSRAM free** (when present): `heap_caps_get_free_size(MALLOC_CAP_SPIRAM)`
- Optional: `esp_get_free_heap_size()` and `heap_caps_get_minimum_free_size()`

Recommended test sequence (CYD-v2):

1) Boot → idle on Info screen (no portal use)
2) Open portal pages repeatedly (home/network/firmware)
3) Trigger HTTPS JPEG download+display repeatedly (20–100 times)
4) Run MQTT publish loop for 10+ minutes

Optional: automate portal/API churn tests

If you want repeatable numbers without copying serial logs, you can drive the test using `/api/health`:

```bash
# API-only (focus on JSON churn): config save + screen switch + health sample, repeated
python3 tools/portal_stress_test.py --host 192.168.1.111 --no-reboot --cycles 10 --scenario api

# Portal + assets (fetch HTML/CSS/JS in addition to API actions)
python3 tools/portal_stress_test.py --host 192.168.1.111 --no-reboot --cycles 10 --scenario portal

# If you see occasional timeouts under load, increase timeout + retries
python3 tools/portal_stress_test.py --host 192.168.1.111 --no-reboot --cycles 10 --scenario portal --timeout 8 --retries 5 --retry-sleep 0.3

# Write a CSV so you can diff runs across firmware changes
python3 tools/portal_stress_test.py --host 192.168.1.111 --no-reboot --cycles 10 --scenario api --out /tmp/portal_api_run.csv

# Stress the full-image upload + decode path (generates a test JPEG each cycle)
python3 tools/portal_stress_test.py --host 192.168.1.111 --no-reboot --cycles 10 --scenario image --image-generate 320x240 --out /tmp/portal_image_run.csv
```

The script prints per-cycle `heap_largest`/`heap_fragmentation` and a min/max summary, using `/api/health` as the source of truth.

When using `--scenario image`, the CSV also includes `img_upload_result` (e.g. `ok`, `too_large`, `insufficient_memory`) and the uploaded `img_jpeg_bytes`. The stress run is designed to continue even if some cycles return `HTTP 507` (so you can quantify failure rates instead of aborting).

Success criteria to aim for:

- Largest free block remains stable (does not monotonically shrink)
- No “sawtooth” heap growth across repeated JPEG displays

---
## Summary Table: All Optimization Items

| # | Category | Optimization | Est. Gain | Measured | Fragmentation Impact | Complexity | Priority |
|---|----------|--------------|-----------|----------|---------------------|------------|----------|
| **0** | Web Assets | PROGMEM verification, cache headers, AsyncTCP stack tuning | 0-8KB | Baseline: hl +4096B, frag -4pp (cyd-v2). Portal churn spike recorded in notes | Low | Low-Med | Medium |
| **1** | Image API | Eliminate full-image buffering (stream-to-decode) | +80-180KB | Not implemented (yet): high complexity. Also ruled out “stream-to-flash then decode” due to frequent uploads (flash wear) | **High** | High | **Critical** |
| **2** | HTTPS | Fully streaming download (no response buffering) | +20-80KB | — | Med-High | Med-High | **Critical** |
| **3** | JSON | Remove String temporaries in portal/MQTT | +2-20KB | Portal churn: hl stayed 77812B (no drop), frag 47→37 (cyd-v2) | **Medium** | Low-Med | **High** |
| **4** | JSON | Replace JsonDocument with StaticJsonDocument | Variable | Implemented (no DynamicJsonDocument in src/app) | Med-High | Low-Med | **High** |
| **5** | JPEG Decode | Reuse decoder buffers per session | +8-24KB | Image stress: after_cycle mins hf 121900→146196, hl 73716→81908, hi 76908→101204 (cyd-v2, 320x240 x5) | Mixed | Medium | High |
| **6** | LVGL | Switch to custom allocator (LV_MEM_CUSTOM=1) | +48KB | Build: DRAM -49168B, hl +49168B. Runtime: heap_free +40KB, hl +20480B (cyd-v2) | Mixed | High | Medium |
| **7** | LVGL | Reduce draw buffer size | +2-6KB | — | Low | Low | Low |
| **8** | LVGL | Disable unused fonts | +10-30KB flash | — | Low | Low | Low |
| **9** | LVGL | Reduce screen/widget complexity | Variable | — | Low | Medium | Low |
| **10** | Config | Lower IMAGE_API_MAX_SIZE_BYTES for no-PSRAM | Indirect | Worst-case 320x240 Q95 noise JPEGs ~77KB, ok under 80KB cap (cyd-v2). PSRAM QSPI boards use 300KB. Image API precheck is PSRAM-aware (runtime `psramFound()` so PSRAM-capable SoCs without PSRAM don't get falsely rejected) | Medium | Low | Medium |
| **11** | Config | Reduce IMAGE_STRIP_BATCH_MAX_ROWS | +1-4KB | — | Low | Low | Low |
| **12** | Config | Tune headroom thresholds for no-PSRAM | Indirect | — | Low | Low | Low |
| **13** | JSON | Use sized StaticJsonDocument (8+ instances) | Variable | Implemented (no `JsonDocument doc;` in src/app) | Med-High | Low-Med | **High** |
| **14** | LVGL | Disable unused widgets | +5-15KB flash | Build: -68204B flash (cyd-v2) | Low | Low | Low |
| **15** | FreeRTOS | Reduce LVGL task stack (8192→4096-6144) | +2-4KB | — | N/A | Medium | Medium |
| **16** | FreeRTOS | Reduce telemetry task stack (2048→1280-1536) | +512-768B | — | N/A | Low | Low |
| **17** | WiFi | Eliminate .toString() String temporaries | +15-25B/call | — | Low-Med | Low | Medium |
| **18** | mDNS | Disable or lazy-init mDNS | +1-3KB | — | Low | Low-Med | Low |
| **L1** | Logging | Add log levels + compile-time stripping | Indirect | — | Low | Medium | Medium |
| **L2** | Logging | Eliminate String temporaries in log statements | Low-Med | — | Low-Med | Low | Medium |
| **L3** | Logging | Rate-limit logs in tight loops | Indirect | — | Low | Low | Low |
| **L4** | Logging | Async logger with ring buffer | Indirect | — | Low | High | Low |

### Table Legend:

**Priority Ratings:**
- **Critical**: Required for no-PSRAM HTTPS+JPEG worst-case stability
- **High**: Significant fragmentation reduction, pairs well with critical items
- **Medium**: Worthwhile optimization with good ROI
- **Low**: Optional polish or niche benefits

**Complexity:**
- **Low**: Simple code changes, minimal testing needed
- **Medium**: Requires testing, monitoring, or moderate refactoring
- **High**: Major architectural changes, extensive testing required

**Gain Notes:**
- Ranges show typical values; actual gains depend on workload
- "Indirect" means improved stability/reliability more than raw KB savings
- Flash savings noted separately from RAM where applicable

---
## High-impact optimizations (priority order)

### 0) Static web assets: confirm flash-resident, reduce per-request RAM churn

**Measured (before/after)**

```text
Board: cyd-v2 (no PSRAM)
Scenario: Baseline (boot + setup + heartbeat after a few minutes)

Before:
  [Mem] boot  hf=232404 hm=226808 hl=110580 hi=187412 hin=181860 frag=52 pf=0 pm=0 pl=0
  [Mem] setup hf=130404 hm=130036 hl=81908  hi=85412  hin=85088  frag=37 pf=0 pm=0 pl=0
  [Mem] hb   hf=127448 hm=100920 hl=73716  hi=82456  hin=55972  frag=42 pf=0 pm=0 pl=0

After:
  [Mem] boot  hf=232404 hm=226808 hl=110580 hi=187412 hin=181860 frag=52 pf=0 pm=0 pl=0
  [Mem] setup hf=130380 hm=130032 hl=81908  hi=85388  hin=85084  frag=37 pf=0 pm=0 pl=0
  [Mem] hb   hf=127440 hm=98700  hl=77812  hi=82448  hin=53752  frag=38 pf=0 pm=0 pl=0

Notes: After switching to `beginResponse_P` for PROGMEM assets and adding cache headers for CSS/JS.

Portal interaction observation (not isolated to static assets): after saving config via portal and switching screens (test → info), next heartbeat showed a fragmentation hit:

  Before portal actions:
    [Mem] hb hf=127440 hm=98700 hl=77812 hi=82448 hin=53752 frag=38 pf=0 pm=0 pl=0
  After config save + screen switches:
    [Mem] hb hf=123916 hm=98700 hl=65524 hi=78924 hin=53752 frag=47 pf=0 pm=0 pl=0

This likely reflects runtime churn in portal API JSON/config handling and/or LVGL screen construction rather than static asset transfer.
```

**What**

The embedded HTML/CSS/JS assets in this repo are stored as `const uint8_t ...[] PROGMEM` (gzipped) in the generated header:

- [src/app/web_assets.h](../src/app/web_assets.h)

On ESP32, `PROGMEM` data is flash-resident (memory-mapped). Serving these assets should not require allocating a full copy of the asset in heap.

However, serving assets still uses RAM for:

- Per-request response objects and headers
- AsyncTCP send buffers while the socket drains
- Concurrency (multiple clients multiplies the above)

**Change**

- Prefer the PROGMEM-aware response helper if your ESPAsyncWebServer version supports it: use `beginResponse_P(...)` for these assets.
- Add caching headers (especially for `portal.css` and `portal.js`) to reduce repeated downloads and therefore reduce concurrent request pressure.
- Consider making AsyncTCP stack size board-tunable (see the separate item on stack sizing) because this is fixed RAM pressure unrelated to asset storage.

**Estimated gain**

- Peak heap reduction: **small** (often **0–a few KB**) per request; main benefit is *stability under repeated portal refreshes*
- RAM pressure reduction (stack): **up to ~8KB** if you can safely reduce AsyncTCP task stack on no-PSRAM targets

**Pros**

- Low-risk, easy to test
- Reduces RAM churn during portal usage

**Cons**

- Cache headers are behavior changes (clients may serve cached UI while device firmware changes)
- `beginResponse_P` availability depends on the ESPAsyncWebServer version

**Complexity / risk**

- **Low** for cache headers
- **Low–Medium** for `beginResponse_P` / stack tuning (needs runtime verification)

### 1) Eliminate full-image buffering for image display (stream-to-decode)

**Status**

- Not implemented yet.
- We explicitly avoided the “spool to flash then decode” variant because this project expects many uploads (flash wear concern).
- Mitigation work landed under item #10 instead (caps + PSRAM-aware precheck + stress tooling), while keeping full-image upload support.

**What**

Today, `image_api.cpp` supports a “full image upload” path that allocates a single contiguous buffer (via `image_api_alloc(total_size)`) and stores the entire JPEG before decode. That’s a textbook cause of **heap pressure + fragmentation**, especially after repeated allocations of varying sizes.

Note: We also tested a “persistent upload buffer reuse” approach to reduce alloc/free churn. On no-PSRAM boards this was counterproductive: it effectively pins a large block in the 8-bit heap and collapses `heap_largest`, so it was removed.

**Experiment (rejected): persistent upload buffer reuse on cyd-v2**

- Workload: 20-cycle worst-case 320x240 Q95 noise uploads (~77KB) via `tools/portal_stress_test.py --scenario image`.
- With persistent buffer reuse: uploads succeeded, but `heap_largest` collapsed to ~7–13KB and fragmentation stayed ~78–88%.
- With per-upload alloc/free: uploads succeeded and `heap_largest` stayed high (~90–94KB) with fragmentation ~35–43%.

- File: [src/app/image_api.cpp](../src/app/image_api.cpp)
- Behavior: `POST /api/display/image` allocates up to `IMAGE_API_MAX_SIZE_BYTES` (defaults to 100KB; `cyd-v2` overrides to 80KB; PSRAM QSPI boards override to 300KB).

**Change**

- Prefer/require strip mode for all image display.
- If you must keep a “single request” API, implement a streaming HTTP handler that:
  - Validates JPEG header using a small fixed buffer (e.g. first 1–4KB)
  - Immediately feeds data into the decoder as it arrives
  - Never allocates `O(image_size)` RAM

This likely means changing the Image API contract and/or client tooling (OK per your constraints).

**Estimated gain**

- Free heap improvement: **+80–180KB** during image display (depends on image size and existing fragmentation)
- Fragmentation reduction: **high** (removes large/variable-size allocations)

**Pros**

- Biggest fragmentation win in the repo
- Makes “repeat JPEG display” stable

**Cons**

- Harder to implement correctly with Async callbacks + decode scheduling
- Need robust cancelation/backpressure (409/busy handling)

**Complexity / risk**

- **High** (touches API behavior, async flow control, decoder integration)

---

### 2) Make HTTPS download fully streaming (no full response buffering)

**Measured (before/after)**

```text
Board: cyd-v2 (no PSRAM)

Build (arduino-cli compile output):
  Before (LV_MEM_CUSTOM=0):
    Sketch uses 1348951 bytes
    Global variables use 100632 bytes
    Leaving 227048 bytes for local variables
  After (LV_MEM_CUSTOM=1):
    Sketch uses 1347535 bytes
    Global variables use 51464 bytes
    Leaving 276216 bytes for local variables

Runtime stress (tools/portal_stress_test.py, host 192.168.1.111, 10 cycles):
  API scenario:
    Before: heap_free min=121616 max=124476, heap_largest min=73716 max=77812, heap_fragmentation min=36 max=40
    After:  heap_free min=162168 max=166244, heap_largest min=94196 max=94196, heap_fragmentation min=41 max=43
  Portal scenario:
    Before: heap_free min=121588 max=121624, heap_largest min=73716 max=73716, heap_fragmentation min=39 max=39
    After:  heap_free min=161804 max=162240, heap_largest min=94196 max=94196, heap_fragmentation min=41 max=41

Notes:
  - Net win on cyd-v2: heap_free +~40KB, heap_largest +20480B.
  - Baseline API run showed a mid-run largest-block drop (77812→73716) that did not occur after.
```

**What**

The worst-case you described is HTTPS + JPEG. Even if image upload is handled well, the “download from Internet” path frequently gets implemented as “download full body into a `String`/buffer”. That causes both pressure and fragmentation.

This repo already has image strip decoding infrastructure (`StripDecoder`) that can work with small buffers; the missing piece is ensuring HTTPS transport is also streaming.

**Change**

- Use a streaming client (`WiFiClientSecure`) and read in small fixed chunks (e.g. 1–4KB) from the TLS socket.
- Avoid `HTTPClient::getString()` / `String` concatenation.
- Consider pinning a TLS I/O buffer (static) to reduce repeated alloc/free patterns.

**Estimated gain**

- Peak heap reduction: **+20–80KB** (varies by HTTP library behavior)
- Fragmentation reduction: **medium-high**

**Pros**

- Critical for album-art style workloads
- Works even if JPEG sizes vary

**Cons**

- TLS itself still needs memory; you can only shrink the “application layer” allocations

**Complexity / risk**

- **Medium–High** (depends where HTTPS is currently implemented)

---

### 3) Stop creating large transient `String` payloads for JSON responses

**Measured (before/after)**

```text
Board: cyd-v2 (no PSRAM)
Scenario: Portal churn (config save + screen switches; compare next heartbeat)

Before (pre-change; config save + screen switches correlated with fragmentation hit):
  Before portal actions:
    [Mem] hb hf=127440 hm=98700 hl=77812 hi=82448 hin=53752 frag=38 pf=0 pm=0 pl=0
  After config save + screen switches:
    [Mem] hb hf=123916 hm=98700 hl=65524 hi=78924 hin=53752 frag=47 pf=0 pm=0 pl=0

After (serialize JSON without String temporaries; stream HTTP responses / bounded MQTT payloads):
  Before portal actions:
    [Mem] hb hf=127720 hm=98940 hl=77812 hi=82728 hin=53992 frag=39 pf=0 pm=0 pl=0
  After screen switch (test→info) + config save:
    [Mem] hb hf=124860 hm=93752 hl=77812 hi=79868 hin=48804 frag=37 pf=0 pm=0 pl=0

Notes:
  - Measured effect: avoided the post-save largest-block drop (77812→65524) seen before, and reduced the post-save frag spike (47→37).
  - This result is not perfectly isolated: it reflects the combined “no String JSON” changes plus the JSON sizing work in items 4/13.
```

**What**

The portal and MQTT code frequently does:

- `JsonDocument doc; ...`
- `String response; serializeJson(doc, response);`

This creates variable-sized heap allocations inside `String`, and repeated usage is a common fragmentation source.

- File: [src/app/web_portal.cpp](../src/app/web_portal.cpp)
- File: [src/app/mqtt_manager.cpp](../src/app/mqtt_manager.cpp)

**Change**

- For HTTP: serialize directly to `AsyncResponseStream` instead of building a `String`.
- For MQTT: serialize into a fixed `char payload[MQTT_MAX_PACKET_SIZE]` (or a `StaticJsonDocument` + `serializeJson(doc, buf, sizeof(buf))`).

If the payload doesn’t fit, either:

- Reduce the payload (preferred), or
- Increase `MQTT_MAX_PACKET_SIZE` only when PSRAM exists.

**Estimated gain**

- Fragmentation reduction: **medium**
- Peak heap reduction: **+2–20KB** per request/publish (more important is stability)

**Pros**

- Easy win; removes many small-to-medium transient allocs
- Improves determinism

**Cons**

- Need to size buffers carefully
- Might force trimming telemetry fields

**Complexity / risk**

- **Low–Medium**

---

### 4) Replace `JsonDocument doc;` (dynamic) with sized `StaticJsonDocument<N>`

**Measured (before/after)**

```text
Board: cyd-v2 (no PSRAM)
Scenario: Portal churn (config save + screen switches; compare next heartbeat)

Before (dynamic JsonDocument growth + String serialization in portal/MQTT paths):
  After config save + screen switches:
    [Mem] hb hf=123916 hm=98700 hl=65524 hi=78924 hin=53752 frag=47 pf=0 pm=0 pl=0

After (sized StaticJsonDocument + streaming/bounded serialization):
  After screen switch (test→info) + config save:
    [Mem] hb hf=124860 hm=93752 hl=77812 hi=79868 hin=48804 frag=37 pf=0 pm=0 pl=0

Notes:
  - This measurement is shared with item 3 because the practical win is the combination: sized docs + no String temporaries.
  - The primary goal here is predictability (fixed capacities + overflow/NoMemory handling), with fragmentation improvement as a side effect.
```

**What**

`JsonDocument` without an explicit capacity is a dynamic allocator choice in ArduinoJson v7 (it grows as needed). This is friendly for development and bad for fragmentation on constrained devices.

**Change**

- Identify each JSON payload and give it a fixed maximum size (or use ArduinoJson’s Assistant / empirical sizing).
- Replace dynamic docs with `StaticJsonDocument<N>`.

Start with:

- `handleGetConfig()`
- `handleGetHealth()`
- MQTT state payload in `publishHealthNow()`

Status (implemented):

- Web portal handlers now use `StaticJsonDocument` and stream JSON responses (with overflow checks and `413` on `NoMemory`).
- MQTT health payload is serialized from a sized `StaticJsonDocument` into a bounded publish buffer.

**Estimated gain**

- Fragmentation reduction: **medium**
- Peak heap reduction: **+1–10KB** depending on payload sizes

**Pros**

- Predictable memory
- Great for no-PSRAM targets

**Cons**

- Sizing mistakes become runtime truncation/failure
- Makes schema changes slightly more annoying

**Complexity / risk**

- **Low–Medium**

---

### 5) Turn off LVGL’s internal heap and route LVGL allocations to ESP-IDF heap

**Measured (before/after)**

```text
Board:
Scenario:

Before:
  [Mem] boot:
  [Mem] setup:
  [Mem] hb:

After:
  [Mem] boot:
  [Mem] setup:
  [Mem] hb:

Notes:
```

**What**

LVGL is currently configured with:

- `LV_MEM_CUSTOM 0`
- `LV_MEM_SIZE (48U * 1024U)`

That means LVGL maintains its own separate memory pool. This can be good for fragmentation isolation, but it also:

- Reserves 48KB that you can’t use for TLS, JPEG, etc.
- Can fail inside LVGL even when the system heap is still available

**Change**

Switch to custom allocator:

- `LV_MEM_CUSTOM 1`
- `LV_MEM_CUSTOM_ALLOC/REALLOC/FREE` mapped to `heap_caps_malloc` variants

And decide a strategy:

- No-PSRAM devices: allocate LVGL objects from internal 8-bit heap
- PSRAM devices: allow some LVGL allocations in PSRAM (careful: not all drivers tolerate it)

**Estimated gain**

- Available heap increase: **+48KB** (reclaimed LVGL pool)
- Fragmentation: **mixed** (may increase system heap churn unless you also adopt “LVGL arena” below)

**Pros**

- Makes memory more fungible between subsystems
- Can prevent “LVGL pool exhausted” while plenty of heap remains

**Cons**

- If LVGL alloc/free patterns are chatty, system fragmentation can worsen

**Complexity / risk**

- **Medium** (touches LVGL config + stability testing)

---

### 6) Introduce a dedicated “image arena” / long-lived buffers to avoid churn

**Measured (before/after)**

```text
Board: cyd-v2 (no PSRAM)
Scenario: Full image upload stress (generated 320x240 JPEG), 5 cycles
Command:
  python3 tools/portal_stress_test.py --host 192.168.1.111 --no-reboot --cycles 5 --scenario image --image-generate 320x240

Before (baseline firmware):
  after_cycle mins: hf=121900 hl=73716 hi=76908 frag=37
  CSV: /tmp/portal_stress_baseline_image_full_320x240.csv

After (decoder buffers reused per session):
  after_cycle mins: hf=146196 hl=81908 hi=101204 frag=43
  CSV: /tmp/portal_stress_after_item5_image_full_320x240.csv

Delta (after - before):
  after_cycle mins: hf +24296B, hl +8192B, hi +24296B

Notes:
  - `heap_fragmentation` is computed as (1 - hl/hf) * 100, so it can increase if total free heap rises faster than the largest free block.
```

**What**

Even with strip decoding, the current decoder allocates and frees several buffers per strip decode (`work`, `line_buffer`, optional `batch_buffer`). Repeated cycles can still fragment the heap.

- File: [src/app/strip_decoder.cpp](../src/app/strip_decoder.cpp)

**Change**

- Convert per-strip allocations into a **single long-lived allocation per session**:
  - `work` buffer cached
  - `line_buffer` cached
  - optional `batch_buffer` cached

Allocate once at `begin()` and free at `end()`.

Optionally, allocate from a dedicated heap region (internal only for no-PSRAM).

**Estimated gain**

- Fragmentation reduction: **medium-high**
- Peak heap reduction: small (mostly stability), but can save **a few KB** by reusing buffers

**Pros**

- Great for repeated image refresh
- Makes memory usage more predictable

**Cons**

- Needs careful reset to avoid stale state

**Complexity / risk**

- **Medium**

---

### 7) Reduce LVGL draw buffer size aggressively on no-PSRAM targets

**Measured (before/after)**

```text
Board:
Scenario:

Before:
  [Mem] boot:
  [Mem] setup:
  [Mem] hb:

After:
  [Mem] boot:
  [Mem] setup:
  [Mem] hb:

Notes:
```

**What**

LVGL draw buffer (`LVGL_BUFFER_SIZE`) is a large contiguous allocation in `DisplayManager::initLVGL()`.

- File: [src/app/display_manager.cpp](../src/app/display_manager.cpp)
- Default: `DISPLAY_WIDTH * 10`

On CYD-v2 (320x240, RGB565):

- 10 lines → $320\*10\*2 = 6400$ bytes

This isn’t huge, but it’s contiguous and it’s always present. You can go smaller if you need heap headroom.

**Change**

- For no-PSRAM devices, set buffer to 4–8 lines (board override).
- If performance is acceptable, consider 1–2 lines.

**Estimated gain**

- Peak heap increase: **+2–10KB** (depends on current override)
- Fragmentation reduction: low (it’s one persistent allocation)

**Pros**

- Easy, predictable

**Cons**

- Slower UI flush (more chunks)

**Complexity / risk**

- **Low**

---

### 8) Drop LVGL features you don’t need (perf monitor, extra widgets/fonts)

**Measured (before/after)**

```text
Board:
Scenario:

Before:
  [Mem] boot:
  [Mem] setup:
  [Mem] hb:

After:
  [Mem] boot:
  [Mem] setup:
  [Mem] hb:

Notes:
```

**What**

`lv_conf.h` enables many widgets and `LV_USE_PERF_MONITOR 1` and multiple Montserrat fonts.

- File: [src/app/lv_conf.h](../src/app/lv_conf.h)

Each feature can add static RAM/flash and may allocate runtime objects.

**Change**

- Disable `LV_USE_PERF_MONITOR` if you don’t rely on it.
- Remove unused widgets (`LV_USE_CANVAS` etc.)
- Reduce fonts to a single size if acceptable.

**Estimated gain**

- RAM: **small–medium** (often a few KB)
- Flash: can be significant depending on fonts

**Pros**

- Shrinks footprint across all boards

**Cons**

- Reduces UI feature set

**Complexity / risk**

- **Low** (but needs UI verification)

---

### 9) Lower AsyncTCP task stack for no-PSRAM targets (or make it board-tunable)

**Measured (before/after)**

```text
Board:
Scenario:

Before:
  [Mem] boot:
  [Mem] setup:
  [Mem] hb:

After:
  [Mem] boot:
  [Mem] setup:
  [Mem] hb:

Notes:
```

**What**

`web_portal.cpp` sets:

- `CONFIG_ASYNC_TCP_STACK_SIZE 16384`

Stack is not heap, but it is RAM pressure and reduces headroom for everything else.

- File: [src/app/web_portal.cpp](../src/app/web_portal.cpp)

**Change**

- Make the stack size board-configurable.
- For no-PSRAM, try 8192–12288 if stable.

**Estimated gain**

- RAM recovered: **+4–8KB**

**Pros**

- Simple, immediate RAM relief

**Cons**

- Too small causes crashes under load

**Complexity / risk**

- **Medium** (needs stress testing with many concurrent requests)

---

### 10) Reduce OTA + portal concurrency (serialize “big memory” operations)

**Measured (before/after)**

```text
Board:
Scenario:

Before:
  [Mem] boot:
  [Mem] setup:
  [Mem] hb:

After:
  [Mem] boot:
  [Mem] setup:
  [Mem] hb:

Notes:
```

**What**

The worst-case scenario is TLS+JPEG+LVGL. Trying to do that concurrently with OTA or heavy portal use multiplies peak usage.

**Change**

- Define a global “high-memory operation lock”:
  - Only one of: OTA, HTTPS image download/decode, large portal response
- When locked, other operations return 409 “busy”.

**Estimated gain**

- Peak heap reduction: **high** (prevents additive worst-case peaks)

**Pros**

- Very effective without deep refactors

**Cons**

- Behavior change (requests may fail/retry)

**Complexity / risk**

- **Low–Medium**

---

### 11) Replace remaining `String` usage in hot paths with fixed buffers

**Measured (before/after)**

```text
Board:
Scenario:

Before:
  [Mem] boot:
  [Mem] setup:
  [Mem] hb:

After:
  [Mem] boot:
  [Mem] setup:
  [Mem] hb:

Notes:
```

**What**

`String` is convenient but a frequent fragmentation culprit. Notable hotspots:

- Portal response building and parsing
- Config default name (`config_manager_get_default_device_name()` returns `String`)
- MQTT JSON payload building

**Change**

- Replace with `char[]` buffers and `snprintf`.
- Avoid concatenation loops.

**Estimated gain**

- Fragmentation reduction: **medium**

**Pros**

- Heap becomes more predictable

**Cons**

- More verbose code

**Complexity / risk**

- **Low–Medium**

---

### 12) Prefer internal allocations for TLS + networking buffers on no-PSRAM

**Measured (before/after)**

```text
Board:
Scenario:

Before:
  [Mem] boot:
  [Mem] setup:
  [Mem] hb:

After:
  [Mem] boot:
  [Mem] setup:
  [Mem] hb:

Notes:
```

**What**

On many ESP32 setups, TLS and socket buffers need internal RAM. If other subsystems consume internal RAM first, TLS becomes flaky.

**Change**

- Reserve internal RAM headroom by moving optional buffers (e.g., strip batch buffer) to PSRAM when available.
- For no-PSRAM, reduce optional buffers (disable batch or reduce rows).

**Estimated gain**

- Stability improvement (fewer TLS failures) more than raw KB

**Pros**

- Makes HTTPS more reliable

**Cons**

- May reduce throughput/perf

**Complexity / risk**

- **Low**

---

## Logging: can it improve memory/fragmentation?

### Summary

In this codebase, logging is **not a primary heap fragmentation source** by itself:

- The logger builds log lines using **fixed-size stack buffers** and `snprintf`/`vsnprintf`.
- It writes to `Serial.print(...)`.
- There is no obvious “append-to-String” or heap-backed log buffering in the logger implementation.

That said, logging can still contribute to memory pressure *indirectly* by:

- Adding **temporary `String` allocations in log arguments** (e.g., `WiFi.softAPIP().toString()`), which can fragment heap if invoked frequently.
- Increasing **CPU time / blocking** in time-sensitive tasks (AsyncTCP callbacks, decode loops), which can cause watchdog resets or timing issues that look like “memory problems”.

Relevant files:

- [src/app/log_manager.cpp](../src/app/log_manager.cpp)
- [src/app/log_manager.h](../src/app/log_manager.h)

---

### L1) Add log levels + compile-time stripping of verbose logs

**Measured (before/after)**

```text
Board:
Scenario:

Before:
  [Mem] boot:
  [Mem] setup:
  [Mem] hb:

After:
  [Mem] boot:
  [Mem] setup:
  [Mem] hb:

Notes:
```

**What**

Many modules log verbose progress (e.g., image upload progress, OTA progress). Even if this doesn’t allocate heap, it consumes stack and CPU and can interfere with networking/timeouts.

**Change**

- Introduce `LOG_LEVEL` (e.g., ERROR/WARN/INFO/DEBUG/TRACE).
- Provide macros like `LOGI(...)`, `LOGD(...)` that compile out when disabled.
- Ensure formatting work (`vsnprintf`) is skipped entirely when the level is off.

**Estimated gain**

- Heap/fragmentation: **low** (mostly indirect)
- Stability/perf: **medium–high** under load (less blocking in hot paths)
- Flash: **small–medium** (format strings and call sites can be compiled out)

**Pros**

- Big improvement in worst-case behavior (Async networking + decoding)
- Lets you keep deep logs for debug builds

**Cons**

- Requires touching many call sites to classify severity

**Complexity / risk**

- **Medium**

---

### L2) Eliminate `String` temporaries inside log statements

**Measured (before/after)**

```text
Board:
Scenario:

Before:
  [Mem] boot:
  [Mem] setup:
  [Mem] hb:

After:
  [Mem] boot:
  [Mem] setup:
  [Mem] hb:

Notes:
```

**What**

Some logs pass `.toString().c_str()` (e.g., `WiFi.softAPIP().toString().c_str()`), which allocates a temporary `String` on heap. If such logs happen repeatedly (or under retries), they can contribute to fragmentation.

**Change**

- Avoid `.toString()` in logs; log IPs using `snprintf` with octets or print `IPAddress` directly if supported.
- Avoid `String` concatenation in log-only paths.

**Estimated gain**

- Fragmentation reduction: **low–medium** (depends on how often those logs fire)
- Peak heap: usually **small**

**Pros**

- Removes a hidden heap allocator in “should be harmless” logs

**Cons**

- Slightly more verbose code

**Complexity / risk**

- **Low**

---

### L3) Avoid logging inside tight loops / per-chunk handlers

**Measured (before/after)**

```text
Board:
Scenario:

Before:
  [Mem] boot:
  [Mem] setup:
  [Mem] hb:

After:
  [Mem] boot:
  [Mem] setup:
  [Mem] hb:

Notes:
```

**What**

Even stack-only logging can be expensive. In particular:

- HTTP upload callbacks (AsyncTCP task) should avoid extra work.
- JPEG decode loops should minimize I/O to keep latency down.

**Change**

- Rate-limit progress logs (e.g., once per N KB or once per second).
- Prefer a single summary line at completion.

**Estimated gain**

- Heap/fragmentation: **low**
- Stability/perf: **medium**

**Complexity / risk**

- **Low**

---

### L4) Optional: asynchronous logger with fixed ring buffer

**Measured (before/after)**

```text
Board:
Scenario:

Before:
  [Mem] boot:
  [Mem] setup:
  [Mem] hb:

After:
  [Mem] boot:
  [Mem] setup:
  [Mem] hb:

Notes:
```

**What**

If you want maximum robustness under heavy networking/decoding, make logging non-blocking by pushing log lines into a fixed ring buffer and flushing from a low-priority task.

**Estimated gain**

- Heap/fragmentation: **low** (you’ll reserve a fixed buffer)
- Stability/perf: **high** in worst-case “HTTPS + JPEG + portal” load

**Pros**

- Prevents `Serial.print` latency from stalling critical work

**Cons**

- Fixed RAM cost (ring buffer)
- Complexity and failure modes (buffer overflow handling)

**Complexity / risk**

- **High**

---

## Notes tied to current code

- Full image upload path allocates a single contiguous buffer up to `IMAGE_API_MAX_SIZE_BYTES`. Current board defaults/overrides:
  - No-PSRAM: `cyd-v2` overrides to 80KB in [src/boards/cyd-v2/board_overrides.h](../src/boards/cyd-v2/board_overrides.h)
  - PSRAM QSPI: `jc3248w535` and `jc3636w518` override to 300KB in their board overrides
  This allocation can still fail if the largest free block is small even when total free heap is large.
  The Image API memory precheck is PSRAM-aware:
  - If PSRAM is present (`psramFound()`), keep an internal headroom guard and expect the upload buffer to land in PSRAM.
  - If PSRAM is not present (including PSRAM-capable SoCs where the board has no PSRAM fitted), use the no-PSRAM style guard (total heap + largest 8-bit block) to avoid false rejections.
- Strip decoding still allocates 3 buffers per decode in [src/app/strip_decoder.cpp](../src/app/strip_decoder.cpp). Reusing buffers per session should reduce churn.
- LVGL uses a fixed 48KB internal pool configured in [src/app/lv_conf.h](../src/app/lv_conf.h). Reclaiming that pool is an easy 48KB win but must be balanced against fragmentation risk.

---

---

## Additional optimizations discovered in final sweep

### 13) Use sized `StaticJsonDocument` or constrain dynamic allocations

**Measured (before/after)**

```text
Board: cyd-v2 (no PSRAM)
Scenario: Portal churn (config save + screen switches; compare next heartbeat)

Before (dynamic JSON allocations present in portal/config flows):
  After config save + screen switches:
    [Mem] hb hf=123916 hm=98700 hl=65524 hi=78924 hin=53752 frag=47 pf=0 pm=0 pl=0

After (sized StaticJsonDocument everywhere in src/app/** + bounded serialization):
  After screen switch (test→info) + config save:
    [Mem] hb hf=124860 hm=93752 hl=77812 hi=79868 hin=48804 frag=37 pf=0 pm=0 pl=0

Notes:
  - This captures the “JSON sweep” effect as experienced by the portal: reduced churn during config save + UI changes.
  - If you need a more isolated measurement, run tools/portal_stress_test.py across firmware revisions that differ only in JSON sizing.
```

**What**

ArduinoJson v7’s `JsonDocument` grows dynamically when you declare it without a capacity (e.g. `JsonDocument doc;`). That’s convenient, but it can create variable-sized heap allocations and fragmentation under repeated use.

In this repo we now avoid that pattern in the main firmware (`src/app/**`): all JSON payloads are built with explicit `StaticJsonDocument<N>` capacities (plus bounded serialization buffers for MQTT and streaming responses for HTTP).

**Change**

- Replace with `StaticJsonDocument<N>` where N is carefully sized to fit the largest expected payload.
- For responses, this pairs well with item 3 (serialize directly to stream/buffer instead of String).
- Document maximum sizes for each JSON payload type to ensure buffers are adequate.

Status (implemented in `src/app/**`):

- Portal JSON:
  - `/api/config` response uses `StaticJsonDocument<2048>` with `doc.overflowed()` guard.
  - `/api/config` request body uses `StaticJsonDocument<2048>` with `413` on `DeserializationError::NoMemory`.
  - `/api/health` response uses `StaticJsonDocument<1024>` with `doc.overflowed()` guard.
  - Display control request bodies use `StaticJsonDocument<256>`.
- MQTT JSON:
  - Health state uses `StaticJsonDocument<768>` and serializes into a bounded `char payload[MQTT_MAX_PACKET_SIZE]`.
- Home Assistant discovery JSON:
  - Each discovery payload uses `StaticJsonDocument<768>` with overflow check.

This leaves no `JsonDocument doc;` declarations in `src/app/**`.

Example:
```cpp
// Before:
JsonDocument doc;
doc["key"] = value;
String response;
serializeJson(doc, response);

// After:
StaticJsonDocument<512> doc;  // Size determined by measureJson() during testing
doc["key"] = value;
char response[512];
serializeJson(doc, response, sizeof(response));
```

**Estimated gain**

- Fragmentation reduction: **medium-high** (removes 8+ variable-sized allocations)
- Predictability: **high** (fixed memory footprint, fails fast if undersized)

**Pros**

- Deterministic memory usage
- Catches oversized payloads during development
- Pairs well with removing String temporaries

**Cons**

- Requires testing to determine correct sizes
- Need separate sizes for different payload types
- Slightly less flexible than dynamic allocation

**Complexity / risk**

- **Low-Medium** (straightforward replacement, main work is sizing)

---

### 14) Disable unused LVGL widgets to reduce code and static footprint

**Measured (before/after)**

```text
Board: cyd-v2
Scenario: build (program storage size)

Before:
  Sketch uses 1417067 bytes (72%) of program storage space.

After:
  Sketch uses 1348863 bytes (68%) of program storage space.

Delta:
  -68204 bytes flash

Notes:
  - Needed explicit LVGL "extra widget" disables because LVGL defaults compile them unless overridden in lv_conf.h.
```

**What**

[src/app/lv_conf.h](../src/app/lv_conf.h) enables many LVGL widgets that may not be used:

- `LV_USE_ARC`, `LV_USE_BAR`, `LV_USE_BTNMATRIX`, `LV_USE_CANVAS`, `LV_USE_CHECKBOX`, `LV_USE_DROPDOWN`, `LV_USE_ROLLER`, `LV_USE_SWITCH`, `LV_USE_TEXTAREA`, `LV_USE_TABLE`
- `LV_USE_FLEX` and `LV_USE_GRID` layouts
- `LV_USE_PERF_MONITOR`

Current screens (splash, info, test) use only: labels, a spinner (which depends on arcs), and basic objects/styles.

**Change**

- Audit actual widget usage in [src/app/screens/](../src/app/screens/) directory
- Set unused widget defines to `0`
- Keep only what is actually used by our screens (at minimum: `LV_USE_LABEL`, `LV_USE_SPINNER`, `LV_USE_ARC`)
- Disable complex widgets: `CANVAS`, `TABLE`, `TEXTAREA`, `DROPDOWN`, `ROLLER`, `CHECKBOX`, `SWITCH`

**Estimated gain**

- Flash: **~5-15KB** typical; **measured -68KB on cyd-v2** (includes disabling LVGL "extra" widgets that were enabled by default)
- RAM: **small-medium** (less widget state structures and style data)
- Compile time: **small improvement**

**Pros**

- No runtime cost
- Reduces attack surface (fewer code paths)
- Makes LVGL behavior more predictable

**Cons**

- Future UI features may need to re-enable widgets
- Need to document which widgets are available

**Complexity / risk**

- **Low** (verify no compile errors, test existing screens)

---

### 15) Reduce LVGL task stack size when HAS_IMAGE_API is disabled

**Measured (before/after)**

```text
Board:
Scenario:

Before:
  [Mem] boot:
  [Mem] setup:
  [Mem] hb:

After:
  [Mem] boot:
  [Mem] setup:
  [Mem] hb:

Notes:
```

**What**

LVGL task is created with 8192 bytes stack in [src/app/display_manager.cpp](../src/app/display_manager.cpp) line 362:

```cpp
xTaskCreatePinnedToCore(lvgl_task, "LVGL", 8192, NULL, 1, NULL, 1);
```

When image API is disabled (no JPEG decoding), LVGL operations are simpler (just widget rendering and touch input). The generous 8KB stack may be overkill.

**Change**

- Make LVGL task stack size configurable via `LVGL_TASK_STACK_SIZE` in board_config.h
- Default to 8192 for safety
- For no-PSRAM boards without image API, test with 4096-6144 bytes
- Monitor stack high water mark: `uxTaskGetStackHighWaterMark()`

**Estimated gain**

- RAM: **2-4KB** if reduced to 4096-6144 bytes
- Safety margin: need careful testing

**Pros**

- Immediate fixed RAM savings
- Tunable per board

**Cons**

- Risk of stack overflow if undersized
- Need runtime monitoring initially

**Complexity / risk**

- **Medium** (must test all screen transitions and touch events)

---

### 16) Reduce CPU monitoring task stack (currently 2048 bytes)

**Measured (before/after)**

```text
Board:
Scenario:

Before:
  [Mem] boot:
  [Mem] setup:
  [Mem] hb:

After:
  [Mem] boot:
  [Mem] setup:
  [Mem] hb:

Notes:
```

**What**

Device telemetry task is created with 2048 bytes stack in [src/app/device_telemetry.cpp](../src/app/device_telemetry.cpp). The task reads CPU stats, temperature sensor, and updates telemetry state every second using mostly stack variables and simple calculations.

**Change**

- Monitor actual stack usage with `uxTaskGetStackHighWaterMark()`
- If comfortable margin exists, reduce to 1536 or 1280 bytes
- Make configurable: `TELEMETRY_TASK_STACK_SIZE` in board_config.h

**Estimated gain**

- RAM: **512-768 bytes** if reduced by ~25-40%

**Pros**

- Low-risk (simple task with no deep call chains)
- Easy to revert if needed

**Cons**

- Very small gain
- Need to test across all board variants

**Complexity / risk**

- **Low** (task code is simple and predictable)

---

### 17) Eliminate `.toString().c_str()` WiFi String temporaries in app.ino

**Measured (before/after)**

```text
Board:
Scenario:

Before:
  [Mem] boot:
  [Mem] setup:
  [Mem] hb:

After:
  [Mem] boot:
  [Mem] setup:
  [Mem] hb:

Notes:
```

**What**

[src/app/app.ino](../src/app/app.ino) has multiple instances of `.toString().c_str()` creating temporary String objects:

- Line 45: `WiFi.softAPIP().toString().c_str()`
- Line 75: `WiFi.localIP().toString().c_str()`
- Line 228: (similar pattern)
- Lines 340-347: Multiple `WiFi.localIP().toString()` calls

Each `.toString()` allocates a String (typically 15-20 bytes for IP address + String overhead). When called during WiFi events or connection monitoring, these contribute to heap churn.

**Change**

Replace with direct IPAddress formatting:
```cpp
// Before:
Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());

// After:
IPAddress ip = WiFi.localIP();
Serial.printf("IP: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
```

Or use a small stack buffer:
```cpp
char ip_str[16];
IPAddress ip = WiFi.localIP();
snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
Logger.logMessage("WiFi", ip_str);
```

**Estimated gain**

- Fragmentation reduction: **low-medium** (removes 6-10 temporary allocations)
- Per-call cost: **15-25 bytes** of heap churn

**Pros**

- Simple find-and-replace style fix
- Removes hidden allocations from "should be cheap" operations
- Pairs well with L2 (logging String temporaries)

**Cons**

- More verbose code

**Complexity / risk**

- **Low** (straightforward code change, no behavior change)

---

### 18) Optional: Disable mDNS when not needed or lazy-initialize

**Measured (before/after)**

```text
Board:
Scenario:

Before:
  [Mem] boot:
  [Mem] setup:
  [Mem] hb:

After:
  [Mem] boot:
  [Mem] setup:
  [Mem] hb:

Notes:
```

**What**

mDNS service (ESPmDNS library) allocates memory for service records, TXT records, and response buffers. In [sample/esp32-energymon-169lcd/src/app/app.ino](../sample/esp32-energymon-169lcd/src/app/app.ino), extensive mDNS setup adds multiple TXT records (firmware version, chip model, features, etc.).

While useful for device discovery, mDNS consumes RAM (exact amount varies, typically 2-4KB) and may not be critical on all deployments.

**Change**

Option A: Make mDNS conditional:
```cpp
#ifndef HAS_MDNS
#define HAS_MDNS true  // Default enabled
#endif
```

Option B: Lazy initialization - only start mDNS on first web portal access or after user enables it in config.

Option C: Minimize TXT records - keep only essential ones (version, model), remove "note", "features", "url" decorative records.

**Estimated gain**

- RAM: **1-3KB** depending on TXT record complexity
- Per deployment: varies (some users need mDNS for .local access)

**Pros**

- Immediate RAM savings if disabled
- Reduces network traffic (no mDNS advertisements)

**Cons**

- Users relying on .local addressing need alternative discovery
- Feature regression if disabled by default

**Complexity / risk**

- **Low** for conditional compilation
- **Medium** for lazy init (need proper lifecycle management)

---

## Suggested "starter set" (highest ROI for CYD-v2)

If you want the smallest set of changes that likely fixes your stated worst-case (HTTPS JPEG display) on no-PSRAM display boards:

1) Remove/disable full-image buffering; require strip/stream decode
2) Make HTTPS download streaming (no full response buffering)
3) Remove `String` JSON building in portal + MQTT
4) Reuse decoder buffers per session
5) Reduce optional buffers (batch rows) for no-PSRAM
6) Replace `JsonDocument` with `StaticJsonDocument<N>` (item 13)
7) Eliminate `.toString()` WiFi String temporaries (item 17)
