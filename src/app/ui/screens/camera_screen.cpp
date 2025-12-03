/*
 * Camera Screen Implementation
 * 
 * Downloads JPEG images via HTTP and displays them full-screen.
 * Uses PSRAM for image storage and LVGL's JPEG decoder.
 */

#include "camera_screen.h"
#include "../../log_manager.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

#define MAX_IMAGE_SIZE (1024 * 1024) // 1MB safety limit
#define HTTP_TIMEOUT_MS 10000         // 10 second timeout

CameraScreen::CameraScreen() 
    : screen_(nullptr),
      img_widget_(nullptr),
      loading_label_(nullptr),
      error_label_(nullptr),
      image_buffer_(nullptr),
      image_buffer_size_(0),
      is_built_(false) {
    memset(&img_dsc_, 0, sizeof(img_dsc_));
}

CameraScreen::~CameraScreen() {
    clearImage();
    if (screen_) {
        lv_obj_del(screen_);
        screen_ = nullptr;
    }
}

lv_obj_t* CameraScreen::root() {
    if (!is_built_) {
        buildUI();
        is_built_ = true;
    }
    return screen_;
}

void CameraScreen::buildUI() {
    // Create screen
    screen_ = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen_, lv_color_black(), 0);
    
    // Create image widget (360x360 full-screen)
    img_widget_ = lv_img_create(screen_);
    lv_obj_center(img_widget_);
    lv_obj_add_flag(img_widget_, LV_OBJ_FLAG_HIDDEN); // Hidden until image loaded
    
    // Create loading label
    loading_label_ = lv_label_create(screen_);
    lv_label_set_text(loading_label_, "Loading...");
    lv_obj_set_style_text_color(loading_label_, lv_color_white(), 0);
    lv_obj_set_style_text_font(loading_label_, &lv_font_montserrat_22, 0);
    lv_obj_center(loading_label_);
    lv_obj_add_flag(loading_label_, LV_OBJ_FLAG_HIDDEN);
    
    // Create error label
    error_label_ = lv_label_create(screen_);
    lv_label_set_text(error_label_, "Error loading image");
    lv_obj_set_style_text_color(error_label_, lv_color_make(255, 80, 80), 0);
    lv_obj_set_style_text_font(error_label_, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(error_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(error_label_, 300);
    lv_label_set_long_mode(error_label_, LV_LABEL_LONG_WRAP);
    lv_obj_center(error_label_);
    lv_obj_add_flag(error_label_, LV_OBJ_FLAG_HIDDEN);
}

void CameraScreen::onEnter() {
    Logger.logMessage("CameraScreen", "Entered");
}

void CameraScreen::onExit() {
    Logger.logMessage("CameraScreen", "Exited");
}

void CameraScreen::handle(const UiEvent &evt) {
    // Handle navigation events if needed
}

void CameraScreen::showLoading() {
    lv_obj_add_flag(img_widget_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(error_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(loading_label_, LV_OBJ_FLAG_HIDDEN);
}

void CameraScreen::showError(const char* message) {
    lv_obj_add_flag(img_widget_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(loading_label_, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(error_label_, message);
    lv_obj_clear_flag(error_label_, LV_OBJ_FLAG_HIDDEN);
}

void CameraScreen::clearImage() {
    if (image_buffer_) {
        heap_caps_free(image_buffer_);
        image_buffer_ = nullptr;
        image_buffer_size_ = 0;
    }
    
    if (img_widget_) {
        lv_obj_add_flag(img_widget_, LV_OBJ_FLAG_HIDDEN);
    }
}

bool CameraScreen::loadImageFromUrl(const char* url) {
    if (!url || strlen(url) == 0) {
        Logger.logMessage("CameraScreen", "Invalid URL");
        showError("Invalid URL");
        return false;
    }
    
    Logger.logBegin("Camera Image Download");
    Logger.logLinef("URL: %s", url);
    
    // Clear previous image
    clearImage();
    showLoading();
    
    // Initialize HTTP client
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    
    if (!http.begin(url)) {
        Logger.logEnd("HTTP begin failed");
        showError("Failed to connect");
        return false;
    }
    
    // Send GET request
    int httpCode = http.GET();
    
    if (httpCode != HTTP_CODE_OK) {
        Logger.logLinef("HTTP error: %d", httpCode);
        Logger.logEnd();
        http.end();
        
        char error_msg[64];
        snprintf(error_msg, sizeof(error_msg), "HTTP error: %d", httpCode);
        showError(error_msg);
        return false;
    }
    
    // Get content length
    int content_length = http.getSize();
    Logger.logLinef("Content-Length: %d bytes", content_length);
    
    if (content_length <= 0) {
        Logger.logEnd("Invalid content length");
        http.end();
        showError("Invalid response");
        return false;
    }
    
    if (content_length > MAX_IMAGE_SIZE) {
        Logger.logLinef("Image too large: %d bytes (max %d)", content_length, MAX_IMAGE_SIZE);
        Logger.logEnd();
        http.end();
        showError("Image too large (>1MB)");
        return false;
    }
    
    // Allocate PSRAM buffer for image
    image_buffer_ = (uint8_t*)heap_caps_malloc(content_length, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!image_buffer_) {
        // Fallback to internal RAM if PSRAM fails
        Logger.logLine("PSRAM allocation failed, trying internal RAM");
        image_buffer_ = (uint8_t*)malloc(content_length);
        if (!image_buffer_) {
            Logger.logEnd("Memory allocation failed");
            http.end();
            showError("Out of memory");
            return false;
        }
    }
    
    image_buffer_size_ = content_length;
    
    // Download image data
    WiFiClient* stream = http.getStreamPtr();
    size_t bytes_read = 0;
    unsigned long start_time = millis();
    
    while (http.connected() && bytes_read < content_length) {
        size_t available = stream->available();
        if (available > 0) {
            size_t to_read = min(available, content_length - bytes_read);
            size_t read = stream->readBytes(image_buffer_ + bytes_read, to_read);
            bytes_read += read;
        }
        
        // Timeout check
        if (millis() - start_time > HTTP_TIMEOUT_MS) {
            Logger.logEnd("Download timeout");
            http.end();
            clearImage();
            showError("Download timeout");
            return false;
        }
        
        delay(1); // Yield to other tasks
    }
    
    http.end();
    
    if (bytes_read != content_length) {
        Logger.logLinef("Incomplete download: %d/%d bytes", bytes_read, content_length);
        Logger.logEnd();
        clearImage();
        showError("Incomplete download");
        return false;
    }
    
    unsigned long download_time = millis() - start_time;
    Logger.logLinef("Downloaded: %d bytes in %lu ms", bytes_read, download_time);
    Logger.logEnd();
    
    // Display the image
    displayImage(image_buffer_, image_buffer_size_);
    
    return true;
}

void CameraScreen::displayImage(const uint8_t* jpeg_data, size_t size) {
    Logger.logBegin("Display Image");
    Logger.logLinef("Size: %d bytes", size);
    
    // Hide loading/error messages
    lv_obj_add_flag(loading_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(error_label_, LV_OBJ_FLAG_HIDDEN);
    
    // Setup LVGL image descriptor for JPEG
    img_dsc_.header.always_zero = 0;
    img_dsc_.header.w = 0; // Auto-detect from JPEG
    img_dsc_.header.h = 0; // Auto-detect from JPEG
    img_dsc_.header.cf = LV_IMG_CF_RAW;
    img_dsc_.data_size = size;
    img_dsc_.data = jpeg_data;
    
    // Set image source
    lv_img_set_src(img_widget_, &img_dsc_);
    
    // Apply zoom to fill 360x360 display (center-crop)
    // LVGL will scale the image to fit while maintaining aspect ratio
    lv_obj_set_size(img_widget_, 360, 360);
    lv_img_set_zoom(img_widget_, 256); // 256 = 100% (no zoom)
    lv_obj_center(img_widget_);
    
    // Show image
    lv_obj_clear_flag(img_widget_, LV_OBJ_FLAG_HIDDEN);
    
    Logger.logEnd();
}
