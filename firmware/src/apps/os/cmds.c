// Console commands that need LVGL: shot, tap, swipe, stats, clock.
#include "cmds.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "console.h"
#include "lvgl.h"
#include "pixelloop.h"
#include "pl_port.h"
#include "screenshot.h"
#include "synth_input.h"
#include "ui.h"

static void cmd_shot(int argc, char **argv)
{
    pl_lvgl_lock();
    int rc = screenshot_send();
    pl_lvgl_unlock();
    if (rc == 0) {
        console_ok();
    } else {
        console_err("screenshot failed (see log)");
    }
}

static void cmd_tap(int argc, char **argv)
{
    if (argc != 3) {
        console_err("usage: tap <x> <y>");
        return;
    }
    pl_lvgl_lock();
    bool ok = synth_tap((int16_t)atoi(argv[1]), (int16_t)atoi(argv[2]));
    pl_lvgl_unlock();
    ok ? console_ok() : console_err("input queue full");
}

static void cmd_swipe(int argc, char **argv)
{
    if (argc < 5) {
        console_err("usage: swipe <x1> <y1> <x2> <y2> [ms]");
        return;
    }
    uint32_t ms = argc > 5 ? (uint32_t)atoi(argv[5]) : 250;
    pl_lvgl_lock();
    bool ok = synth_swipe((int16_t)atoi(argv[1]), (int16_t)atoi(argv[2]), (int16_t)atoi(argv[3]),
                          (int16_t)atoi(argv[4]), ms);
    pl_lvgl_unlock();
    ok ? console_ok() : console_err("input queue full");
}

static void cmd_stats(int argc, char **argv)
{
    pl_lvgl_lock();
    lv_display_t *d = lv_display_get_default();
    int32_t w = lv_display_get_horizontal_resolution(d);
    int32_t h = lv_display_get_vertical_resolution(d);
    const char *screen = ui_active_screen_name();
    bool idle = synth_idle();
    pl_lvgl_unlock();
    pl_mem_stats_t mem;
    pl_mem_stats(&mem);
    printf("STATS {\"uptime_ms\":%llu,\"heap_free\":%u,\"heap_min\":%u,\"psram_free\":%u,"
           "\"w\":%ld,\"h\":%ld,\"screen\":\"%s\",\"input_idle\":%s,\"board\":\"%s\",\"version\":\"%s\"}\n",
           (unsigned long long)(pl_uptime_us() / 1000), (unsigned)mem.heap_free, (unsigned)mem.heap_min,
           (unsigned)mem.psram_free, (long)w, (long)h, screen, idle ? "true" : "false", PIXELLOOP_BOARD_NAME,
           PIXELLOOP_VERSION);
    console_ok();
}

static void cmd_clock(int argc, char **argv)
{
    if (argc != 2) {
        console_err("usage: clock <seconds-since-midnight>|run");
        return;
    }
    int32_t v = strcmp(argv[1], "run") == 0 ? -1 : atoi(argv[1]);
    pl_lvgl_lock();
    ui_clock_override(v);
    pl_lvgl_unlock();
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
