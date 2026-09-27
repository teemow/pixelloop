#pragma once
// Dump the active LVGL screen over the console as RLE-compressed RGB565,
// base64 framed by "SHOT ..." / "SHOT-END" lines (see AGENTS.md).
// Must be called with the LVGL lock held.
#include "esp_err.h"
esp_err_t screenshot_send(void);
