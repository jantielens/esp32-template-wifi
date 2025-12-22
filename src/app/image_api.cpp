/*
 * Image API Implementation
 * 
 * REST API handlers for uploading and displaying JPEG images.
 * Uses backend adapter pattern for portability across projects.
 */

#include "board_config.h"

#if HAS_IMAGE_API

#include "image_api.h"
#include "jpeg_preflight.h"
#include "log_manager.h"
#include "device_telemetry.h"

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

// ===== Constants =====

// Note: AsyncWebServer callbacks run on the AsyncTCP task. Do not block
// (e.g., with delay()/busy waits). If we're busy, return 409 and let the
// client retry.

// ===== Internal state =====

static ImageApiConfig g_cfg;
static ImageApiBackend g_backend = {nullptr, nullptr, nullptr};

// Image upload buffer (allocated temporarily during upload)
static uint8_t* image_upload_buffer = nullptr;
static size_t image_upload_size = 0;
static unsigned long image_upload_timeout_ms = 10000;

// Upload state tracking
enum UploadState {
    UPLOAD_IDLE = 0,
    UPLOAD_IN_PROGRESS,
    UPLOAD_READY_TO_DISPLAY
};
static volatile UploadState upload_state = UPLOAD_IDLE;
static volatile unsigned long pending_op_id = 0;  // Incremented when new op is queued

// Pending image display operation (processed by main loop)
struct PendingImageOp {
    uint8_t* buffer;
    size_t size;
    bool dismiss;  // true = dismiss current image, false = show new image
    unsigned long timeout_ms;  // Display timeout in milliseconds
    unsigned long start_time;  // Time when upload completed (for accurate timeout)
};
static PendingImageOp pending_image_op = {nullptr, 0, false, 10000, 0};

// Strip upload state (buffering during HTTP upload)
static uint8_t* current_strip_buffer = nullptr;
static size_t current_strip_size = 0;

// Strip processing state (async decode queue)
struct PendingStripOp {
    uint8_t* buffer;
    size_t size;
    uint8_t strip_index;
    int image_width;
    int image_height;
    int total_strips;
    unsigned long timeout_ms;
    unsigned long start_time;
};
static PendingStripOp pending_strip_op = {nullptr, 0, 0, 0, 0, 0, 10000, 0};

static bool is_jpeg_magic(const uint8_t* buf, size_t sz) {
    return (buf && sz >= 3 && buf[0] == 0xFF && buf[1] == 0xD8 && buf[2] == 0xFF);
}

static unsigned long parse_timeout_ms(AsyncWebServerRequest* request) {
    // Parse optional timeout parameter from query string (e.g., ?timeout=30)
    unsigned long timeout_seconds = g_cfg.default_timeout_ms / 1000;
    if (request->hasParam("timeout")) {
        String timeout_str = request->getParam("timeout")->value();
        timeout_seconds = (unsigned long)timeout_str.toInt();
        // Clamp to prevent overflow and respect max timeout
        unsigned long max_seconds = g_cfg.max_timeout_ms / 1000;
        if (timeout_seconds > max_seconds) timeout_seconds = max_seconds;
    }
    return timeout_seconds * 1000UL;
}

// ===== Handlers =====

