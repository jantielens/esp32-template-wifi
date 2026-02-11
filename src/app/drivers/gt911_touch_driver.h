#ifndef GT911_TOUCH_DRIVER_H
#define GT911_TOUCH_DRIVER_H

#include "../touch_driver.h"

#include <Wire.h>

// Minimal GT911 capacitive touch driver using Wire1 (I2C bus 1).
//
// Uses Wire1 to avoid ISR contention with WiFi on Core 0.
// When Wire (bus 0) is initialized from Core 0, its ISR is pinned there.
// LVGL polls touch from Core 1, so using bus 0 from Core 1 causes
// interrupt watchdog timeouts when WiFi is active on Core 0.

class GT911_TouchDriver : public TouchDriver {
public:
		GT911_TouchDriver();
		~GT911_TouchDriver() override = default;

		void init() override;

		bool isTouched() override;
		bool getTouch(uint16_t* x, uint16_t* y, uint16_t* pressure = nullptr) override;

		void setCalibration(uint16_t x_min, uint16_t x_max, uint16_t y_min, uint16_t y_max) override;
		void setRotation(uint8_t rotation) override;

private:
		uint8_t addr;
		uint8_t rotation;

		bool calibrationEnabled;
		uint16_t calXMin;
		uint16_t calXMax;
		uint16_t calYMin;
		uint16_t calYMax;

		// Cached state from last read()
		bool lastTouched;
		uint16_t lastX;
		uint16_t lastY;

		// Low-level GT911 I2C operations (Wire1)
		void gt911Read();
		void writeReg(uint16_t reg, uint8_t val);
		uint8_t readReg(uint16_t reg);
		void readBlock(uint16_t reg, uint8_t* buf, uint8_t len);

		void applyRotation(uint16_t& x, uint16_t& y) const;
};

#endif // GT911_TOUCH_DRIVER_H
