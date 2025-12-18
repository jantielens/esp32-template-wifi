/*
 * Strip Decoder Implementation
 * 
 * Decodes JPEG strips using TJpgDec and writes directly to LCD via DisplayDriver.
 * Performs RGB→BGR color swap during pixel write.
 */

#include "board_config.h"

#if HAS_IMAGE_API

#include "strip_decoder.h"
#include "display_driver.h"
#include "log_manager.h"

// Use the ESP-ROM TJpgDec types/signatures. This matches the ROM-provided jd_prepare/jd_decomp
// symbols used by the ESP32 Arduino core.
//
// Note: not all Arduino-ESP32 installs ship headers for every ESP32-family target.
// We select based on the IDF target macro when available, and fall back to
// __has_include for toolchains that don't define CONFIG_IDF_TARGET_*.
#if defined(CONFIG_IDF_TARGET_ESP32)
    #include <esp32/rom/tjpgd.h>
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
    #if __has_include(<esp32s2/rom/tjpgd.h>)
        #include <esp32s2/rom/tjpgd.h>
    #else
        #error "Missing <esp32s2/rom/tjpgd.h> in this Arduino-ESP32 install"
    #endif
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    #include <esp32s3/rom/tjpgd.h>
#elif defined(CONFIG_IDF_TARGET_ESP32C2)
    #if __has_include(<esp32c2/rom/tjpgd.h>)
        #include <esp32c2/rom/tjpgd.h>
    #else
        #error "Missing <esp32c2/rom/tjpgd.h> in this Arduino-ESP32 install"
    #endif
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
    #include <esp32c3/rom/tjpgd.h>
#elif defined(CONFIG_IDF_TARGET_ESP32C5)
    #include <esp32c5/rom/tjpgd.h>
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
    #include <esp32c6/rom/tjpgd.h>
#elif defined(CONFIG_IDF_TARGET_ESP32H2)
    #if __has_include(<esp32h2/rom/tjpgd.h>)
        #include <esp32h2/rom/tjpgd.h>
    #else
        #error "Missing <esp32h2/rom/tjpgd.h> in this Arduino-ESP32 install"
    #endif
#elif defined(CONFIG_IDF_TARGET_ESP32P4)
    #if __has_include(<esp32p4/rom/tjpgd.h>)
        #include <esp32p4/rom/tjpgd.h>
    #else
        #error "Missing <esp32p4/rom/tjpgd.h> in this Arduino-ESP32 install"
    #endif
#else
    // Fallback for unknown targets - try common variants
    #if __has_include(<esp32/rom/tjpgd.h>)
        #include <esp32/rom/tjpgd.h>
    #elif __has_include(<esp32s3/rom/tjpgd.h>)
        #include <esp32s3/rom/tjpgd.h>
    #elif __has_include(<esp32c6/rom/tjpgd.h>)
        #include <esp32c6/rom/tjpgd.h>
    #elif __has_include(<esp32c5/rom/tjpgd.h>)
        #include <esp32c5/rom/tjpgd.h>
    #elif __has_include(<esp32c3/rom/tjpgd.h>)
        #include <esp32c3/rom/tjpgd.h>
    #elif __has_include(<esp32s2/rom/tjpgd.h>)
        #include <esp32s2/rom/tjpgd.h>
    #elif __has_include(<esp32c2/rom/tjpgd.h>)
        #include <esp32c2/rom/tjpgd.h>
    #elif __has_include(<esp32h2/rom/tjpgd.h>)
        #include <esp32h2/rom/tjpgd.h>
    #elif __has_include(<esp32p4/rom/tjpgd.h>)
        #include <esp32p4/rom/tjpgd.h>
    #else
        #error "Unsupported ESP32 target for TJpgDec (no ROM tjpgd.h found)"
    #endif
#endif

// Input buffer context for TJpgDec
struct JpegInputContext {
    const uint8_t* data;
    size_t size;
    size_t pos;
};

