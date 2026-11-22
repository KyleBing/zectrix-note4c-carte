# Carte 固件

PlatformIO + Arduino。面向 **ZECTRIX NOTE4C** 四色墨水屏：多 App（待办 / 日历 / 天气 / 相册）+ **WiFi / Web 配网鉴权**（portal-go）。

完整说明：[固件功能](../docs/features/index.md) · [App 功能](../docs/features/apps.md)

## 编译 / 刷写

芯片：**ESP32-S3 N16R8**（16 MB Flash / 8 MB PSRAM）· env：`carte`

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
  0x0      .pio/build/carte/bootloader.bin \
  0x8000   .pio/build/carte/partitions.bin \
  0x10000  .pio/build/carte/firmware.bin
# 字库 / 相册文件系统另用：pio run -t uploadfs  → 写到 0xC90000
```

## 按键

- **BOOT** 松开：App 内功能（天气切 NOW↔7D / 相册翻页 / 待办·日历刷新）；连按后静默 500ms 合并跳过 N 次
- **UP** 短按：上一个 App（连按同样 500ms 合并）
- **UP** 长按 ~1.5s：开配置 Web（有 WiFi→同 WiFi 站点，否则 AP 模式）
- **UP** 再长按（已在 Web）：AP 模式 ↔ 同 WiFi 站点（热点密码 `12345678`）
- **DOWN** 短按：下一个 App（连按同样 500ms 合并）
- **DOWN** 长按 ~3s：关机（深睡）；再按下键唤醒开机
- **刷屏中**：导航键可立刻打断四色全刷（~10s+），停手后目标页完整刷完；详见 [行为机制 · 屏幕刷新](../docs/features/behavior.md)
- **LED**：按键按下即亮、松开即灭；全刷成功结束一闪；慢闪连 WiFi；失败三短闪

## Web

`/` 状态 · `/settings` 设置（应用开关 / 电源 / 时间）· `/help` 帮助（含 LED）· `/wifi` · `/auth` · `/todo` · `/weather` · `/album`

JWT：登录保存；业务响应若带 `X-Access-Token` 则自动更新（portal-go 剩余 &lt; 7 天续签）。

相册编辑页可在电脑浏览器里直接测（不必刷机 / 开 WiFi）：

```bash
python3 tools/album_preview.py    # http://127.0.0.1:8765
```
