# NOTE4C 从零开发指南

> 目标：自己写应用逻辑；显示等底层驱动尽量复用 Wiki 指向的现成方案。  
> 硬件参数见：[硬件参数](/hardware/note4c)（**§2 硬件能力一览**、bpp、四色成像原理）

---

## 1. 先认清「官方」边界

| 你想用的 | 实际情况 |
| --- | --- |
| 官方硬件引脚 / 原理图 / 规格 | ✅ Wiki 有，本文档已整理 |
| NOTE4 消费版 AI / 云固件 | ❌ NOTE4C **没有**，且与四色屏不通用 |
| 官方量产应用固件 | ❌ 无；开发机走开放固件路线 |
| Wiki 收录的「显示可跑通」参考 | ✅ LazyYoun `2bp` 四色 Demo（社区 MIT，非量产固件） |

结论：**从零做产品应用**可以；**屏驱 / 波形 / 2bpp 刷新**优先复用参考工程里的面板代码，不要从 SSD2683 寄存器从零猜。

---

## 2. 和 Sparks / Cardputer 有什么不一样？

| | Sparks（Cardputer） | 本仓库 NOTE4C |
| --- | --- | --- |
| 工程形态 | PlatformIO + **Arduino** | 当前自检固件同样是 **PlatformIO + Arduino** |
| 板级库 | `M5Cardputer` / `M5Unified` 一把梭 | **没有**官方 Arduino 板级库；引脚自己写（`board.h`） |
| 显示 | LCD，刷新快，库现成 | 四色墨水屏 SSD2683，全刷 10s+；完整驱动在社区 **ESP-IDF** 参考仓 |
| 推荐长期路线 | 继续 Arduino/PIO 即可 | 自检 / 按键 / I2C：Arduino 够用；**完整四色屏驱**建议后续接 ESP-IDF 或从参考仓抽组件 |

结论：**不是「必须 Arduino」或「必须 IDF」**——你现在的工具链和 Cardputer 一样是 Arduino/PIO；Wiki 参考 Demo 走 ESP-IDF，是因为屏驱项目用 IDF 写的。本仓库 `firmware/` 先用你熟悉的 Arduino 做硬件自检。

本机检查：能跑 `pio`，有 `framework-arduinoespressif32`；**未找到**独立的 `esp-idf/export.sh` / `idf.py`。若你以为装过 ESP-IDF，多半是 Arduino-ESP32 核心（底层也基于 IDF，但不是原生 `idf.py` 工程）。

---

## 3. 推荐路径（货还在路上也能先做）

```text
阶段 A（无板）  用已有 PlatformIO 编译 firmware/ → 固定 board.h → 对照参考仓屏驱
阶段 B（到货）  备份 16MB Flash → 可选刷历史 Demo 验机 → 再刷 hardware self-test
阶段 C（迭代）  自检通过后 → 再接入四色全刷（IDF 组件或移植驱动）
```

### 3.1 工具链（当前仓库）

- **PlatformIO + Arduino-ESP32**（与 Cardputer 相同）
- 目标：`esp32-s3`，16MB Flash + OPI PSRAM（N16R8）
- 编译：`cd firmware && pio run`
- 刷写：`pio run -t upload`（设备到货后）
- 串口：`pio device monitor`（115200，USB CDC）
- 数据线：必须支持数据，不能只用充电线
- （可选）原生 **ESP-IDF 6.0**：做完整 SSD2683 四色驱动时再装；当前自检不依赖它

### 3.2 本仓库已有自检固件

```text
firmware/
├── platformio.ini
├── include/board.h      # NOTE4C PCB 1.0 引脚
└── src/main.cpp         # 电源保持 / 按键 / ADC / I2C 扫描 / EPD 供电探测
```

串口每秒打印：按键、近似电池电压、充电脚、I2C 上 RTC/NFC/ES8311 是否 ACK、EPD BUSY。  
长按下键约 3 秒会释放 `PWR_HOLD`（电池供电时可能关机）。

