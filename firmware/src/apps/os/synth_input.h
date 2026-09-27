#pragma once
// A second LVGL pointer device fed by the console instead of a finger.
// `tap` and `swipe` from the host land here; the real touch indev keeps
// working alongside. All functions must be called with the LVGL lock held.
#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

lv_indev_t *synth_input_create(lv_display_t *disp);
bool synth_tap(int16_t x, int16_t y);
bool synth_swipe(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t duration_ms);
bool synth_idle(void);
