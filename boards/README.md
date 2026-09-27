# Boards

Short ids are what `make BOARD=<id>` and `tools/probe.py` use. Every board here
is the ESP32-S3R8 (16 MB quad flash, 8 MB octal PSRAM, native USB-Serial/JTAG)
unless noted. Pin data comes from the Waveshare wiki/docs pages linked per board;
verify against the schematic before trusting a pin that is not yet exercised
by a driver in this repo.

| id      | Waveshare board            | display                     | touch     | shared I2C (SDA/SCL) | extras |
|---------|----------------------------|-----------------------------|-----------|----------------------|--------|
| `ws169` | ESP32-S3-Touch-LCD-1.69    | ST7789V2 240×280, 4-wire SPI | CST816D/T @0x15 | 11/10 (touch+IMU+RTC) | QMI8658 @0x6B, PCF85063 @0x51, buzzer, battery |
| `ws185` | ESP32-S3-Touch-LCD-1.85    | ST77916 360×360, QSPI       | CST816 @0x15 on 1/3 | 11/10 (IMU+RTC+expander) | TCA9554 @0x20, PCM5101, mic, TF card |
| `ws146` | ESP32-S3-Touch-LCD-1.46    | SPD2010 412×412, QSPI       | SPD2010 @0x53 | 11/10 | TCA9554 @0x20, PCM5101, TF card |
| `ws28`  | ESP32-S3-Touch-LCD-2.8     | ST7789 240×320, SPI         | CST328 @0x1A on 1/3 | 11/10 (IMU+RTC) | PCM5101, TF card |
| `ws21`  | ESP32-S3-Touch-LCD-2.1     | ST7701 480×480, RGB         | CST820 @0x15 | 15/7 (all) | TCA9554 @0x20, TF card |
| `ws43`  | ESP32-S3-Touch-LCD-4.3     | ST7262 800×480, RGB         | GT911 @0x5D | 8/9 | CH422G expander, CAN, RS485 (WROOM-1-N16R8) |
| `ws128` | ESP32-S3-Touch-LCD-1.28    | GC9A01 240×240, SPI         | CST816S @0x15 | 6/7 | 2 MB PSRAM variant |

## ws169 — ESP32-S3-Touch-LCD-1.69 (docs.waveshare.com/ESP32-S3-Touch-LCD-1.69)

| function | GPIO | | function | GPIO |
|---|---|---|---|---|
| LCD_CS  | 5  | | TP_SDA | 11 |
| LCD_DC  | 4  | | TP_SCL | 10 |
| LCD_CLK | 6  | | TP_INT | 14 |
| LCD_DIN | 7  | | TP_RST | 13 |
| LCD_RST | 8  | | IMU_INT1 | 38 |
| LCD_BL  | 15 | | RTC_INT | 39 |
| BAT_ADC | 1  | | BUZZER | 42 |
| BOOT    | 0  | | SYS_EN (power hold) | 41 |
| UART TX/RX | 43/44 | | SYS_OUT (power button state) | 40 |

## ws185 — ESP32-S3-Touch-LCD-1.85 (waveshare.com/wiki/ESP32-S3-Touch-LCD-1.85)

QSPI LCD: D0 46, D1 45, D2 42, D3 41, SCK 40, CS 21, TE 18, RST EXIO2, BL 5.
Touch: SDA 1, SCL 3, INT 4, RST EXIO1. IMU/RTC/expander I2C: SDA 11, SCL 10, RTC_INT 9.
SD: MISO 16, MOSI 17, SCK 14, CS EXIO3. I2S speaker: DIN 47, LRCK 38, BCK 48. Mic: WS 2, SCK 15, SD 39.

## ws146 — ESP32-S3-Touch-LCD-1.46 (docs.waveshare.com/ESP32-S3-Touch-LCD-1.46)

Same QSPI/SD/audio layout as ws185; touch SPD2010 on SDA 11 / SCL 10, INT 4, RST EXIO1.

## ws28 — ESP32-S3-Touch-LCD-2.8 (waveshare.com/wiki/ESP32-S3-Touch-LCD-2.8)

LCD SPI: MOSI 45, SCLK 40, CS 42, DC 41, RST 39, BL 5. Touch CST328: SDA 1, SCL 3, INT 4, RST 2.
IMU/RTC: SDA 11, SCL 10, IMU_INT1 13, IMU_INT2 12, RTC_INT 9. SD: MISO 16, MOSI 17, SCK 14, CS 21.
I2S: LRCK 38, DIN 47, BCK 48.

## ws21 — ESP32-S3-Touch-LCD-2.1 (waveshare.com/wiki/ESP32-S3-Touch-LCD-2.1)

RGB: PCLK 41, DE 40, VSYNC 39, HSYNC 38; R1–R5 46,3,8,18,17; G0–G5 14,13,12,11,10,9; B1–B5 5,45,48,47,21.
Panel init over SPI-ish: LCD_SDA 1, LCD_SCL 2, CS EXIO3, RST EXIO1, BL 6. Touch/IMU/RTC I2C: SDA 15, SCL 7, TP_INT 16.
BAT_ADC 4. TCA9554 drives RST/CS/INTs/buzzer.

## ws43 — ESP32-S3-Touch-LCD-4.3 (waveshare.com/wiki/ESP32-S3-Touch-LCD-4.3)

RGB 800×480: VSYNC 3, DE 5, PCLK 7, HSYNC 46; G 0,39,45,48,47,21; R 1,2,40,41,42; B 10,14,17,18,38.
I2C SDA 8 / SCL 9 (GT911 @0x5D, CH422G). TP_IRQ 4. SD: MOSI 11, SCK 12, MISO 13. RS485/CAN 15/16.
