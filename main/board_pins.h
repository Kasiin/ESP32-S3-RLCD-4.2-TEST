#pragma once

// Stock Waveshare / LinIT-L wiring. Check against the final PCB before flashing.
#define PIN_LCD_DC 5
#define PIN_LCD_RST 41
#define PIN_LCD_CS 40
#define PIN_LCD_SCK 11
#define PIN_LCD_MOSI 12
#define PIN_I2C_SDA 13
#define PIN_I2C_SCL 14
#define PIN_I2S_BCLK 9
#define PIN_I2S_WS 45
#define PIN_I2S_DOUT 8
#define PIN_I2S_MCLK 16
#define PIN_PA_ENABLE 46
#define PIN_KEY 18
#define PIN_BOOT 0
#define PIN_PWR 1
#define PIN_SD_CMD 21
#define PIN_SD_CLK 38
#define PIN_SD_D0 39
// Battery GPIO4 / ADC1_CH3, 200k upper + 100k lower, in upstream board_battery.c.
