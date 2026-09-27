#include "screenshot.h"

#include <stdio.h>
#include <string.h>

#include "lvgl.h"
#include "pl_port.h"

static const char *TAG = "shot";

// Run-length encode a stream of RGB565 pixels: [count u8][pixel u16 le]...
static size_t rle_encode(const uint8_t *px, size_t npx, uint8_t *out, size_t out_cap)
{
    size_t o = 0;
    size_t i = 0;
    while (i < npx) {
        uint8_t lo = px[2 * i], hi = px[2 * i + 1];
        size_t run = 1;
        while (i + run < npx && run < 255 && px[2 * (i + run)] == lo && px[2 * (i + run) + 1] == hi) {
            run++;
        }
        if (o + 3 > out_cap) {
            return 0;
        }
        out[o++] = (uint8_t)run;
        out[o++] = lo;
        out[o++] = hi;
        i += run;
    }
    return o;
}

int screenshot_send(void)
{
    lv_display_t *disp = lv_display_get_default();
    lv_obj_t *scr = lv_screen_active();
    if (!disp || !scr) {
        PL_LOGE(TAG, "no display");
        return -1;
    }
    const int32_t w = lv_display_get_horizontal_resolution(disp);
    const int32_t h = lv_display_get_vertical_resolution(disp);
    const uint32_t stride = lv_draw_buf_width_to_stride(w, LV_COLOR_FORMAT_RGB565);
    const size_t buf_size = (size_t)stride * (size_t)h;

    uint8_t *pixels = pl_alloc_large(buf_size);
    if (!pixels) {
        PL_LOGE(TAG, "no memory for %u byte framebuffer", (unsigned)buf_size);
        return -1;
    }
    lv_draw_buf_t db;
    lv_draw_buf_init(&db, w, h, LV_COLOR_FORMAT_RGB565, stride, pixels, buf_size);
    lv_result_t res = lv_snapshot_take_to_draw_buf(scr, LV_COLOR_FORMAT_RGB565, &db);
    if (res != LV_RESULT_OK) {
        pl_free_large(pixels);
        PL_LOGE(TAG, "snapshot failed");
        return -1;
    }

    // Pack rows tightly (the draw buffer stride may be padded), then RLE.
    const size_t raw_len = (size_t)w * (size_t)h * 2;
    uint8_t *raw = pixels;
    if (stride != (uint32_t)w * 2) {
        for (int32_t y = 0; y < h; y++) {
            memmove(raw + (size_t)y * w * 2, pixels + (size_t)y * stride, (size_t)w * 2);
        }
    }
    const uint32_t crc = pl_crc32(raw, raw_len);

    const size_t rle_cap = raw_len / 2 * 3 + 16;
    uint8_t *rle = pl_alloc_large(rle_cap);
    if (!rle) {
        pl_free_large(pixels);
        PL_LOGE(TAG, "no memory for %u byte rle buffer", (unsigned)rle_cap);
        return -1;
    }
    const size_t rle_len = rle_encode(raw, raw_len / 2, rle, rle_cap);
    pl_free_large(pixels);
    if (rle_len == 0) {
        pl_free_large(rle);
        PL_LOGE(TAG, "rle overflow");
        return -1;
    }

    printf("SHOT %ld %ld rgb565rle %u %u %08lx\n", (long)w, (long)h, (unsigned)raw_len, (unsigned)rle_len,
           (unsigned long)crc);
    // 57 input bytes -> 76 base64 chars per line.
    char line[80];
    for (size_t off = 0; off < rle_len; off += 57) {
        size_t chunk = rle_len - off < 57 ? rle_len - off : 57;
        size_t olen = pl_base64_encode(line, sizeof(line) - 1, rle + off, chunk);
        line[olen] = '\0';
        puts(line);
    }
    printf("SHOT-END\n");
    fflush(stdout);
    pl_free_large(rle);
    return 0;
}
