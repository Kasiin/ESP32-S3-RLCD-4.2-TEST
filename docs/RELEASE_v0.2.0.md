# v0.2.0 · 最小硬件自检（含触摸）

适用于原板引脚方案的 ESP32-S3 / ST7305 4.2 英寸反射式 LCD，16MB Flash、8MB Octal PSRAM。

包含屏幕测试图、触摸画线与五点测试、ES8311 音频短音、电池 ADC、三按键、PSRAM 检查和 SD 只读测试。
没有模拟器、游戏、Wi-Fi、蓝牙或 USB 磁盘功能。

本次发布保留实板已经验证的触摸 v2 bin：五点触摸 5/5，SD 3840MiB 首扇区读取成功，
音频、显示及 PSRAM 初始化检查正常。完整源码独立整理并通过 ESP-IDF v5.5.5 构建。

## 下载和烧录

- `ESP32-S3-RLCD-4.2-TEST-v0.2.0.zip`：源码、固件、脚本、许可和中文文档完整包。
- `rlcd-hardware-test-v0.2.0-merged.bin`：空白板整合镜像，烧录地址 **0x0**。
- 分文件：bootloader → 0x0，partition-table → 0x8000，rlcd_hardware_test → 0x10000。
- `SHA256SUMS` / `manifest.json`：校验信息与固件来源。

完整包解压后可执行：

```sh
python -m pip install -r requirements-flash.txt
python tools/flash.py --port COM15
```

替换成实际 ESP32 端口。分文件脚本先校验哈希，不整片擦除、不写 SD 卡。
**整合镜像包含填充区，会覆盖 NVS/PHY；已有设备升级优先使用分文件脚本。**

BOOT 切换测试页面；KEY 播放短音；PWR 只计数，不休眠。
串口 T 切换触摸页，C 清除轨迹，S 只读测试 SD，H 显示帮助。

电池电压不等于电量计或充电电路合格，五点通过不等于全屏无死区；本固件不能替代长期稳定性和产品电气测试。

上游：[LinIT-L/ESP32-S3-RLCD-BBK](https://github.com/LinIT-L/ESP32-S3-RLCD-BBK)，基线 `1c05492`；保留原许可和来源声明。
