// PixelLoop OS entry point: board bring-up, LVGL, UI, console.
#include <stdio.h>

#include "board_api.h"
#include "cmds.h"
#include "console.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "nvs_flash.h"
#include "pixelloop.h"
#include "pl_port.h"
#include "synth_input.h"
#include "ui.h"

static const char *TAG = "os";

void app_main(void)
{
    console_init();
    ESP_LOGI(TAG, "PixelLoop OS %s on %s", PIXELLOOP_VERSION, PIXELLOOP_BOARD_NAME);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(board_init());

    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 4;
    port_cfg.task_stack = 8192;
    port_cfg.task_affinity = 1;
    port_cfg.task_max_sleep_ms = 500;
    port_cfg.timer_period_ms = 5;
    ESP_ERROR_CHECK(lvgl_port_init(&port_cfg));

    lv_display_t *disp = board_display_create();
    lv_indev_t *touch = board_touch_create(disp);
    ESP_LOGI(TAG, "display %ldx%ld, touch %s", (long)lv_display_get_horizontal_resolution(disp),
             (long)lv_display_get_vertical_resolution(disp), touch ? "yes" : "no");

    pl_lvgl_lock();
    synth_input_create(disp);
    ui_create();
    pl_lvgl_unlock();

    board_backlight_set(100);
    os_register_commands();

    // Give LVGL one render pass before announcing readiness.
    vTaskDelay(pdMS_TO_TICKS(150));
    printf("%s board=%s app=%s w=%ld h=%ld\n", PL_MARK_READY, PIXELLOOP_BOARD_NAME, PIXELLOOP_APP_NAME,
           (long)lv_display_get_horizontal_resolution(disp), (long)lv_display_get_vertical_resolution(disp));
    fflush(stdout);

    console_run();
}
