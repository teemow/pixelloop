// Console commands that need LVGL: shot, tap, swipe, stats.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "console.h"
#include "esp_heap_caps.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "pixelloop.h"
#include "screenshot.h"
#include "synth_input.h"
#include "ui.h"

static void cmd_shot(int argc, char **argv)
{
    lvgl_port_lock(0);
    esp_err_t err = screenshot_send();
    lvgl_port_unlock();
    if (err == ESP_OK) {
        console_ok();
    } else {
        console_err("screenshot: %s", esp_err_to_name(err));
    }
}

static void cmd_tap(int argc, char **argv)
{
    if (argc != 3) {
        console_err("usage: tap <x> <y>");
        return;
    }
    lvgl_port_lock(0);
    bool ok = synth_tap((int16_t)atoi(argv[1]), (int16_t)atoi(argv[2]));
    lvgl_port_unlock();
    ok ? console_ok() : console_err("input queue full");
}

static void cmd_swipe(int argc, char **argv)
{
    if (argc < 5) {
        console_err("usage: swipe <x1> <y1> <x2> <y2> [ms]");
        return;
    }
    uint32_t ms = argc > 5 ? (uint32_t)atoi(argv[5]) : 250;
    lvgl_port_lock(0);
    bool ok = synth_swipe((int16_t)atoi(argv[1]), (int16_t)atoi(argv[2]), (int16_t)atoi(argv[3]),
                          (int16_t)atoi(argv[4]), ms);
    lvgl_port_unlock();
    ok ? console_ok() : console_err("input queue full");
}

static void cmd_stats(int argc, char **argv)
{
    lvgl_port_lock(0);
    lv_display_t *d = lv_display_get_default();
    int32_t w = lv_display_get_horizontal_resolution(d);
    int32_t h = lv_display_get_vertical_resolution(d);
    const char *screen = ui_active_screen_name();
    bool idle = synth_idle();
    lvgl_port_unlock();
    printf("STATS {\"uptime_ms\":%llu,\"heap_free\":%u,\"heap_min\":%u,\"psram_free\":%u,"
           "\"w\":%ld,\"h\":%ld,\"screen\":\"%s\",\"input_idle\":%s,\"board\":\"%s\",\"version\":\"%s\"}\n",
           (unsigned long long)(esp_timer_get_time() / 1000), (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM), (long)w, (long)h, screen, idle ? "true" : "false",
           PIXELLOOP_BOARD_NAME, PIXELLOOP_VERSION);
    console_ok();
}

static void cmd_clock(int argc, char **argv)
{
    if (argc != 2) {
        console_err("usage: clock <seconds-since-midnight>|run");
        return;
    }
    int32_t v = strcmp(argv[1], "run") == 0 ? -1 : atoi(argv[1]);
    lvgl_port_lock(0);
    ui_clock_override(v);
    lvgl_port_unlock();
    console_ok();
}

void os_register_commands(void)
{
    console_register("clock", cmd_clock, "clock <seconds>|run - freeze the clock for reproducible shots");
    console_register("shot", cmd_shot, "dump the framebuffer (SHOT ... SHOT-END)");
    console_register("tap", cmd_tap, "tap <x> <y>");
    console_register("swipe", cmd_swipe, "swipe <x1> <y1> <x2> <y2> [ms]");
    console_register("stats", cmd_stats, "STATS {json}");
}
