#pragma once
// What every board directory under boards/ must implement for the OS app.
// The probe app does not use this; it runs on the "generic" board.
#include <stdint.h>
#include "esp_err.h"
#include "lvgl.h"

// Power rails, buses, IO expander, backlight PWM. Called once, first.
esp_err_t board_init(void);
// Panel + LVGL display (through esp_lvgl_port). Never returns NULL on success.
lv_display_t *board_display_create(void);
// Touch controller as an LVGL pointer indev; NULL when the board has none.
lv_indev_t *board_touch_create(lv_display_t *disp);
// 0..100 percent.
void board_backlight_set(uint8_t percent);