// Output context for TJpgDec
struct JpegOutputContext {
    StripDecoder* decoder;
    DisplayDriver* driver;
    int strip_y_offset;
    uint16_t* line_buffer;  // Buffer for one line of pixels
    int buffer_width;
    int lcd_width;
    int lcd_height;
    bool output_bgr565;     // true=BGR565, false=RGB565
};

// TJpgDec uses a single opaque device pointer for the entire decode session.
// Both the input function and output function must be able to access their
// respective state through the same pointer.
struct JpegSessionContext {
    JpegInputContext input;
    JpegOutputContext output;
};

// TJpgDec input function - read from memory buffer
// Signature must match ROM: UINT (*)(JDEC*, BYTE*, UINT)
static UINT jpeg_input_func(JDEC* jd, BYTE* buff, UINT nbyte) {
    JpegSessionContext* session = (JpegSessionContext*)jd->device;
    if (!session) return 0;

    JpegInputContext* ctx = &session->input;
    if (!ctx->data) return 0;
    if (ctx->pos >= ctx->size) return 0;

    const size_t remaining = ctx->size - ctx->pos;
    const size_t requested = (size_t)nbyte;
    const size_t to_read = (requested < remaining) ? requested : remaining;

    if (buff && to_read > 0) {
        memcpy(buff, ctx->data + ctx->pos, to_read);
    }

    ctx->pos += to_read;
    return (UINT)to_read;
}

// TJpgDec output function - convert RGB888→(BGR565 or RGB565) and write to LCD
static UINT jpeg_output_func(JDEC* jd, void* bitmap, JRECT* rect) {
    JpegSessionContext* session = (JpegSessionContext*)jd->device;
    JpegOutputContext* ctx = session ? &session->output : nullptr;
    uint8_t* src = (uint8_t*)bitmap;
    
    if (!ctx || !ctx->line_buffer || !ctx->driver) {
        Logger.logMessage("StripDecoder", "ERROR: Invalid context or line_buffer or driver");
        return 0;
    }
    
    // Process each line in the MCU block
    for (int y = rect->top; y <= rect->bottom; y++) {
        int line_width = rect->right - rect->left + 1;
        
        // Bounds check
        if (line_width > ctx->buffer_width) {
            Logger.logMessagef("StripDecoder", "ERROR: line_width %d > buffer_width %d", line_width, ctx->buffer_width);
            return 0;
        }
        
        // Convert RGB888 to BGR565 or RGB565 for this line
        for (int x = 0; x < line_width; x++) {
            uint8_t r = *src++;
            uint8_t g = *src++;
            uint8_t b = *src++;

            if (ctx->output_bgr565) {
                // RGB888 → BGR565 conversion
                // BGR565: BBBB BGGG GGGR RRRR
                ctx->line_buffer[x] = ((b & 0xF8) << 8) |   // Blue in high bits
                                      ((g & 0xFC) << 3) |   // Green in middle
                                      (r >> 3);             // Red in low bits
            } else {
                // RGB888 → RGB565 conversion
                // RGB565: RRRR RGGG GGGB BBBB
                ctx->line_buffer[x] = ((r & 0xF8) << 8) |   // Red in high bits
                                      ((g & 0xFC) << 3) |   // Green in middle
                                      (b >> 3);             // Blue in low bits
            }
        }
        
        // Write line to LCD at correct position
        int lcd_x = rect->left;
        int lcd_y = ctx->strip_y_offset + y;
        
        // Bounds check for LCD coordinates
        if (lcd_x < 0 || lcd_y < 0 || lcd_x + line_width > ctx->lcd_width || lcd_y >= ctx->lcd_height) {
            Logger.logMessagef("StripDecoder", "ERROR: Invalid LCD coords: x=%d y=%d w=%d (LCD: %dx%d)", 
                              lcd_x, lcd_y, line_width, ctx->lcd_width, ctx->lcd_height);
            return 0;
        }
        
        // Push pixels to LCD via display driver
        ctx->driver->startWrite();
        ctx->driver->setAddrWindow(lcd_x, lcd_y, line_width, 1);
        ctx->driver->pushColors(ctx->line_buffer, line_width, true);
        ctx->driver->endWrite();
        
        // Yield periodically to prevent watchdog timeouts during long decodes.
        // Yielding every line can be unnecessarily expensive.
        if ((lcd_y & 0x03) == 0) {
            taskYIELD();
        }
    }
    
    return 1;  // Continue decoding
}

