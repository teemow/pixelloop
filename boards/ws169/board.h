#pragma once
// Waveshare ESP32-S3-Touch-LCD-1.69 — ST7789V2 240x280 over 4-wire SPI,
// CST816 touch + QMI8658 IMU + PCF85063 RTC on one I2C bus.
// Source: https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.69 (also boards/README.md)
#define BOARD_NAME "ws169"

#define BOARD_LCD_H_RES 240
#define BOARD_LCD_V_RES 280
// The ST7789 frame memory is 240x320; the 280-line glass starts 20 rows in.
#define BOARD_LCD_X_GAP 0
#define BOARD_LCD_Y_GAP 20
#define BOARD_LCD_SPI_HOST SPI2_HOST
#define BOARD_LCD_PCLK_HZ (40 * 1000 * 1000)
#define BOARD_LCD_PIN_SCLK 6
#define BOARD_LCD_PIN_MOSI 7
#define BOARD_LCD_PIN_CS 5
#define BOARD_LCD_PIN_DC 4
#define BOARD_LCD_PIN_RST 8
#define BOARD_LCD_PIN_BL 15
#define BOARD_LCD_INVERT_COLOR 1  // IPS panel: 0x21 INVON gives correct colours

#define BOARD_I2C_PORT 0
#define BOARD_I2C_PIN_SDA 11
#define BOARD_I2C_PIN_SCL 10
#define BOARD_I2C_HZ 400000

#define BOARD_TOUCH_PIN_INT 14
#define BOARD_TOUCH_PIN_RST 13
#define BOARD_TOUCH_I2C_ADDR 0x15

#define BOARD_IMU_I2C_ADDR 0x6B
#define BOARD_IMU_PIN_INT1 38
#define BOARD_RTC_I2C_ADDR 0x51
#define BOARD_RTC_PIN_INT 39

#define BOARD_PIN_BAT_ADC 1
#define BOARD_PIN_BUZZER 42
#define BOARD_PIN_SYS_EN 41   // hold high to keep battery power on
#define BOARD_PIN_SYS_OUT 40  // power button state
