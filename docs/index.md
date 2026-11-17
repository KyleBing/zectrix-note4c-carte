---
layout: home

hero:
  name: NOTE4C Display
  text: 四色墨水屏开放固件
  tagline: ZECTRIX NOTE4C · ESP32-S3 · SSD2683 BWRY · PlatformIO Arduino
  image:
    src: /assets/appicon-sketch.svg
    alt: NOTE4C
  actions:
    - theme: brand
      text: 固件功能
      link: /features/
    - theme: alt
      text: 从零开发
      link: /guide/getting-started
    - theme: alt
      text: 硬件参数
      link: /hardware/note4c
    - theme: alt
      text: GitHub
      link: https://github.com/KyleBing/zectrix-note4c-display

features:
  - title: 硬件自检
    details: 串口 + 四色屏状态页：按键、电池、充电脚、I2C（RTC / NFC / ES8311）、内存
  - title: 四色全刷
    details: SSD2683 400×300 BWRY，最小 Arduino 驱动；开机与短按 BOOT 刷新状态页
  - title: 熟悉工具链
    details: PlatformIO + Arduino，与 Cardputer / Sparks 同一套编译 / 刷写流程
  - title: 文档齐全
    details: 硬件参数、引脚表、从零开发路径与刷机约定，全部可在本站查阅
---

## 当前版本

文档对应固件 **{{DOC_VERSION}}**。

| 项目 | 说明 |
| --- | --- |
| 目标板 | ZECTRIX NOTE4C Devkit（PCB 1.0） |
| 主控 | ESP32-S3 N16R8（16 MB Flash / 8 MB PSRAM） |
| 显示 | 4.2″ 400×300 黑 / 白 / 红 / 黄，SSD2683，2 bpp |
| 工程 | `firmware/` · env `note4c` |

```bash
cd firmware
pio run                 # 编译
pio run -t upload       # 刷写
pio device monitor      # 串口 115200
```

更多：[固件功能说明](/features/) · [从零开发指南](/guide/getting-started) · [硬件参数](/hardware/note4c)
