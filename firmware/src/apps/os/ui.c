#include "ui.h"

#include <stdio.h>

#include "lvgl.h"
#include "pixelloop.h"
#include "pl_port.h"

static lv_obj_t *s_clock;
static lv_obj_t *s_counter;
static int s_count;
static int32_t s_clock_override = -1;

static void tick_cb(lv_timer_t *t)
{
    uint32_t s = s_clock_override >= 0 ? (uint32_t)s_clock_override : (uint32_t)(pl_uptime_us() / 1000000);
    lv_label_set_text_fmt(s_clock, "%02lu:%02lu:%02lu", (unsigned long)(s / 3600), (unsigned long)((s / 60) % 60),
                          (unsigned long)(s % 60));
}

static void counter_cb(lv_event_t *e)
{
    s_count++;
    lv_label_set_text_fmt(s_counter, "%d", s_count);
}

const char *ui_active_screen_name(void)
{
    return "home";
}

void ui_clock_override(int32_t seconds)
{
    s_clock_override = seconds;
    if (s_clock) {
        tick_cb(NULL);
    }
}

void ui_create(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0b0f1a), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_display_t *disp = lv_display_get_default();
    const int32_t w = lv_display_get_horizontal_resolution(disp);
    const int32_t h = lv_display_get_vertical_resolution(disp);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "PixelLoop");
    lv_obj_set_style_text_color(title, lv_color_hex(0x7dd3fc), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, h / 12);

    s_clock = lv_label_create(scr);
    lv_obj_set_style_text_color(s_clock, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_clock, &lv_font_montserrat_48, 0);
    lv_obj_align(s_clock, LV_ALIGN_TOP_MID, 0, h / 12 + 44);
    tick_cb(NULL);
    lv_timer_create(tick_cb, 1000, NULL);

    lv_obj_t *btn = lv_button_create(scr);
    lv_obj_set_size(btn, w * 3 / 5, 56);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, h / 8);
    lv_obj_set_style_radius(btn, 28, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2563eb), 0);
    lv_obj_add_event_cb(btn, counter_cb, LV_EVENT_CLICKED, NULL);
    s_counter = lv_label_create(btn);
    lv_label_set_text(s_counter, "0");
    lv_obj_set_style_text_font(s_counter, &lv_font_montserrat_28, 0);
    lv_obj_center(s_counter);

    lv_obj_t *hint = lv_label_create(scr);
    lv_label_set_text(hint, "tap to count");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x94a3b8), 0);
    lv_obj_align_to(hint, btn, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    lv_obj_t *foot = lv_label_create(scr);
    lv_label_set_text_fmt(foot, "%s  %ldx%ld  v%s", PIXELLOOP_BOARD_NAME, (long)w, (long)h, PIXELLOOP_VERSION);
    lv_obj_set_style_text_color(foot, lv_color_hex(0x475569), 0);
    lv_obj_set_style_text_font(foot, &lv_font_montserrat_14, 0);
    lv_obj_align(foot, LV_ALIGN_BOTTOM_MID, 0, -8);
}