// POST /api/display/image - Upload and display JPEG image (deferred decode)
static void handleImageUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    (void)filename;

    // First chunk - initialize upload
    if (index == 0) {
        // If upload already in progress OR pending display, reject (client can retry)
        if (upload_state == UPLOAD_IN_PROGRESS || upload_state == UPLOAD_READY_TO_DISPLAY) {
            request->send(409, "application/json", "{\"success\":false,\"message\":\"Upload busy\"}");
            return;
        }

        Logger.logBegin("Image Upload");
        Logger.logLinef("Total size: %u bytes", request->contentLength());

        image_upload_timeout_ms = parse_timeout_ms(request);
        Logger.logLinef("Timeout: %lu ms", image_upload_timeout_ms);

        device_telemetry_log_memory_snapshot("img pre-clear");

        // Free any pending image buffer to make room for new upload
        if (pending_image_op.buffer) {
            Logger.logMessage("Upload", "Freeing pending image buffer");
            free((void*)pending_image_op.buffer);
            pending_image_op.buffer = nullptr;
            pending_image_op.size = 0;
        }

        device_telemetry_log_memory_snapshot("img post-clear");

        // Check file size
        size_t total_size = request->contentLength();
        if (total_size > g_cfg.max_image_size_bytes) {
            Logger.logEnd("ERROR: Image too large");
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Image too large\"}");
            return;
        }

        // Check available memory (upload size + decode headroom)
        size_t free_heap = ESP.getFreeHeap();
        size_t required_heap = total_size + g_cfg.decode_headroom_bytes;
        if (free_heap < required_heap) {
            Logger.logLinef("ERROR: Insufficient memory (need %u, have %u)", required_heap, free_heap);
            device_telemetry_log_memory_snapshot("img insufficient");
            char error_msg[192];
            snprintf(error_msg, sizeof(error_msg),
                     "{\"success\":false,\"message\":\"Insufficient memory: need %uKB, have %uKB. Try reducing image size.\"}",
                     (unsigned)(required_heap / 1024), (unsigned)(free_heap / 1024));
            Logger.logEnd();
            request->send(507, "application/json", error_msg);
            return;
        }

        // Allocate buffer
        device_telemetry_log_memory_snapshot("img pre-alloc");
        image_upload_buffer = (uint8_t*)malloc(total_size);
        if (!image_upload_buffer) {
            Logger.logEnd("ERROR: Memory allocation failed");
            device_telemetry_log_memory_snapshot("img alloc-fail");
            request->send(507, "application/json", "{\"success\":false,\"message\":\"Memory allocation failed\"}");
            return;
        }
        device_telemetry_log_memory_snapshot("img post-alloc");

        image_upload_size = 0;
        upload_state = UPLOAD_IN_PROGRESS;
    }

    // Receive data chunks
    if (len && image_upload_buffer && upload_state == UPLOAD_IN_PROGRESS) {
        memcpy(image_upload_buffer + image_upload_size, data, len);
        image_upload_size += len;

        // Log progress every 10KB
        static size_t last_logged_size = 0;
        if (image_upload_size - last_logged_size >= 10240) {
            Logger.logLinef("Received: %u bytes", image_upload_size);
            last_logged_size = image_upload_size;
        }
    }

    // Final chunk - validate and queue for display
    if (final) {
        if (image_upload_buffer && image_upload_size > 0 && upload_state == UPLOAD_IN_PROGRESS) {
            Logger.logLinef("Upload complete: %u bytes", image_upload_size);

            if (!is_jpeg_magic(image_upload_buffer, image_upload_size)) {
                Logger.logLinef("Invalid header: %02X %02X %02X %02X",
                                image_upload_buffer[0], image_upload_buffer[1],
                                image_upload_buffer[2], image_upload_buffer[3]);
                Logger.logEnd("ERROR: Not a valid JPEG file");
                free(image_upload_buffer);
                image_upload_buffer = nullptr;
                image_upload_size = 0;
                upload_state = UPLOAD_IDLE;
                request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JPEG file\"}");
                return;
            }

            // Best-effort header preflight so we can return a descriptive 400 before queuing
            char preflight_err[160];
            if (!jpeg_preflight_tjpgd_supported(
                    image_upload_buffer,
                    image_upload_size,
                    g_cfg.lcd_width,
                    g_cfg.lcd_height,
                    preflight_err,
                    sizeof(preflight_err))) {
                Logger.logLinef("ERROR: JPEG preflight failed: %s", preflight_err);
                Logger.logEnd();
                free(image_upload_buffer);
                image_upload_buffer = nullptr;
                image_upload_size = 0;
                upload_state = UPLOAD_IDLE;

                char resp[256];
                snprintf(resp, sizeof(resp), "{\"success\":false,\"message\":\"%s\"}", preflight_err);
                request->send(400, "application/json", resp);
                return;
            }

            // Queue image for display by main loop (deferred operation)
            if (pending_image_op.buffer) {
                Logger.logMessage("Upload", "Replacing pending image");
                free((void*)pending_image_op.buffer);
            }

            pending_image_op.buffer = image_upload_buffer;
            pending_image_op.size = image_upload_size;
            pending_image_op.dismiss = false;
            pending_image_op.timeout_ms = image_upload_timeout_ms;
            pending_image_op.start_time = millis();
            pending_op_id++;
            upload_state = UPLOAD_READY_TO_DISPLAY;

            // main loop owns the buffer now
            image_upload_buffer = nullptr;
            image_upload_size = 0;

            Logger.logEnd("Image queued for display");

            char response_msg[160];
            snprintf(response_msg, sizeof(response_msg),
                     "{\"success\":true,\"message\":\"Image queued for display (%lus timeout)\"}",
                     (unsigned long)(image_upload_timeout_ms / 1000));
            request->send(200, "application/json", response_msg);
        } else {
            Logger.logEnd("ERROR: No data received");
            upload_state = UPLOAD_IDLE;
            request->send(400, "application/json", "{\"success\":false,\"message\":\"No data received\"}");
        }
    }
}

