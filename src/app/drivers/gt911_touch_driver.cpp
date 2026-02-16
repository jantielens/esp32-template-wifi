#include "gt911_touch_driver.h"

#include "../board_config.h"
#include "../log_manager.h"

// GT911 register addresses
#define GT911_POINT_INFO  0x814E
#define GT911_POINT_1     0x814F

GT911_TouchDriver::GT911_TouchDriver()
		: addr(TOUCH_I2C_ADDR), rotation(0), calibrationEnabled(false),
			calXMin(0), calXMax(0), calYMin(0), calYMax(0),
			lastTouched(false), lastX(0), lastY(0) {}

void GT911_TouchDriver::init() {
		LOGI("GT911", "Initializing touch on Wire1 (SDA=%d, SCL=%d, ADDR=0x%02X)",
				TOUCH_I2C_SDA, TOUCH_I2C_SCL, TOUCH_I2C_ADDR);

		// Use Wire1 (I2C bus 1) to avoid ISR contention with Core 0.
		// The LVGL task polls touch from Core 1; Wire1.begin() here pins
		// its ISR to the current core, keeping I2C traffic off Core 0.
		Wire1.begin(TOUCH_I2C_SDA, TOUCH_I2C_SCL, 400000);

		// Skip hardware reset — RST and INT pins are not connected on this board.
		// The GT911 boots into normal mode by default when no reset sequence is applied.

		// Probe the controller to verify communication
		Wire1.beginTransmission(addr);
		uint8_t err = Wire1.endTransmission();
		if (err != 0) {
				// Try alternate address
				#ifdef TOUCH_I2C_ADDR_ALT
				LOGW("GT911", "Primary addr 0x%02X failed (err=%d), trying alt 0x%02X",
						addr, err, TOUCH_I2C_ADDR_ALT);
				addr = TOUCH_I2C_ADDR_ALT;
				Wire1.beginTransmission(addr);
				err = Wire1.endTransmission();
				if (err != 0) {
						LOGE("GT911", "Alt addr 0x%02X also failed (err=%d)", addr, err);
						return;
				}
				#else
				LOGE("GT911", "I2C probe failed (err=%d)", err);
				return;
				#endif
		}

		// Clear any pending touch data
		writeReg(GT911_POINT_INFO, 0);

		LOGI("GT911", "Touch initialized on Wire1 (%dx%d, addr=0x%02X)",
				DISPLAY_WIDTH, DISPLAY_HEIGHT, addr);
}

void GT911_TouchDriver::gt911Read() {
		uint8_t pointInfo = readReg(GT911_POINT_INFO);
		uint8_t bufferStatus = (pointInfo >> 7) & 1;
		uint8_t touches = pointInfo & 0x0F;

		// Only update state when the GT911 has completed a new scan.
		// When bufferStatus==0, no new data is available — keep the previous
		// touch state to avoid inserting a false RELEASED between scans.
		// The GT911 scans at ~60-140 Hz; the LVGL task can poll much faster,
		// so empty reads are expected while the finger is still down.
		if (bufferStatus == 0) {
				return;
		}

		lastTouched = (touches > 0);

		if (lastTouched) {
				// Read first touch point only (7 bytes: id, x_lo, x_hi, y_lo, y_hi, size_lo, size_hi)
				uint8_t data[7];
				readBlock(GT911_POINT_1, data, 7);
				lastX = data[1] | (data[2] << 8);
				lastY = data[3] | (data[4] << 8);
		}

		// Clear buffer status flag (must always be done after reading)
		writeReg(GT911_POINT_INFO, 0);
}

bool GT911_TouchDriver::isTouched() {
		// Return cached state from last gt911Read() — avoids redundant I2C polling
		// when getTouch() is called immediately after.
		return lastTouched;
}

bool GT911_TouchDriver::getTouch(uint16_t* x, uint16_t* y, uint16_t* pressure) {
		if (pressure) *pressure = 0;
		if (!x || !y) return false;

		gt911Read();
		if (!lastTouched) return false;

		uint16_t tx = lastX;
		uint16_t ty = lastY;

		// Apply calibration if configured
		if (calibrationEnabled && calXMax > calXMin && calYMax > calYMin) {
				uint32_t cx = tx;
				uint32_t cy = ty;
				if (cx < calXMin) cx = calXMin;
				if (cx > calXMax) cx = calXMax;
				if (cy < calYMin) cy = calYMin;
				if (cy > calYMax) cy = calYMax;

				tx = (uint16_t)((cx - calXMin) * (uint32_t)(DISPLAY_WIDTH - 1) / (uint32_t)(calXMax - calXMin));
				ty = (uint16_t)((cy - calYMin) * (uint32_t)(DISPLAY_HEIGHT - 1) / (uint32_t)(calYMax - calYMin));
		}

		applyRotation(tx, ty);

		*x = tx;
		*y = ty;
		return true;
}

void GT911_TouchDriver::setCalibration(uint16_t x_min, uint16_t x_max, uint16_t y_min, uint16_t y_max) {
		calibrationEnabled = true;
		calXMin = x_min;
		calXMax = x_max;
		calYMin = y_min;
		calYMax = y_max;
}

void GT911_TouchDriver::setRotation(uint8_t r) {
		rotation = r & 0x03;
}

// ============================================================================
// Low-level I2C (Wire1)
// ============================================================================

void GT911_TouchDriver::writeReg(uint16_t reg, uint8_t val) {
		Wire1.beginTransmission(addr);
		Wire1.write(highByte(reg));
		Wire1.write(lowByte(reg));
		Wire1.write(val);
		Wire1.endTransmission();
}

uint8_t GT911_TouchDriver::readReg(uint16_t reg) {
		Wire1.beginTransmission(addr);
		Wire1.write(highByte(reg));
		Wire1.write(lowByte(reg));
		Wire1.endTransmission();
		Wire1.requestFrom(addr, (uint8_t)1);
		return Wire1.read();
}

void GT911_TouchDriver::readBlock(uint16_t reg, uint8_t* buf, uint8_t len) {
		Wire1.beginTransmission(addr);
		Wire1.write(highByte(reg));
		Wire1.write(lowByte(reg));
		Wire1.endTransmission();
		Wire1.requestFrom(addr, len);
		for (uint8_t i = 0; i < len; i++) {
				buf[i] = Wire1.read();
		}
}

void GT911_TouchDriver::applyRotation(uint16_t& x, uint16_t& y) const {
		uint16_t w = DISPLAY_WIDTH;
		uint16_t h = DISPLAY_HEIGHT;

		switch (rotation) {
				case 0:
						return;
				case 1: {
						uint16_t nx = y;
						uint16_t ny = (uint16_t)(w - 1 - x);
						x = nx;
						y = ny;
						return;
				}
				case 2:
						x = (uint16_t)(w - 1 - x);
						y = (uint16_t)(h - 1 - y);
						return;
				case 3: {
						uint16_t nx = (uint16_t)(h - 1 - y);
						uint16_t ny = x;
						x = nx;
						y = ny;
						return;
				}
				default:
						return;
		}
}
