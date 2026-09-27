// Board layer for the Waveshare ESP32-S3-Touch-LCD-1.69.
#include "board.h"
#include "board_api.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_lcd_io_i2c.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_touch_cst816s.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

static const char *TAG = "ws169";

static esp_lcd_panel_io_handle_t s_lcd_io;
static esp_lcd_panel_handle_t s_lcd;
static i2c_master_bus_handle_t s_i2c;
static esp_lcd_touch_handle_t s_touch;

#define BL_LEDC_TIMER LEDC_TIMER_0
#define BL_LEDC_CHANNEL LEDC_CHANNEL_0
#define BL_LEDC_MODE LEDC_LOW_SPEED_MODE
#define BL_LEDC_RES LEDC_TIMER_10_BIT

esp_err_t board_init(void)
{
    // Keep the battery power path enabled when running off the cell.
    gpio_config_t sys_en = {
        .pin_bit_mask = 1ULL << BOARD_PIN_SYS_EN,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&sys_en), TAG, "sys_en");
    gpio_set_level(BOARD_PIN_SYS_EN, 1);

    // Backlight PWM, starts dark until the first frame is up.
    ledc_timer_config_t tcfg = {
        .speed_mode = BL_LEDC_MODE,
        .duty_resolution = BL_LEDC_RES,
        .timer_num = BL_LEDC_TIMER,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&tcfg), TAG, "ledc timer");
    ledc_channel_config_t ccfg = {
        .gpio_num = BOARD_LCD_PIN_BL,
        .speed_mode = BL_LEDC_MODE,
        .channel = BL_LEDC_CHANNEL,
        .timer_sel = BL_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&ccfg), TAG, "ledc channel");

    // Shared I2C bus: touch, IMU, RTC.
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = BOARD_I2C_PORT,
        .sda_io_num = BOARD_I2C_PIN_SDA,
        .scl_io_num = BOARD_I2C_PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &s_i2c), TAG, "i2c bus");

    // SPI bus for the panel. One full frame per transfer keeps the DMA
    // descriptor count sane and lets LVGL flush whole frames from PSRAM.
    spi_bus_config_t spi_cfg = {
        .sclk_io_num = BOARD_LCD_PIN_SCLK,
        .mosi_io_num = BOARD_LCD_PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BOARD_LCD_H_RES * BOARD_LCD_V_RES * 2,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BOARD_LCD_SPI_HOST, &spi_cfg, SPI_DMA_CH_AUTO), TAG, "spi bus");
    return ESP_OK;
}

lv_display_t *board_display_create(void)
{
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = BOARD_LCD_PIN_CS,
        .dc_gpio_num = BOARD_LCD_PIN_DC,
        .spi_mode = 0,
        .pclk_hz = BOARD_LCD_PCLK_HZ,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BOARD_LCD_SPI_HOST, &io_cfg, &s_lcd_io));

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = BOARD_LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(s_lcd_io, &panel_cfg, &s_lcd));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_lcd));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_lcd));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(s_lcd, BOARD_LCD_X_GAP, BOARD_LCD_Y_GAP));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_lcd, BOARD_LCD_INVERT_COLOR));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_lcd, true));

    lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = s_lcd_io,
        .panel_handle = s_lcd,
        .buffer_size = BOARD_LCD_H_RES * BOARD_LCD_V_RES,
        .double_buffer = true,
        .hres = BOARD_LCD_H_RES,
        .vres = BOARD_LCD_V_RES,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {.swap_xy = false, .mirror_x = false, .mirror_y = false},
        .flags = {
            .buff_dma = false,
            .buff_spiram = true,
            .swap_bytes = true,  // ST7789 wants big-endian RGB565 on the wire
        },
    };
    lv_display_t *disp = lvgl_port_add_disp(&disp_cfg);
    if (!disp) {
        ESP_LOGE(TAG, "lvgl_port_add_disp failed");
        abort();
    }
    return disp;
}

lv_indev_t *board_touch_create(lv_display_t *disp)
{
    esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    tp_io_cfg.scl_speed_hz = BOARD_I2C_HZ;
    esp_lcd_panel_io_handle_t tp_io = NULL;
    esp_err_t err = esp_lcd_new_panel_io_i2c(s_i2c, &tp_io_cfg, &tp_io);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "touch io: %s", esp_err_to_name(err));
        return NULL;
    }
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = BOARD_LCD_H_RES,
        .y_max = BOARD_LCD_V_RES,
        .rst_gpio_num = BOARD_TOUCH_PIN_RST,
        .int_gpio_num = BOARD_TOUCH_PIN_INT,
        .levels = {.reset = 0, .interrupt = 0},
        .flags = {.swap_xy = 0, .mirror_x = 0, .mirror_y = 0},
    };
    err = esp_lcd_touch_new_i2c_cst816s(tp_io, &tp_cfg, &s_touch);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "touch init: %s (running without touch)", esp_err_to_name(err));
        return NULL;
    }
    lvgl_port_touch_cfg_t touch_cfg = {.disp = disp, .handle = s_touch};
    return lvgl_port_add_touch(&touch_cfg);
}

void board_backlight_set(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }
    uint32_t duty = ((1u << 10) - 1) * percent / 100;
    ledc_set_duty(BL_LEDC_MODE, BL_LEDC_CHANNEL, duty);
    ledc_update_duty(BL_LEDC_MODE, BL_LEDC_CHANNEL);
}
