#include "synth_input.h"

#include <stdlib.h>

typedef struct {
    int16_t x, y;
    uint8_t pressed;
    uint16_t hold_ms;
} step_t;

#define QUEUE_LEN 512

static step_t s_q[QUEUE_LEN];
static volatile uint16_t s_head, s_tail;  // head: next to play, tail: next free
static uint32_t s_step_started;
static lv_point_t s_last = {0, 0};

static uint16_t q_count(void)
{
    return (uint16_t)(s_tail - s_head);
}

static bool q_push(int16_t x, int16_t y, uint8_t pressed, uint16_t hold_ms)
{
    if (q_count() >= QUEUE_LEN) {
        return false;
    }
    s_q[s_tail % QUEUE_LEN] = (step_t){x, y, pressed, hold_ms};
    s_tail++;
    return true;
}

static void read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    if (q_count() == 0) {
        data->point = s_last;
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    const step_t *st = &s_q[s_head % QUEUE_LEN];
    if (s_step_started == 0) {
        s_step_started = lv_tick_get();
        if (s_step_started == 0) {
            s_step_started = 1;
        }
    }
    s_last.x = st->x;
    s_last.y = st->y;
    data->point = s_last;
    data->state = st->pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    if (lv_tick_elaps(s_step_started) >= st->hold_ms) {
        s_head++;
        s_step_started = 0;
    }
    // Keep polling while steps are queued so long swipes stay smooth.
    data->continue_reading = false;
}

lv_indev_t *synth_input_create(lv_display_t *disp)
{
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, read_cb);
    lv_indev_set_display(indev, disp);
    return indev;
}

bool synth_idle(void)
{
    return q_count() == 0;
}

bool synth_tap(int16_t x, int16_t y)
{
    return q_push(x, y, 1, 90) && q_push(x, y, 0, 60);
}

bool synth_swipe(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t duration_ms)
{
    if (duration_ms < 40) {
        duration_ms = 40;
    }
    const uint32_t step_ms = 20;
    uint32_t steps = duration_ms / step_ms;
    if (steps < 2) {
        steps = 2;
    }
    if (!q_push(x1, y1, 1, 40)) {
        return false;
    }
    for (uint32_t i = 1; i <= steps; i++) {
        int16_t x = (int16_t)(x1 + ((int32_t)(x2 - x1) * (int32_t)i) / (int32_t)steps);
        int16_t y = (int16_t)(y1 + ((int32_t)(y2 - y1) * (int32_t)i) / (int32_t)steps);
        if (!q_push(x, y, 1, (uint16_t)step_ms)) {
            return false;
        }
    }
    return q_push(x2, y2, 1, 30) && q_push(x2, y2, 0, 60);
}
