# 烧录与恢复

## 推荐：分文件烧录

安装 Python 3，执行：

```sh
python -m pip install -r requirements-flash.txt
python tools/flash.py --port COM15
```

脚本使用 esptool 4.x，先验证 `SHA256SUMS`，再写入：

| 地址 | 文件 |
| --- | --- |
| 0x0 | bootloader.bin |
| 0x8000 | partition-table.bin |
| 0x10000 | rlcd_hardware_test.bin |

Flash 参数 DIO / 40MHz / 16MB，默认烧录速率 460800；不稳定可加 `--baud 115200`。
`--dry-run` 可验证文件并打印命令，不连接设备。
必须核对端口对应 ESP32-S3，不要选择其他串口设备。

## 图形工具：整合 bin

把 `rlcd-hardware-test-v0.2.0-merged.bin` 烧录到 **0x0**，目标芯片 ESP32-S3。
这个文件包含启动程序、分区表和应用，不是仅应用镜像。
它的填充区覆盖 0x9000–0xFFFF 的 NVS/PHY 区域，因此用于空白板最方便，
已配置设备升级时建议用上面的分文件方式。

不要把 `rlcd_hardware_test.bin` 单应用文件写到 0x0。
仅当设备已经使用相同自检分区表时，才可只更新 0x10000 的应用。
本程序不使用模拟器原来的大资源分区，也不会格式化 SD 卡。

## 下载模式和启动

1. 连接能传数据的 USB 线。
2. 自动下载失败时，按住 BOOT，短按 EN/RESET，再松开 BOOT。
3. 完成烧录并提示哈希校验通过后，松开 BOOT，让设备重启。
4. 如未启动，短按 EN/RESET；PWR 是测试按键，不等同于硬件复位。
5. 屏幕应出现 `RLCD HARDWARE TEST v2`；音频存在时短响一次。

程序使用 USB Serial/JTAG 而不是 USB 磁盘/自定义 HID，端口通常可以继续用于日志。
若电脑反复连接/断开，先看串口是否出现 panic、brownout 或重启原因；不能仅凭连接音判断坏板。

## 日志与指令

```sh
python tools/capture_serial.py --port COM15 --seconds 30 --log check.log
python tools/capture_serial.py --port COM15 --seconds 10 --send S
python tools/capture_serial.py --port COM15 --seconds 30 --send T
```

日志工具不会切换 BOOT/RESET 控制线，运行时间结束后释放串口；断线后会在剩余时间内尝试重连。
也可以使用普通串口终端。使用终端时不要再运行另一个日志工具或烧录工具占用同一端口。

重要：烧录会替换现有应用和分区表。想回到其他固件时，应按目标项目说明恢复其启动程序、分区表、应用和资源。
本仓库不执行 eFuse 写入、整片擦除、设备 Flash 备份上传或 SD 写入操作。
