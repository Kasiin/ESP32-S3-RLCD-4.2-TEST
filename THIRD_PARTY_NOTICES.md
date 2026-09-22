# 来源与第三方许可

## LinIT-L/ESP32-S3-RLCD-BBK

- 来源：https://github.com/LinIT-L/ESP32-S3-RLCD-BBK
- 提取基线：`1c0549206f128c311103f374ee46c38b95229c91`
- 上游版权声明：Copyright (C) 2026 linit。
- 根目录 LICENSE 保留上游 GPL v2 文本及其 GPL-2.0-or-later 声明。

原样提取：

- `components/st7305/st7305.c`、`st7305.h`。
- `components/board_battery/board_battery.c`、`include/board_battery.h`。
- `components/touch_panel/touch_panel.c`、`touch_panel.h`。

`main/test_audio.c` 的 ES8311 初始化序列来自上游 `components/audio_player/es8311.c`，
增加寄存器失败处理、实际设备地址选择和读回验证，移除播放器、解码器和麦克风依赖。
自检入口、触摸测试界面、辅助脚本与文档作为派生测试工程同样按 GPL-2.0-or-later 分发。
未带入游戏 ROM、模拟器、字库资源包或第三方应用。

## ESP-IDF 与运行库

- 固件基于 Espressif ESP-IDF v5.5.5 构建：https://github.com/espressif/esp-idf/tree/v5.5.5
- ESP-IDF 自身主要采用 Apache-2.0，所带组件按各自许可分发。
- SDK 根许可文本附于 `licenses/ESP-IDF-Apache-2.0.txt`。
- C 运行库声明附于 `licenses/COPYING.NEWLIB`、`licenses/COPYING.picolibc`。
- FreeRTOS MIT 文本附于 `licenses/FreeRTOS-MIT.md`；SDK 中 micro-ecc 和 Mbed TLS 的声明也附于 `licenses/`。
- 编译器支持库等仍遵循 SDK/工具链中各自的版权与许可文本，不因本项目根 LICENSE 改变。

编译请使用官方 ESP-IDF 安装及工具链；本仓库不重新打包 SDK 或编译器。
