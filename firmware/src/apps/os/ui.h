#pragma once
// The PixelLoop OS user interface. Call with the LVGL lock held.
void ui_create(void);
// Called once a second from the LVGL task via a timer; kept public for tests.
const char *ui_active_screen_name(void);
