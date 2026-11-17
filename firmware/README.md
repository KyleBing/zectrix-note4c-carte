# NOTE4C 硬件自检固件

PlatformIO + Arduino。串口 + **四色墨水屏状态页**。

完整说明见文档站：[固件功能](../docs/features/index.md)（线上：https://kylebing.github.io/zectrix-note4c-display/features/）。

## 编译 / 刷写

和 Cardputer / Sparks 一样走 PlatformIO（Cursor / CLion 底栏或 CLI）。

| 你以前在 Cardputer 做的 | 本工程 |
| --- | --- |
| Build | ✅ 相同 |
| Upload（刷固件） | ✅ 相同 → 选 env **`note4c`** |
| Upload Filesystem / `uploadfs`（写 `data/`） | ❌ 当前**没有** LittleFS `data/`，自检不需要 |
| M5Burner 合并包 | ❌ 不用；PIO Upload 即可 |

```bash
cd firmware
pio run                    # 编译
pio run -t upload          # 刷固件（到货后，USB 数据线）
pio device monitor         # 串口 115200
```

IDE：打开文件夹选到 **`firmware/`**（或工作区里打开该 PIO 工程）→ 环境选 `note4c` → 闪电 **Upload**。认不到口时：按住 BOOT → 点一下 RESET → 松 BOOT，再 Upload。

注意：这是 **app 分区写入**，不是 Wiki Demo 那种单个合并 `.bin` 写到 `0x0`。首次刷自研固件前建议先备份整颗 16MB Flash。

## 屏幕行为

- 开机采样硬件状态，全刷一页（约 10 秒以上，属正常）
- 顶部四色色条：黑 / 白 / 红 / 黄
- 正文：按键、电池、充电脚、I2C（RTC/NFC/ES8311）、内存
- **短按后松开 BOOT**：再刷一页最新状态（勿每秒全刷）
- **长按 DOWN ~3s**：释放电源保持（电池可能关机）

屏驱序列来自社区参考仓四色路径（MIT），本仓库做了最小 Arduino 移植：`src/epd_ssd2683.cpp`。

## 自检项

| 项 | 串口 | 屏幕 |
| --- | --- | --- |
| 电源保持 GPIO17 | ✓ | ✓ |
| 三键 / LED | ✓ | ✓ |
| 电池 ADC | ✓ | ✓ |
| CHRG / STDBY | ✓ | ✓ |
| I2C RTC/NFC/ES8311 | ✓ | ✓ |
| 四色全刷 | — | ✓ |