// TJpgDec work buffer size (recommended minimum)
static const size_t TJPGD_WORK_BUFFER_SIZE = 4096;

StripDecoder::StripDecoder() : driver(nullptr), width(0), height(0), lcd_width(0), lcd_height(0), current_y(0) {
}

StripDecoder::~StripDecoder() {
    end();
}

void StripDecoder::setDisplayDriver(DisplayDriver* drv) {
    driver = drv;
}

void StripDecoder::begin(int image_width, int image_height, int lcd_w, int lcd_h) {
    width = image_width;
    height = image_height;
    lcd_width = lcd_w;
    lcd_height = lcd_h;
    current_y = 0;
    
    Logger.logMessagef("StripDecoder", "Begin decode: %dx%d image on %dx%d LCD", width, height, lcd_width, lcd_height);
}

bool StripDecoder::decode_strip(const uint8_t* jpeg_data, size_t jpeg_size, int strip_index, bool output_bgr565) {
    if (!driver) {
        Logger.logMessage("StripDecoder", "ERROR: No display driver set");
        return false;
    }
    
    Logger.logBegin("Strip");
    
    // Allocate TJpgDec work area
    // TJpgDec requires: 3100 + (width * height * 2 / MCU_size) bytes
    // For 240x16: minimum ~3220 bytes, using 4096 for safety
    const size_t work_size = 4096;
    void* work = malloc(work_size);
    if (!work) {
        Logger.logEnd("ERROR: Failed to allocate work buffer");
        return false;
    }
    
    // Allocate line buffer for pixel conversion
    uint16_t* line_buffer = (uint16_t*)malloc(width * sizeof(uint16_t));
    if (!line_buffer) {
        free(work);
        Logger.logEnd("ERROR: Failed to allocate line buffer");
        return false;
    }
    
    JDEC jdec;
    JRESULT res;
    
    // Setup session context (shared between input and output callbacks)
    JpegSessionContext session_ctx;
    session_ctx.input.data = jpeg_data;
    session_ctx.input.size = jpeg_size;
    session_ctx.input.pos = 0;

    session_ctx.output.decoder = this;
    session_ctx.output.driver = driver;
    session_ctx.output.strip_y_offset = current_y;
    session_ctx.output.line_buffer = line_buffer;
    session_ctx.output.buffer_width = width;
    session_ctx.output.lcd_width = lcd_width;
    session_ctx.output.lcd_height = lcd_height;
    session_ctx.output.output_bgr565 = output_bgr565;
    
    // Prepare decoder
    res = jd_prepare(&jdec, jpeg_input_func, work, work_size, &session_ctx);
    
    if (res != JDR_OK) {
        Logger.logLinef("ERROR: jd_prepare failed: %d", res);
        Logger.logEnd();
        free(line_buffer);
        free(work);
        return false;
    }
    
    // Decompress and output to LCD
    res = jd_decomp(&jdec, jpeg_output_func, 0);  // 0 = 1:1 scale
    
    if (res != JDR_OK) {
        Logger.logLinef("ERROR: jd_decomp failed: %d", res);
        Logger.logEnd();
        free(line_buffer);
        free(work);
        return false;
    }
    
    // Move Y position for next strip
    current_y += jdec.height;
    
    // Cleanup
    free(line_buffer);
    free(work);
    
    return true;
}

void StripDecoder::end() {
    Logger.logMessagef("StripDecoder", "Complete at Y=%d", current_y);
    current_y = 0;
    width = 0;
    height = 0;
    lcd_width = 0;
    lcd_height = 0;
}

#endif // HAS_IMAGE_API
