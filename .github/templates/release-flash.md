## 刷机说明

> **烧录前请先备份**  
> 整片刷写 `*-merged.bin` 会覆盖 Flash 中的程序分区内容。首次刷自研固件前建议完整备份 **16 MB** 原厂 Flash。

### 下载固件

| 文件 | 用途 |
|------|------|
| `note4c-carte-${TAG}-merged.bin` | 整片烧录（bootloader + 分区表 + 程序），地址 `0x0` |
| `note4c-carte-${TAG}-firmware.bin` | 仅 app（一般配合 PlatformIO Upload / 对应偏移） |

### 刷写工具

```bash
pip install -U esptool
```

### 连接设备

USB 连接 NOTE4C（Carte 目标板），确认串口后替换下方命令中的 `PORT`：

- Windows：`COM3` 等
- macOS：`/dev/cu.usbmodem*`
- Linux：`/dev/ttyACM0` 或 `/dev/ttyUSB0`

若未识别：按住 **BOOT** → 短按 **RESET** → 松开 BOOT，再选新串口。

### 刷写指令（合并镜像）

```bash
esptool.py --chip esp32s3 --port PORT --baud 921600 write_flash \
  --flash_mode qio --flash_freq 80m --flash_size 16MB \
  0x0 note4c-carte-${TAG}-merged.bin
```

### PlatformIO 本地编译 / 上传

```bash
cd firmware
pio run -e carte -t upload
pio device monitor
```

本地 `upload` 只写程序分区。

### 使用提示

- 开机全刷状态页约 **16 秒**，属正常
- **短按松开 BOOT**：再刷一页最新状态
- **长按 DOWN ~2s**：关机（深睡）；再按下键唤醒开机
- 文档：https://kylebing.github.io/zectrix-note4c-carte/
