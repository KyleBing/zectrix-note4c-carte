# NOTE4C 固件（待办看板）

PlatformIO + Arduino。**四色墨水屏待办页** + **WiFi / Web 配网鉴权**（portal-go diary）。

完整说明：[固件功能](../docs/features/index.md)

## 编译 / 刷写

芯片：**ESP32-S3 N16R8**（16 MB Flash / 8 MB PSRAM）· env：`note4c`

| 操作 | 说明 |
| --- | --- |
| Build | `pio run` |
| Upload | `pio run -t upload` |
| Upload FS | `pio run -t uploadfs`（`data/` 字库，**首次必做**） |

仓库根目录也可：

```bash
./flash.sh        # 编译并刷固件
./flash.sh all    # 固件 + LittleFS
./flash.sh fs     # 只刷字库
./flash.sh mon    # 串口 115200
```

```bash
cd firmware
pio run
pio run -t upload
pio run -t uploadfs
pio device monitor   # 115200
```

### Flash 分区（`default_16MB.csv`）

`platformio.ini`：`board_build.partitions = default_16MB.csv`，文件系统为 **LittleFS**（分区表 subtype 仍写 `spiffs`，Arduino 惯例）。

整片 Flash：**16 MiB = `0x1000000`**。

| 名称 | Type / SubType | 起始地址 | 大小 | 结束地址 | 用途 |
| --- | --- | --- | --- | --- | --- |
| bootloader | — | `0x000000` | 32 KB (`0x8000`) | `0x008000` | 二级启动 |
| partition table | — | `0x008000` | 4 KB (`0x1000`) | `0x009000` | 分区表 |
| nvs | data / nvs | `0x009000` | 20 KB (`0x5000`) | `0x00E000` | Preferences / WiFi 等键值 |
| otadata | data / ota | `0x00E000` | 8 KB (`0x2000`) | `0x010000` | OTA 槽位选择 |
| **app0** | app / ota_0 | **`0x010000`** | **6.25 MiB** (`0x640000`) | `0x650000` | **当前固件**（`firmware.bin`） |
| app1 | app / ota_1 | `0x650000` | 6.25 MiB (`0x640000`) | `0xC90000` | OTA 备用槽（本工程未用 OTA） |
| **spiffs** | data / spiffs | **`0xC90000`** | **3.375 MiB** (`0x360000`) | `0xFF0000` | **LittleFS**（字库、相册、favicon） |
| coredump | data / coredump | `0xFF0000` | 64 KB (`0x10000`) | `0x1000000` | 崩溃转储 |

### `pio` / esptool 刷写地址

| 镜像 | 刷入地址 | 命令来源 |
| --- | --- | --- |
| `bootloader.bin` | `0x0` | `pio run -t upload` |
| `partitions.bin` | `0x8000` | 同上 |
| `firmware.bin` | `0x10000`（`ESP32_APP_OFFSET`） | 同上 |
| LittleFS 镜像（`data/`） | `0xC90000` | `pio run -t uploadfs` |

手动示例（端口按实际改）：

```bash
esptool.py --chip esp32s3 write_flash \
  0x0      .pio/build/note4c/bootloader.bin \
  0x8000   .pio/build/note4c/partitions.bin \
  0x10000  .pio/build/note4c/firmware.bin
# 字库 / 相册文件系统另用：pio run -t uploadfs  → 写到 0xC90000
```

## 按键

- **BOOT** 松开：拉日记并全刷
- **UP** 短按：开配置 Web（有 WiFi→STA，否则 AP）
- **UP** 长按 ~1.5s：强制 AP（`note4c` / `note4c4c`，密码须 ≥8 位）
- **DOWN** 长按 ~3s：释放电源保持
- **LED**：按键按下即亮、松开即灭；刷屏呼吸；慢闪连 WiFi；失败三短闪

## Web

`/` 状态 · `/wifi` 多 WiFi · `/auth` 登录与 diary id · `/todo` 预览 · `/weather` · `/album`

JWT：登录保存；业务响应若带 `X-Access-Token` 则自动更新（portal-go 剩余 &lt; 7 天续签）。
