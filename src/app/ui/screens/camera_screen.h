/*
 * Camera Screen
 * 
 * Displays camera images from Home Assistant person detection.
 * Downloads JPEG from URL and displays full-screen with center-crop scaling.
 * Shows loading indicator during download, error message on failure.
 * 
 * USAGE:
 *   camera_screen.loadImageFromUrl("http://homeassistant.local:8123/...");
 *   screen_manager.navigate(SCREEN_CAMERA);
 */

#ifndef CAMERA_SCREEN_H
#define CAMERA_SCREEN_H

#include "../base_screen.h"
#include <Arduino.h>

class CameraScreen : public BaseScreen {
 public:
  CameraScreen();
  virtual ~CameraScreen();
  
  lv_obj_t* root() override;
  void onEnter() override;
  void onExit() override;
  void handle(const UiEvent &evt) override;
  
  // Load and display image from URL
  // Returns true if download started successfully
  bool loadImageFromUrl(const char* url);
  
  // Clear current image and free memory
  void clearImage();
  
 private:
  void buildUI();
  void showLoading();
  void showError(const char* message);
  void displayImage(const uint8_t* jpeg_data, size_t size);
  
  lv_obj_t* screen_;
  lv_obj_t* img_widget_;
  lv_obj_t* loading_label_;
  lv_obj_t* error_label_;
  
  uint8_t* image_buffer_;
  size_t image_buffer_size_;
  lv_img_dsc_t img_dsc_;
  
  bool is_built_;
};

#endif // CAMERA_SCREEN_H