// DELETE /api/display/image - Manually dismiss image
static void handleImageDelete(AsyncWebServerRequest *request) {
    Logger.logMessage("Portal", "Image dismiss requested");

    if (pending_image_op.buffer) {
        free((void*)pending_image_op.buffer);
    }
    pending_image_op.buffer = nullptr;
    pending_image_op.size = 0;
    pending_image_op.dismiss = true;
    upload_state = UPLOAD_READY_TO_DISPLAY;
    pending_op_id++;

    request->send(200, "application/json", "{\"success\":true,\"message\":\"Image dismiss queued\"}");
}

// POST /api/display/image/strips?strip_index=N&strip_count=T&width=W&height=H[&timeout=seconds]
// Upload a single JPEG strip; decode is deferred to the main loop.
static void handleStripUpload(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    // Validate required params
    if (index == 0) {
        const bool has_required =
            request->hasParam("strip_index", false) &&
            request->hasParam("strip_count", false) &&
            request->hasParam("width", false) &&
            request->hasParam("height", false);

        if (!has_required) {
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Missing required parameters: strip_index, strip_count, width, height\"}");
            return;
        }
    }

    if (!request->hasParam("strip_index", false) || !request->hasParam("strip_count", false) ||
        !request->hasParam("width", false) || !request->hasParam("height", false)) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Missing required parameters: strip_index, strip_count, width, height\"}");
        return;
    }

    const int stripIndex = request->getParam("strip_index", false)->value().toInt();
    const int totalStrips = request->getParam("strip_count", false)->value().toInt();
    const int imageWidth = request->getParam("width", false)->value().toInt();
    const int imageHeight = request->getParam("height", false)->value().toInt();
    const unsigned long timeoutMs = request->hasParam("timeout", false)
        ? (unsigned long)request->getParam("timeout", false)->value().toInt() * 1000UL
        : g_cfg.default_timeout_ms;

    if (index == 0) {
        // Reject if we're busy. AsyncWebServer runs on AsyncTCP task; do not block.
        if (upload_state == UPLOAD_IN_PROGRESS || upload_state == UPLOAD_READY_TO_DISPLAY || pending_strip_op.buffer) {
            request->send(409, "application/json", "{\"success\":false,\"message\":\"Busy\"}");
            return;
        }

        // Only log first strip to reduce verbosity
        if (stripIndex == 0) {
            Logger.logMessagef("Strip Mode", "Uploading %dx%d image (%d strips)", imageWidth, imageHeight, totalStrips);
            device_telemetry_log_memory_snapshot("strip pre-alloc");
        }

        if (stripIndex < 0 || stripIndex >= totalStrips) {
            Logger.logEnd("ERROR: Invalid strip index");
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid strip index\"}");
            return;
        }

        if (imageWidth <= 0 || imageHeight <= 0 || imageWidth > g_cfg.lcd_width || imageHeight > g_cfg.lcd_height) {
            Logger.logLinef("ERROR: Invalid dimensions %dx%d", imageWidth, imageHeight);
            Logger.logEnd();
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid image dimensions\"}");
            return;
        }

        if (current_strip_buffer) {
            free((void*)current_strip_buffer);
            current_strip_buffer = nullptr;
        }

        current_strip_buffer = (uint8_t*)malloc(total);
        if (!current_strip_buffer) {
            Logger.logLinef("ERROR: Out of memory (requested %u bytes, free heap: %u)", (unsigned)total, ESP.getFreeHeap());
            device_telemetry_log_memory_snapshot("strip alloc-fail");
            Logger.logEnd();
            request->send(507, "application/json", "{\"success\":false,\"message\":\"Out of memory\"}");
            return;
        }

        current_strip_size = 0;
    }

    if (current_strip_buffer && current_strip_size + len <= total) {
        memcpy((uint8_t*)current_strip_buffer + current_strip_size, data, len);
        current_strip_size += len;
    }

    // Final chunk: decode synchronously before returning
    if (index + len >= total) {
        if (current_strip_size != total) {
            free((void*)current_strip_buffer);
            current_strip_buffer = nullptr;
            Logger.logEnd();
            request->send(500, "application/json", "{\"success\":false,\"message\":\"Incomplete upload\"}");
            return;
        }

        if (!is_jpeg_magic(current_strip_buffer, current_strip_size)) {
            free((void*)current_strip_buffer);
            current_strip_buffer = nullptr;
            Logger.logEnd();
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JPEG data\"}");
            return;
        }

        // Best-effort header preflight
        char preflight_err[160];
        const int remaining_height = imageHeight;
        if (!jpeg_preflight_tjpgd_fragment_supported(
                current_strip_buffer,
                current_strip_size,
                imageWidth,
                remaining_height,
                g_cfg.lcd_height,
                preflight_err,
                sizeof(preflight_err))) {
            Logger.logLinef("ERROR: JPEG fragment preflight failed: %s", preflight_err);
            free((void*)current_strip_buffer);
            current_strip_buffer = nullptr;
            Logger.logEnd();

            char resp[256];
            snprintf(resp, sizeof(resp), "{\"success\":false,\"message\":\"%s\"}", preflight_err);
            request->send(400, "application/json", resp);
            return;
        }

        // Queue strip for async decode (don't decode in HTTP handler)
        // If we're busy, reject and let client retry.
        if (upload_state == UPLOAD_IN_PROGRESS || upload_state == UPLOAD_READY_TO_DISPLAY || pending_strip_op.buffer) {
            free((void*)current_strip_buffer);
            current_strip_buffer = nullptr;
            Logger.logEnd();
            request->send(409, "application/json", "{\"success\":false,\"message\":\"Busy\"}" );
            return;
        }

        // Transfer strip buffer to pending operation
        upload_state = UPLOAD_IN_PROGRESS;
        pending_strip_op.buffer = current_strip_buffer;
        pending_strip_op.size = current_strip_size;
        pending_strip_op.strip_index = (uint8_t)stripIndex;
        pending_strip_op.image_width = imageWidth;
        pending_strip_op.image_height = imageHeight;
        pending_strip_op.total_strips = totalStrips;
        pending_strip_op.timeout_ms = timeoutMs;
        pending_strip_op.start_time = millis();
        
        current_strip_buffer = nullptr;
        current_strip_size = 0;
        
        upload_state = UPLOAD_READY_TO_DISPLAY;
        pending_op_id++;
        
        Logger.logMessagef("Strip", "Strip %d/%d queued for decode", stripIndex, totalStrips - 1);
        Logger.logEnd();

        char response[160];
        snprintf(response, sizeof(response),
                 "{\"success\":true,\"strip_index\":%d,\"strip_count\":%d,\"complete\":%s}",
                 stripIndex, totalStrips, (stripIndex == totalStrips - 1) ? "true" : "false");
        request->send(200, "application/json", response);
    }
}

