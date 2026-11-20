---
layout: home

hero:
  name: Carte
  text: 四色墨水屏开放固件
  tagline: ZECTRIX NOTE4C · ESP32-S3
  image:
    src: /assets/appicon-sketch.svg
    alt: Carte
  actions:
    - theme: brand
      text: 固件功能
      link: /features/
    - theme: alt
      text: 行为机制
      link: /features/behavior
    - theme: alt
      text: 硬件参数
      link: /hardware/note4c
    - theme: alt
      text: GitHub
      link: https://github.com/KyleBing/zectrix-note4c-display

features:
  - title: 多 App + Web 配网
    details: 待办 / 日历 / 天气 / 相册；长按上键开配置站；Portal 登录与和风代理
  - title: 按键与 LED 语义
    details: UP/DOWN 切 App，BOOT 只做页内动作；呼吸刷屏、三短闪失败，详见行为机制
  - title: 错误可追溯
    details: 墨水屏显示错误详情；事件写入 LittleFS，Web /errors 可查看与清空
  - title: 熟悉工具链
    details: PlatformIO + Arduino；硬件参数与刷机约定均可在本站查阅
---

## 当前版本

文档对应固件 **{{DOC_VERSION}}**。

| 项目 | 说明 |
| --- | --- |
| 目标板 | ZECTRIX NOTE4C Devkit（PCB 1.0） |
| 主控 | ESP32-S3 N16R8（16 MB Flash / 8 MB PSRAM） |
| 显示 | 4.2″ 400×300 黑 / 白 / 红 / 黄，SSD2683，2 bpp |
| 固件 | **Carte** · `firmware/` · env `carte` |

```bash
cd firmware
pio run                 # 编译
pio run -t upload       # 刷写
pio device monitor      # 串口 115200
```

更多：[固件功能说明](/features/) · [行为机制](/features/behavior) · [硬件参数](/hardware/note4c)
