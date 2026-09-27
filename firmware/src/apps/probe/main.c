// PixelLoop board probe.
//
// Prints a machine-readable report over the USB console so tools/probe.py can
// identify the attached board without any prior knowledge of it:
//   - chip model/revision, flash size, PSRAM size, MAC, IDF version
//   - an I2C scan on every (SDA, SCL) pair Waveshare uses on its ESP32-S3
//     Touch LCD boards, listing the addresses that ACK
//
// Output format (one JSON object per line, framed by markers):
//   PROBE-BEGIN
//   {"k":"chip", ...}
//   {"k":"i2c","sda":11,"scl":10,"addrs":[21,81,107]}
//   ...
//   PROBE-END
// The block repeats every few seconds, so the host can attach any time.
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_psram.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pixelloop.h"

static const char *TAG = "probe";

typedef struct {
    int sda, scl;
} bus_pins_t;

// Every I2C pin pair seen on a Waveshare ESP32-S3-Touch-LCD board, both
// orientations. Strapping pins 0/45/46, USB 19/20 and UART0 43/44 are left
// alone on purpose. (Sources: Waveshare wiki/docs for 1.28, 1.69, 1.85, 1.46,
// 2.1, 2.8, 4.3.)
static const bus_pins_t k_buses[] = {
    {11, 10}, {1, 3}, {15, 7}, {8, 9}, {6, 7},
    {10, 11}, {3, 1}, {7, 15}, {9, 8}, {7, 6},
};

static void scan_bus(int sda, int scl)
{
    i2c_master_bus_config_t cfg = {
        .i2c_port = -1,  // any free controller
        .sda_io_num = sda,
        .scl_io_num = scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus = NULL;
    esp_err_t err = i2c_new_master_bus(&cfg, &bus);
    if (err != ESP_OK) {
        printf("{\"k\":\"i2c\",\"sda\":%d,\"scl\":%d,\"addrs\":[],\"err\":\"%s\"}\n", sda, scl,
               esp_err_to_name(err));
        return;
    }
    printf("{\"k\":\"i2c\",\"sda\":%d,\"scl\":%d,\"addrs\":[", sda, scl);
    bool first = true;
    for (int addr = 0x08; addr < 0x78; addr++) {
        // 10 ms per address bounds a bus held low by a non-I2C peripheral.
        if (i2c_master_probe(bus, addr, 10) == ESP_OK) {
            printf("%s%d", first ? "" : ",", addr);
            first = false;
        }
    }
    printf("]}\n");
    i2c_del_master_bus(bus);
    // Leave the pins floating again so a later driver sees them untouched.
    gpio_reset_pin(sda);
    gpio_reset_pin(scl);
}

static void print_chip(void)
{
    esp_chip_info_t ci;
    esp_chip_info(&ci);
    uint32_t flash_cfg = 0, flash_phys = 0;
    esp_flash_get_size(NULL, &flash_cfg);
    esp_flash_get_physical_size(NULL, &flash_phys);
    size_t psram = esp_psram_is_initialized() ? esp_psram_get_size() : 0;
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    const esp_app_desc_t *app = esp_app_get_description();

    const char *model = "unknown";
    switch (ci.model) {
    case CHIP_ESP32: model = "ESP32"; break;
    case CHIP_ESP32S2: model = "ESP32-S2"; break;
    case CHIP_ESP32S3: model = "ESP32-S3"; break;
    case CHIP_ESP32C3: model = "ESP32-C3"; break;
    case CHIP_ESP32C6: model = "ESP32-C6"; break;
    case CHIP_ESP32H2: model = "ESP32-H2"; break;
    default: break;
    }
    printf("{\"k\":\"chip\",\"model\":\"%s\",\"rev\":%d,\"cores\":%d,"
           "\"flash_mb\":%" PRIu32 ",\"flash_phys_mb\":%" PRIu32 ",\"psram_mb\":%u,"
           "\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\",\"idf\":\"%s\","
           "\"app\":\"%s\",\"version\":\"%s\",\"board\":\"%s\"}\n",
           model, ci.revision, ci.cores, flash_cfg / (1024 * 1024), flash_phys / (1024 * 1024),
           (unsigned)(psram / (1024 * 1024)), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
           esp_get_idf_version(), app->project_name, PIXELLOOP_VERSION, PIXELLOOP_BOARD_NAME);
}

void app_main(void)
{
    ESP_LOGI(TAG, "PixelLoop probe %s (app %s, board %s)", PIXELLOOP_VERSION, PIXELLOOP_APP_NAME,
             PIXELLOOP_BOARD_NAME);
    // Pins that are not an I2C bus on this board time out on every address;
    // the driver's per-address error line would drown the report.
    esp_log_level_set("i2c.master", ESP_LOG_NONE);
    for (;;) {
        printf("PROBE-BEGIN\n");
        print_chip();
        for (size_t i = 0; i < sizeof(k_buses) / sizeof(k_buses[0]); i++) {
            scan_bus(k_buses[i].sda, k_buses[i].scl);
        }
        printf("PROBE-END\n");
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
