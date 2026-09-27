// ESP-IDF implementation of the port layer (pl_port.h).
#include "pl_port.h"

#include <stdarg.h>
#include <stdio.h>

#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_rom_crc.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "mbedtls/base64.h"

int64_t pl_uptime_us(void)
{
    return esp_timer_get_time();
}

void pl_lvgl_lock(void)
{
    lvgl_port_lock(0);
}

void pl_lvgl_unlock(void)
{
    lvgl_port_unlock();
}

void *pl_alloc_large(size_t bytes)
{
    void *p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p ? p : heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
}

void pl_free_large(void *p)
{
    heap_caps_free(p);
}

uint32_t pl_crc32(const void *data, size_t len)
{
    return esp_rom_crc32_le(0, data, len);
}

size_t pl_base64_encode(char *dst, size_t dst_cap, const uint8_t *src, size_t len)
{
    size_t olen = 0;
    if (mbedtls_base64_encode((unsigned char *)dst, dst_cap, &olen, src, len) != 0) {
        return 0;
    }
    return olen;
}

void pl_mem_stats(pl_mem_stats_t *out)
{
    out->heap_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    out->heap_min = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    out->psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}

void pl_console_setup(void)
{
    // Switch the console to the interrupt-driven driver so stdin blocks
    // properly and large stdout writes (screenshots) are buffered.
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    cfg.tx_buffer_size = 8192;
    cfg.rx_buffer_size = 1024;
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&cfg));
    usb_serial_jtag_vfs_use_driver();
    usb_serial_jtag_vfs_set_rx_line_endings(ESP_LINE_ENDINGS_LF);
    usb_serial_jtag_vfs_set_tx_line_endings(ESP_LINE_ENDINGS_LF);
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IOLBF, 0);
}

void pl_restart(void)
{
    esp_restart();
}

void pl_log(char level, const char *tag, const char *fmt, ...)
{
    char msg[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    switch (level) {
    case 'E':
        ESP_LOGE(tag, "%s", msg);
        break;
    case 'W':
        ESP_LOGW(tag, "%s", msg);
        break;
    default:
        ESP_LOGI(tag, "%s", msg);
        break;
    }
}
