#pragma once
// Dump the active LVGL screen over the console as RLE-compressed RGB565,
// base64 framed by "SHOT ..." / "SHOT-END" lines (see AGENTS.md).
// Must be called with the LVGL lock held. Returns 0 on success, otherwise a
// negative value after logging why.
int screenshot_send(void);