**本版已含最小 SSD2683 四色全刷**：开机（及松开 BOOT）把自检结果画到屏上。全刷约 10s+，不要做高频刷新。

### 3.3 参考源码（显示「现成」从这里拿）

```bash
git clone https://github.com/LazyYoun/youn-ink-fourcolor-firmware.git
cd youn-ink-fourcolor-firmware/firmware
# 默认分支 2bp；menuconfig 确认：
#   ZecTrix EPD panel type → 4-color BWRY SSD2683 EPD
#   CONFIG_ZECTRIX_EPD_PANEL_4COLOR_SSD2683=y
```

不要选 1bpp 黑白面板。

复用策略（任选其一）：

1. **最小自建工程**：新建 ESP-IDF 工程，只拷贝 / 组件化参考仓中的 **EPD / SSD2683 / 2bpp framebuffer** 相关文件，应用层自己写  
2. **在参考仓上删减**：先能编译刷写，再砍掉相册/配网/服务端，留下显示 + 电源保持  
3. **其他社区四色 fork**（如 FourColor Ink）同理，仍以 SSD2683 四色为准

### 3.4 仓库结构

```text
zectrix-note4c-display/
├── docs/                     # 文档站（独立 npm / VitePress）
│   ├── package.json
│   ├── .vitepress/
│   ├── features/             # 固件功能说明页
│   ├── guide/
│   └── hardware/
├── firmware/                 # 固件（独立 PlatformIO）
│   ├── platformio.ini
│   ├── include/board.h
│   └── src/main.cpp
├── design/                   # 图标等设计源文件
└── third_party/              # 可选：本地挂参考仓对照屏驱
```

---

## 4. 到货验收顺序

1. USB 插上电脑，确认串口出现  
2. **完整读出并保存 16 MB Flash 备份**（再谈任何刷机）  
3. 可选：刷 Wiki **历史 Demo 参考版**，确认屏、按键、配网大致正常  
4. `cd firmware && pio run -t upload && pio device monitor`  
5. 看串口：RTC=`OK`、按键有变化、BAT 有读数；NFC/ES8311 视供电与地址而定  
6. 出问题先回退到备份 / Demo，再查引脚与电源保持

合并镜像（Demo）写入地址：**`0x0`**。

---

## 5. 现成可用 vs 需自己写

| 模块 | 建议 |
| --- | --- |
| SSD2683 四色驱动 / 全刷 / 2bpp | 复用参考固件（偏 ESP-IDF） |
| 引脚表 / 原理图 | 用 Wiki（已写入硬件参数文档 + `board.h`） |
| ES8311 音频 | I2C 探测先用自检；播放再接组件 / I2S |
| PCF8563 RTC | 标准 I2C，地址 `0x51` |
| Wi-Fi / BLE | Arduino 或 IDF 标准 API 均可 |
| 电源保持 / 充电检测 / 电池 ADC | 自检已接；分压比到货后按原理图校准 |
| NFC | 硬件有（PCB 1.0）；协议与产品逻辑自研 |
| 应用 UI / 业务 / 云 | 全部自研 |

---

## 6. 明确不要做的事

- 把黑白 NOTE4 / 4BP 灰阶固件刷进 NOTE4C  
- 未备份就改分区或刷第三方合并包  
- 在未验证电源保持的情况下只靠电池调试  
- 假设「官方 AI 固件」能直接跑在 NOTE4C 上  

---

## 7. 货到前你可以完成的检查表

- [x] PlatformIO 能编译 `firmware/`（`pio run`）  
- [x] `board.h` 已按 Wiki 写好  
- [ ] 设备到货后备份 16 MB Flash  
- [ ] 刷自检固件，串口确认 RTC / 按键 / 电池  
- [ ] （可选）clone 参考仓，准备后续四色屏驱  
- [ ] （可选）安装原生 ESP-IDF 6.0，仅当要直接编参考仓时  
