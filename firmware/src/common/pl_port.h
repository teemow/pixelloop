#pragma once
// The port layer: everything the PixelLoop OS needs from the platform it runs
// on, and nothing else. The OS sources (ui, cmds, screenshot, synth_input,
// console) include only this header, lvgl.h and libc, so the same files build
// for the ESP32 (pl_port_esp.c) and for the desktop simulator (sim/).
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Microseconds since boot; the UI clock and `stats` uptime derive from it.
int64_t pl_uptime_us(void);

// The LVGL mutex. Recursive; held by the render task while it runs timers and
// by every console command that touches LVGL objects.
void pl_lvgl_lock(void);
void pl_lvgl_unlock(void);

// Large short-lived buffers (a full framebuffer for a screenshot). The device
// serves these from PSRAM, the host from the heap.
void *pl_alloc_large(size_t bytes);
void pl_free_large(void *p);

// Codecs the screenshot transfer relies on. CRC-32 is the zlib polynomial
// (what tools/serial_ctl.py verifies with zlib.crc32). base64 returns the
// number of characters written (no terminator), 0 when dst is too small.
uint32_t pl_crc32(const void *data, size_t len);
size_t pl_base64_encode(char *dst, size_t dst_cap, const uint8_t *src, size_t len);

// Memory figures for `stats`. A platform that cannot report a field leaves it 0.
typedef struct {
    size_t heap_free;
    size_t heap_min;
    size_t psram_free;
} pl_mem_stats_t;
void pl_mem_stats(pl_mem_stats_t *out);

// The console byte stream (stdin/stdout): make it usable before the first
// printf. Restart the firmware / process.
void pl_console_setup(void);
void pl_restart(void);

// Log one line. Level is 'E', 'W' or 'I'; the platform decides where it goes
// (the ESP log on the device, stdout in the simulator).
void pl_log(char level, const char *tag, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
#define PL_LOGE(tag, ...) pl_log('E', tag, __VA_ARGS__)
#define PL_LOGW(tag, ...) pl_log('W', tag, __VA_ARGS__)
#define PL_LOGI(tag, ...) pl_log('I', tag, __VA_ARGS__)
