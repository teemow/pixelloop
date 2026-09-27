#pragma once
#include <stdint.h>
// The PixelLoop OS user interface. Call with the LVGL lock held.
void ui_create(void);
const char *ui_active_screen_name(void);
// Test hook: show a fixed time (seconds since midnight) instead of uptime so
// screenshots are reproducible. Negative = follow uptime again.
void ui_clock_override(int32_t seconds);