// ===== Public API =====

void image_api_init(const ImageApiConfig& cfg, const ImageApiBackend& backend) {
    g_cfg = cfg;
    g_backend = backend;

    image_upload_timeout_ms = g_cfg.default_timeout_ms;

    // Best-effort: reset state
    upload_state = UPLOAD_IDLE;
    pending_op_id = 0;
    pending_image_op = {nullptr, 0, false, g_cfg.default_timeout_ms, 0};

    if (current_strip_buffer) {
        free((void*)current_strip_buffer);
        current_strip_buffer = nullptr;
    }
    current_strip_size = 0;

    if (pending_strip_op.buffer) {
        free((void*)pending_strip_op.buffer);
        pending_strip_op.buffer = nullptr;
    }
    pending_strip_op = {nullptr, 0, 0, 0, 0, 0, g_cfg.default_timeout_ms, 0};

    if (image_upload_buffer) {
        free((void*)image_upload_buffer);
        image_upload_buffer = nullptr;
    }
    image_upload_size = 0;
}

void image_api_register_routes(AsyncWebServer* server) {
    // Register the more specific /strips endpoint before /image.
    server->on(
        "/api/display/image/strips",
        HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        NULL,
        handleStripUpload
    );

    server->on(
        "/api/display/image",
        HTTP_POST,
        [](AsyncWebServerRequest *request) {},
        handleImageUpload
    );

    server->on("/api/display/image", HTTP_DELETE, handleImageDelete);
}

