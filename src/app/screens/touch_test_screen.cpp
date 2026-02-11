#include "touch_test_screen.h"

#if HAS_TOUCH

#include "../board_config.h"
#include "../log_manager.h"

#include <stdlib.h>  // abs()

TouchTestScreen::TouchTestScreen()
		: screen(nullptr), canvas(nullptr), canvasBuf(nullptr),
			headerLabel(nullptr),
			prevTouchValid(false), prevX(0), prevY(0), brushRadius(3) {}

TouchTestScreen::~TouchTestScreen() {
		destroy();
}

void TouchTestScreen::create() {
		if (screen) return;

		LOGI("TouchTest", "Create start");

		// Adaptive brush size: ~0.8% of the smaller display dimension, clamped 2-6px
		uint16_t minDim = DISPLAY_WIDTH < DISPLAY_HEIGHT ? DISPLAY_WIDTH : DISPLAY_HEIGHT;
		brushRadius = (uint8_t)(minDim * 8 / 1000);
		if (brushRadius < 2) brushRadius = 2;
		if (brushRadius > 6) brushRadius = 6;

		// Create the LVGL screen object (always lightweight)
		screen = lv_obj_create(NULL);
		lv_obj_set_style_bg_color(screen, lv_color_black(), 0);

		// Header label — small, semi-transparent, top-center
		headerLabel = lv_label_create(screen);
		char header[48];
		snprintf(header, sizeof(header), "Touch Test  %dx%d", DISPLAY_WIDTH, DISPLAY_HEIGHT);
		lv_label_set_text(headerLabel, header);
		lv_obj_set_style_text_color(headerLabel, lv_color_make(80, 80, 80), 0);
		lv_obj_set_style_text_font(headerLabel, &lv_font_montserrat_14, 0);
		lv_obj_align(headerLabel, LV_ALIGN_TOP_MID, 0, 4);
		lv_obj_clear_flag(headerLabel, LV_OBJ_FLAG_CLICKABLE);

		// Canvas is NOT allocated here — deferred to show() to save PSRAM.

		LOGI("TouchTest", "Create complete (brush r=%d)", brushRadius);
}

void TouchTestScreen::destroy() {
		if (canvas) {
				lv_obj_del(canvas);
				canvas = nullptr;
		}
		if (canvasBuf) {
				heap_caps_free(canvasBuf);
				canvasBuf = nullptr;
		}
		if (screen) {
				lv_obj_del(screen);
				screen = nullptr;
				headerLabel = nullptr;
		}
}

void TouchTestScreen::show() {
		if (!screen) return;

		// Allocate canvas buffer in PSRAM (only while this screen is active)
		if (!canvasBuf) {
				size_t bufSize = (size_t)DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(lv_color_t);
				canvasBuf = (lv_color_t*)heap_caps_malloc(bufSize, MALLOC_CAP_SPIRAM);
				if (!canvasBuf) {
						LOGE("TouchTest", "Failed to allocate canvas (%u bytes PSRAM)", (unsigned)bufSize);
						// Show screen anyway (just won't draw)
						lv_scr_load(screen);
						return;
				}
				LOGI("TouchTest", "Canvas buffer allocated: %u KB PSRAM", (unsigned)(bufSize / 1024));
		}

		// Create or re-create the canvas widget
		if (canvas) {
				lv_obj_del(canvas);
				canvas = nullptr;
		}

		canvas = lv_canvas_create(screen);
		lv_canvas_set_buffer(canvas, canvasBuf, DISPLAY_WIDTH, DISPLAY_HEIGHT, LV_IMG_CF_TRUE_COLOR);
		lv_obj_align(canvas, LV_ALIGN_CENTER, 0, 0);

		// Clear canvas to black
		lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_COVER);

		// Make canvas receive touch events
		lv_obj_add_flag(canvas, LV_OBJ_FLAG_CLICKABLE);
		lv_obj_add_event_cb(canvas, touchEventCallback, LV_EVENT_PRESSING, this);
		lv_obj_add_event_cb(canvas, touchEventCallback, LV_EVENT_RELEASED, this);

		// Move header label to front (above canvas)
		lv_obj_move_foreground(headerLabel);

		// Reset touch tracking
		prevTouchValid = false;

		lv_scr_load(screen);
		LOGI("TouchTest", "Screen shown");
}

