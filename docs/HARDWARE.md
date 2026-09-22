# 硬件与引脚

核对实际原理图后再烧录。以下是已经验证的原板引脚方案。

| 功能 | GPIO / 说明 |
| --- | --- |
| LCD DC / RESET / CS / CLK / MOSI | 5 / 41 / 40 / 11 / 12 |
| ES8311 SDA / SCL | 13 / 14，I2C0 100kHz |
| 音频 MCLK / BCLK / LRCK / DOUT | 16 / 9 / 45 / 8 |
| 功放使能 | 46，高有效；只在短音期间开启 |
| 触摸 SDA / SCL / INT / RESET | 15 / 7 / 17 / 2，独立 I2C1 100kHz |
| 电池 ADC | 4，ADC1_CH3；VBAT → 200kΩ → ADC → 100kΩ → GND |
| BOOT / KEY / PWR | 0 / 18 / 1，低有效 |
| SD CMD / CLK / D0 | 21 / 38 / 39，1-bit，5MHz；不使用卡检测脚 |
| 原生 USB D- / D+ | 19 / 20，保留给 USB Serial/JTAG |

音频等引脚在 `main/board_pins.h`；触摸引脚在 `components/touch_panel/touch_panel.h`；
电池 ADC 和分压倍率在 `components/board_battery/board_battery.c`。
不要只修改一个头文件就假定所有硬件配置同步变化。

本机触摸为 0x15，原驱动按 CST816 兼容方式识别。驱动保留上游 GT911/FT6236 分支，
但这两个分支没有在本项目的实板上验证，不承诺其他面板可直接使用。
触摸与音频不共享 I2C 控制器，GPIO17 没有用作 SD 检测。

本固件针对 16MB Flash、8MB Octal PSRAM，其他模块要调整配置。
缺少触摸、音频或 SD 卡不会故意阻止其余自检继续；不代表缺失外设测试通过。
电池电压显示依赖实际分压电阻，不是电量计，也不是充电状态检测。

参考硬件资料：[微雪 ESP32-S3-RLCD-4.2](https://docs.waveshare.net/ESP32-S3-RLCD-4.2/Resources-And-Documents)。
本仓库不包含自制板完整原理图或 PCB 工程，也不表示与所有官方板卡版本完全兼容。
