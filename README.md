# ESP32 S3 RLCD 4.2 TEST

ESP32-S3 + 4.2 英寸 ST7305 反射式 LCD 的最小硬件自检固件，用于焊接后检查屏幕、触摸、音频、电池采样、按键和 SD 卡。

基于 [LinIT-L/ESP32-S3-RLCD-BBK](https://github.com/LinIT-L/ESP32-S3-RLCD-BBK) 的驱动整理为**独立 ESP-IDF 工程**。
不需要克隆原模拟器仓库，不包含模拟器、游戏、应用菜单、Wi-Fi、蓝牙、USB 磁盘或录音功能。

## 适用硬件

- ESP32-S3，16MB Quad Flash + 8MB Octal PSRAM（例如 ESP32-S3-WROOM-1-N16R8）。
- ST7305，400×300 黑白反射式 LCD。
- ES8311 音频编解码器 + GPIO46 高有效使能功放。
- 可选电容触摸，本机实测 I2C 地址 0x15，使用原项目 CST816 兼容协议。
- 电池采样 GPIO4，200kΩ 上臂 / 100kΩ 下臂分压。
- SDMMC 单线卡槽、BOOT / KEY / PWR 三个按键。

适配的是原板引脚方案的自制板，不是对任意同尺寸屏幕或触摸控制器的通用支持。
请先核对 [硬件与引脚](docs/HARDWARE.md)。

## 直接烧录

[firmware/v0.2.0](firmware/v0.2.0) 提供**已在实板验证的触摸 v2 固件**，包括整合 bin、三个分文件及 SHA256。
[Releases](https://github.com/Kasiin/ESP32-S3-RLCD-4.2-TEST/releases) 提供相同固件下载包。

```sh
python -m pip install -r requirements-flash.txt
python tools/flash.py --port COM15
```

把 `COM15` 改成实际 ESP32 USB 串口；Linux/macOS 使用对应 `/dev/...` 端口。
脚本先核对 SHA256，再分文件烧录，不执行整片擦除、不读写 SD 卡。
它会替换当前固件和分区表；已有其他应用的数据布局可能不同，重要数据请先备份。
若不能自动进入下载模式：按住 BOOT，短按 EN/RESET，再松开 BOOT。

也可使用图形化烧录工具，将 `rlcd-hardware-test-v0.2.0-merged.bin` 烧到 **0x0**。
**整合 bin 适合空白板，会覆盖包含 NVS 在内的低地址区域；升级优先用分文件脚本。**
完整步骤及故障排查见 [烧录说明](docs/FLASHING.md)。

## 如何测试

开机显示状态页，扬声器播放一次 0.5 秒低音量提示音。

| 操作 | 功能 |
| --- | --- |
| BOOT 短按 | 状态 → 全白 → 全黑 → 棋盘格 → 单像素竖条 → 触摸页 → 状态 |
| KEY 短按 | 播放 880Hz 短音 |
| PWR 短按 | 按键计数，不执行关机或休眠 |
| 串口 `T` | 触摸页 / 状态页切换 |
| 串口 `C` | 清空触摸轨迹和五点测试记录 |
| 串口 `S` | SD 容量识别与首扇区只读测试，不格式化、不写卡 |
| 串口 `L` / `A` / `B` / `H` | 下一页 / 音频 / 电池采样 / 帮助和触摸统计 |

串口 115200，指令无需回车；不要同时让两个程序占用同一个串口。
触摸页依次点击 1–5 方框，再沿边缘及中央画线，检查位置、方向、连续性和跳点。
`HIT:5/5` 表示五个点收到坐标，不等于全屏无死区。更多内容见 [测试指南](docs/TESTING.md)。

## 源码构建

已使用 ESP-IDF **v5.5.5** 在 Windows 编译验证；请先安装并激活 ESP-IDF 环境。

```sh
idf.py set-target esp32s3
idf.py build
idf.py -p COM15 flash
idf.py -p COM15 monitor
```

配置已在 `sdkconfig.defaults` 中提供：Flash DIO 40MHz，Octal PSRAM 40MHz，屏幕 SPI 4MHz。
无需外部应用组件或原仓库资源文件。构建输出在 `build/`，不提交到 Git。

打包自己编译的固件：

```sh
python tools/package_firmware.py --build-dir build --output-dir firmware/local --version local
python tools/flash.py --port COM15 --firmware-dir firmware/local
```

发布目录保留的是实板已经运行过的原始 v2 bin，不用整理目录后的新构建替换它。
源码按组件独立拆分，功能源码一致；由于构建路径、时间和版本元数据不同，重新构建的 bin 不保证逐字节相同。

## 项目结构

```text
main/                    自检入口、音频、触摸测试与引脚定义
components/              ST7305、触摸、电池驱动
firmware/v0.2.0/         实板验证 bin、校验值、固件元数据
tools/                   烧录、打包、串口日志工具
docs/                    硬件、烧录、测试指南和验证记录
licenses/                随固件分发的 SDK / C 运行库许可文本
```

实测结果见 [验证记录](docs/VALIDATION.md)。这是一套功能自检程序，不替代电源、充电、安规、EMC 或长期稳定性测试。

## 来源与许可

基于上游提交 `1c0549206f128c311103f374ee46c38b95229c91`，保留上游许可和来源说明。
本项目按上游的 **GPL-2.0-or-later** 条款分发，详见 [LICENSE](LICENSE) 和 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