void TouchTestScreen::hide() {
		prevTouchValid = false;

		// Free canvas and PSRAM buffer when leaving (zero cost while inactive)
		if (canvas) {
				lv_obj_del(canvas);
				canvas = nullptr;
		}
		if (canvasBuf) {
				heap_caps_free(canvasBuf);
				canvasBuf = nullptr;
				LOGI("TouchTest", "Canvas buffer freed");
		}
}

void TouchTestScreen::update() {
		// No periodic updates needed — drawing happens in touch event callback
}

// ============================================================================
// Drawing helpers
// ============================================================================

void TouchTestScreen::drawDot(int16_t cx, int16_t cy, lv_color_t color, uint8_t radius) {
		if (!canvas) return;

		lv_draw_rect_dsc_t rect_dsc;
		lv_draw_rect_dsc_init(&rect_dsc);
		rect_dsc.bg_color = color;
		rect_dsc.bg_opa = LV_OPA_COVER;
		rect_dsc.radius = radius;
		rect_dsc.border_width = 0;

		int16_t x1 = cx - radius;
		int16_t y1 = cy - radius;
		int16_t x2 = cx + radius;
		int16_t y2 = cy + radius;

		// Clamp to canvas bounds
		if (x1 < 0) x1 = 0;
		if (y1 < 0) y1 = 0;
		if (x2 >= DISPLAY_WIDTH) x2 = DISPLAY_WIDTH - 1;
		if (y2 >= DISPLAY_HEIGHT) y2 = DISPLAY_HEIGHT - 1;

		lv_canvas_draw_rect(canvas, x1, y1, x2 - x1 + 1, y2 - y1 + 1, &rect_dsc);
}

void TouchTestScreen::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
		// Interpolate white dots between two touch points so fast movements don't leave gaps.
		// Uses a thinner brush so the red touch-point dots remain visible on top.
		uint8_t lineRadius = (brushRadius > 1) ? brushRadius - 1 : 1;

		int16_t dx = x1 - x0;
		int16_t dy = y1 - y0;
		int16_t dist = (int16_t)sqrt((float)(dx * dx + dy * dy));

		int16_t step = lineRadius;  // step = radius for nice overlap
		if (step < 1) step = 1;

		if (dist > step) {
				int16_t steps = dist / step;
				for (int16_t i = 1; i < steps; i++) {  // skip endpoints (dots drawn separately)
						int16_t x = x0 + (int16_t)((int32_t)dx * i / steps);
						int16_t y = y0 + (int16_t)((int32_t)dy * i / steps);
						drawDot(x, y, lv_color_white(), lineRadius);
				}
		}
}

// ============================================================================
// Touch event callback
// ============================================================================

void TouchTestScreen::touchEventCallback(lv_event_t* e) {
		TouchTestScreen* self = (TouchTestScreen*)lv_event_get_user_data(e);
		if (!self || !self->canvas) return;

		lv_event_code_t code = lv_event_get_code(e);

		if (code == LV_EVENT_RELEASED) {
				self->prevTouchValid = false;
				return;
		}

		// LV_EVENT_PRESSING — finger is down and moving
		lv_indev_t* indev = lv_indev_get_act();
		if (!indev) return;

		lv_point_t point;
		lv_indev_get_point(indev, &point);

		int16_t x = point.x;
		int16_t y = point.y;

		if (self->prevTouchValid) {
				// 1. White connecting line (thinner, drawn first)
				self->drawLine(self->prevX, self->prevY, x, y);
				// 2. Re-draw previous red dot on top (line may have partially covered it)
				self->drawDot(self->prevX, self->prevY, lv_color_hex(0xFF0000), self->brushRadius);
		}

		// 3. Red dot at current touch point (always on top)
		self->drawDot(x, y, lv_color_hex(0xFF0000), self->brushRadius);

		self->prevX = x;
		self->prevY = y;
		self->prevTouchValid = true;
}

#endif // HAS_TOUCH