void image_api_process_pending(bool ota_in_progress) {
    static unsigned long last_processed_id = 0;

    if (upload_state != UPLOAD_READY_TO_DISPLAY || ota_in_progress) {
        return;
    }

    if (pending_op_id == last_processed_id) {
        return;
    }

    last_processed_id = pending_op_id;

    // Handle dismiss operation
    if (pending_image_op.dismiss) {
        device_telemetry_log_memory_snapshot("img dismiss");
        if (g_backend.hide_current_image) {
            g_backend.hide_current_image();
        }
        pending_image_op.dismiss = false;
        upload_state = UPLOAD_IDLE;
        return;
    }

    // Handle strip operation
    if (pending_strip_op.buffer && pending_strip_op.size > 0) {
        const uint8_t* buf = pending_strip_op.buffer;
        const size_t sz = pending_strip_op.size;
        const uint8_t strip_index = pending_strip_op.strip_index;
        const int total_strips = pending_strip_op.total_strips;

        Logger.logMessagef("Portal", "Processing strip %d/%d (%u bytes)", strip_index, total_strips - 1, (unsigned)sz);

        if (strip_index == 0) {
            device_telemetry_log_memory_snapshot("strip pre-decode");
        }

        // Initialize strip session on first strip
        if (strip_index == 0) {
            if (!g_backend.start_strip_session) {
                Logger.logMessage("Portal", "ERROR: No strip session handler");
                if (g_backend.hide_current_image) {
                    g_backend.hide_current_image();
                }
                free((void*)pending_strip_op.buffer);
                pending_strip_op.buffer = nullptr;
                upload_state = UPLOAD_IDLE;
                return;
            }

            if (!g_backend.start_strip_session(pending_strip_op.image_width, pending_strip_op.image_height, 
                                                pending_strip_op.timeout_ms, pending_strip_op.start_time)) {
                Logger.logMessage("Portal", "ERROR: Failed to init strip session");
                if (g_backend.hide_current_image) {
                    g_backend.hide_current_image();
                }
                free((void*)pending_strip_op.buffer);
                pending_strip_op.buffer = nullptr;
                upload_state = UPLOAD_IDLE;
                return;
            }
        }

        // Decode strip
        bool success = false;
        if (g_backend.decode_strip) {
            success = g_backend.decode_strip(buf, sz, strip_index, false);
        }

        if (strip_index == (uint8_t)(total_strips - 1)) {
            device_telemetry_log_memory_snapshot("strip post-decode");
        }

        free((void*)pending_strip_op.buffer);
        pending_strip_op.buffer = nullptr;
        pending_strip_op.size = 0;
        upload_state = UPLOAD_IDLE;

        if (!success) {
            Logger.logMessagef("Portal", "ERROR: Failed to decode strip %d", strip_index);
            device_telemetry_log_memory_snapshot("strip decode-fail");
            if (g_backend.hide_current_image) {
                g_backend.hide_current_image();
            }
        } else if (strip_index == total_strips - 1) {
            Logger.logMessagef("Portal", "\u2713 All %d strips decoded", total_strips);
        }
        return;
    }

    // Handle full image operation (fallback for full mode)
    if (pending_image_op.buffer && pending_image_op.size > 0) {
        const uint8_t* buf = pending_image_op.buffer;
        const size_t sz = pending_image_op.size;

        Logger.logMessagef("Portal", "Processing pending image (%u bytes)", (unsigned)sz);

        device_telemetry_log_memory_snapshot("img pre-decode");

        bool success = false;
        if (g_backend.start_strip_session && g_backend.decode_strip) {
            if (!g_backend.start_strip_session(g_cfg.lcd_width, g_cfg.lcd_height, pending_image_op.timeout_ms, pending_image_op.start_time)) {
                Logger.logMessage("Portal", "ERROR: Failed to init image display");
                success = false;
            } else {
                success = g_backend.decode_strip(buf, sz, 0, false);
            }
        }

        device_telemetry_log_memory_snapshot("img post-decode");

        free((void*)pending_image_op.buffer);
        pending_image_op.buffer = nullptr;
        pending_image_op.size = 0;
        upload_state = UPLOAD_IDLE;

        if (!success) {
            Logger.logMessage("Portal", "ERROR: Failed to display image");
            device_telemetry_log_memory_snapshot("img decode-fail");
            if (g_backend.hide_current_image) {
                g_backend.hide_current_image();
            }
        }
        return;
    }

    // Invalid state
    upload_state = UPLOAD_IDLE;
}

#endif // HAS_IMAGE_API
