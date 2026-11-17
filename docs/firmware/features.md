# 固件功能说明

> 当前固件：**硬件自检**（Serial + 四色墨水屏状态页）  
> 版本：`{{DOC_VERSION}}` · 工程目录：`firmware/` · PlatformIO env：`note4c`

---

## 1. 定位

本仓库当前交付的是 **NOTE4C 到货验收 / 硬件自检** 固件，不是完整产品 UI。

| 目标 | 说明 |
| --- | --- |
| 验证板级电源保持 | 启动后拉高 `GPIO17`，电池可继续运行 |
| 验证人机与电源脚 | 三键、LED、电池 ADC、CHRG / STDBY |
| 验证 I2C 外设在位 | RTC `0x51`、NFC `0x55`、ES8311 探测 |
| 验证四色屏通路 | SSD2683 全刷一页状态信息（约 10s+） |

尚未实现（硬件具备，待后续）：Wi-Fi / BLE 应用、录音播放、NFC 协议、RTC 校时、相册 / 看板产品 UI。

---

## 2. 屏幕行为

- **开机**：采样硬件状态，全刷一页（约 10 秒以上，属正常）
- **顶部四色色条**：黑 / 白 / 红 / 黄，用于确认 BWRY 通路
- **正文**：按键、电池、充电脚、I2C（RTC / NFC / ES8311）、Heap / PSRAM
- **短按后松开 BOOT**：再刷一页最新状态（勿每秒全刷）
- **长按 DOWN ~3s**：释放电源保持（电池供电时可能关机）

屏驱序列来自社区参考仓四色路径（MIT），本仓库最小 Arduino 移植：`firmware/src/epd_ssd2683.cpp`。

状态页示意字段：

| 区域 | 内容 |
| --- | --- |
| 色条 | 黑 / 白 / 红 / 黄 各 100×16 |
| 标题 | `NOTE4C SELFTEST` / `SSD2683 400x300 BWRY` |
| 电源 | `PWR HOLD: ON` |
| 按键 | `BTN BOOT / UP / DOWN` |
| 电池 | 近似 mV（分压估算，到货后可按原理图校准） |
| 充电 | `CHRG` / `STDBY` |
| I2C | RTC / NFC / ES8311 → `OK` 或 `--` |
| 其它 | NFC_FIELD、RTC_INT、EPD_BUSY、Heap、PSRAM |
| 提示 | BOOT 刷新；DOWN 3s 关机；全刷慢属正常 |

---

## 3. 自检项对照

| 项 | 串口 | 屏幕 |
| --- | --- | --- |
| 电源保持 GPIO17 | ✓ | ✓ |
| 三键 / LED | ✓ | ✓ |
| 电池 ADC | ✓ | ✓ |
| CHRG / STDBY | ✓ | ✓ |
| I2C RTC / NFC / ES8311 | ✓ | ✓ |
| 四色全刷 | — | ✓ |

串口每秒打印按键与外设摘要；I2C 在音频 / NFC 电源打开前后各扫描一次。

---

## 4. 编译 / 刷写

与 Cardputer / Sparks 一样走 PlatformIO（Cursor / CLion 底栏或 CLI）。

| 操作 | 本工程 |
| --- | --- |
| Build | ✅ `pio run`（env `note4c`） |
| Upload（刷固件） | ✅ `pio run -t upload` |
| Upload Filesystem / `uploadfs` | ❌ 当前无 LittleFS `data/` |
| 合并镜像 Release | ✅ 推送 `v*` tag 后由 GitHub Actions 发布 |

```bash
cd firmware
pio run                    # 编译
pio run -t upload          # 刷固件（到货后，USB 数据线）
pio device monitor         # 串口 115200
```

IDE：打开文件夹选到 **`firmware/`** → 环境选 `note4c` → Upload。认不到口时：按住 BOOT → 点一下 RESET → 松 BOOT，再 Upload。

注意：日常 PIO Upload 写的是 **app 分区**，不是 Wiki Demo 那种单个合并 `.bin` 写到 `0x0`。首次刷自研固件前建议先备份整颗 16MB Flash。

Release 产物含 `*-merged.bin`（bootloader + 分区表 + app），可用 `esptool` 写到 `0x0`。详见各 [Release](https://github.com/KyleBing/zectrix-note4c-display/releases) 说明。

---

## 5. 工程结构

```text
firmware/
├── platformio.ini
├── include/
│   ├── app_version.h
│   ├── board.h           # NOTE4C PCB 1.0 引脚
│   ├── epd_ssd2683.h
│   └── font6x8.h
└── src/
    ├── main.cpp          # 自检逻辑 + 状态页
    └── epd_ssd2683.cpp   # 最小四色全刷驱动
```

---

## 6. 相关文档

- [从零开发指南](/guide/getting-started)
- [硬件参数（完整）](/hardware/note4c)
- [硬件速览](/hardware/overview)
