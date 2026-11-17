# ZECTRIX NOTE4C 硬件参数文档

> 适用范围：四色屏 **ZECTRIX NOTE4C 开发套件**，PCB **版本 1.0**  
> 主控：ESP32-S3 **N16R8**（16 MB Flash / 8 MB PSRAM）  
> 面板：4.2 英寸 400×300 黑 / 白 / 红 / 黄（BWRY）电子墨水屏，控制器 **SSD2683**，像素格式 **2 bpp**  
> 资料来源：[ZECTRIX Wiki · NOTE4C 开发指南](https://wiki.zectrix.com/zh/hardware/note4c/quick-start)，以及社区参考固件板级头文件 `config.h`（与 Wiki 引脚一致）  
> 整理日期：2026-11-17（货未到前的预研文档；到货后请对照实物与原理图复核）

---

## 1. 产品与定位

| 项目 | 说明 |
| --- | --- |
| 产品名 | ZECTRIX NOTE4C Devkit |
| PCB | 1.0（含 NFC；未来国际消费量产版可能取消 NFC） |
| 形态 | 已组装成品：屏 + 控制板 + 电池 + 外壳 + 按键 + 磁吸背板 |
| 与 NOTE4 关系 | **同一控制板 / 按键 / 音频 / 电源 / USB / 外壳**；**显示面板与固件镜像不可混用** |
| 官方消费固件 | **不含** NOTE4 消费版固件、官方 AI 语音任务、NOTE4 云服务 |
| 公开 Demo | Wiki 收录的是社区参考固件（LazyYoun `2bp`），供学习与二次开发 |

典型用途：四色看板 / 电子标签 / 照片卡片 / 语音交互终端 / NFC 触发 / 低功耗 IoT 面板。

---

## 2. 整机规格

| 项目 | 规格 |
| --- | --- |
| 主控 | ESP32-S3 N16R8 |
| Flash / PSRAM | 16 MB / 8 MB |
| 无线 | 2.4 GHz Wi-Fi、Bluetooth LE 5.0 |
| 显示 | 4.2" 四色 EPD，400 × 300，黑白红黄 |
| 显示控制器 | SSD2683 |
| 像素格式 | 2 bpp BWRY |
| 刷新 | 全屏刷新；单次通常 **≥ 10 s**（闪烁 / 短暂反色属正常） |
| 音频 | ES8311 + 麦克风 + 扬声器 |
| RTC | PCF8563（I2C `0x51`） |
| NFC | PCB 1.0 含；参考驱动中型号标注为 GT23SC6699（I2C `0x55`） |
| 按键 | 上、下/电源、BOOT/确认 + 内凹 RESET |
| USB | Type-C：供电、原生 USB 数据、刷写、串口 |
| 电池 | 2000+ mAh 可充 |
| 尺寸 | 97 × 97 × 8.75 mm（含磁吸后盖） |
| 外壳 | ABS |
| 逻辑电平 | **3.3 V**（GPIO 禁止直接接 5 V） |

包装通常含：NOTE4C 本体、桌面支架；Demo 固件 / 源码 / 资料在线提供。

---

## 3. 按键与外观接口

屏幕朝向使用者时：

| 位置 | 硬件信号 | 说明 |
| --- | --- | --- |
| 右侧上键 | GPIO39 | 上键，低电平有效 |
| 右侧下键 | GPIO18 | 下键 / 电源相关，低电平有效；与电源键复用 |
| 正面圆形键 | GPIO0 | BOOT / 确认，低电平有效；启动配置脚 |
| 圆形键旁小孔 | 麦克风 | 接 ES8311 音频通路 |
| 扬声器孔列最右独立小孔 | GPIO3 | 状态 LED（绿色） |
| 左侧内凹针孔 | EN / RST | 硬件复位，**不擦固件/用户数据** |
| 底部中央 Type-C | D−=GPIO19，D+=GPIO20 | 供电 / USB / 刷写 / 串口 |

示意图（Wiki）：https://wiki.zectrix.com/note4c-devkit-controls-ports-en.png

---

## 4. 电源与复位要点

| 信号 | GPIO | 说明 |
| --- | --- | --- |
| 电源保持（锁存） | **17** | 电池供电固件启动后必须维持所需锁存状态 |
| 下键 / 电源键 | **18** | 开/关机、长按行为由固件定义 |
| 硬件 RESET | EN/RST 针孔 | 仅复位主控 |
| 充满 / 待机 | 1 | `STDBY_H` |
| 充电状态 | 2 | `CHRG_L`（参考固件：低电平表示充电中） |
| 电池电压 ADC | 4 | 电阻分压采样 |

**注意：** 墨水屏断电后仍保留最后一帧画面；有画面 ≠ 设备仍在运行。

---

## 5. 进入下载模式（ROM Download）

多数电脑可直接 USB 识别。若浏览器刷机工具或 `esptool` 找不到设备：

1. 保持 Type-C **数据线**连接（充电线无串口）
2. 按住正面 **BOOT（GPIO0）**
3. 保持 BOOT，短按一次左侧 **RESET**
4. 松开 BOOT
5. 重新连接并选择新出现的 ESP32-S3 串口

---

## 6. 完整 GPIO / 总线映射

### 6.1 电子墨水屏（SPI + 控制脚）

控制器侧信号与 NOTE4 共用；**面板与波形 / 固件配置不同**。

| 功能 | GPIO | 备注 |
| --- | --- | --- |
| EPD 电源使能 | 6 | |
| BUSY | 8 | |
| RESET | 9 | |
| DC | 10 | |
| CS | 11 | |
| SCK | 12 | |
| MOSI / SDA | 13 | |
| SPI Host（参考固件） | — | `SPI3_HOST` |
| 分辨率（参考固件） | — | 400 × 300 |
| 镜像 / 旋转（参考固件默认） | — | Mirror X/Y = false，Swap XY = false，Offset = 0 |

### 6.2 按键、LED、电池、充电、NFC、电源

| 功能 | GPIO | 说明 |
| --- | --- | --- |
| BOOT / 确认 | 0 | 低有效；启动配置 |
| 充满 / 待机 | 1 | `STDBY_H` |
| 充电状态 | 2 | `CHRG_L` |
| 状态 LED | 3 | 绿色 |
| 电池电压 ADC | 4 | 分压 |
| RTC 中断 | 5 | PCF8563 INT |
| NFC 场检测 | 7 | 参考固件：有效电平 0 |
| 电源保持 | 17 | 主板电源锁存 |
| 下键 / 电源 | 18 | 低有效 |
| NFC 电源控制 | 21 | NFC 掉电 / 使能 |
| 上键 | 39 | 低有效 |

### 6.3 音频（ES8311 + I2S）

| 功能 | GPIO |
| --- | --- |
| I2S MCLK | 14 |
| I2S BCLK / SCLK | 15 |
| Codec DOUT → ESP RX (DIN) | 16 |
| I2S LRCK / WS | 38 |
| 音频电源使能 | 42 |
| Codec DIN ← ESP TX (DOUT) | 45 |
| 扬声器功放控制 (PA) | 46 |

参考固件采样率：输入 / 输出均为 **16000 Hz**。  
ES8311 控制走下方 I2C；地址为芯片默认地址。

### 6.4 I2C 外设

| 信号 / 设备 | 值 |
| --- | --- |
| SDA | GPIO47 |
| SCL | GPIO48 |
| PCF8563 RTC | `0x51` |
| NFC | `0x55` |

### 6.5 USB 与 UART

| 功能 | GPIO |
| --- | --- |
| USB D− | 19 |
| USB D+ | 20 |
| UART0 TX | 43 |
| UART0 RX | 44 |

---

## 7. 显示子系统（从零开发必读）

| 项目 | 值 |
| --- | --- |
| 面板 | 4.2" 四色 BWRY |
| 控制器 | SSD2683 |
| 分辨率 | 400 × 300 |
| 像素格式 | **2 bpp**（黑 / 白 / 红 / 黄） |
| 刷新特性 | 全刷为主；单次常 ≥ 10 s |
| 官方公开 datasheet | Wiki **未**托管厂商官网 SSD2683 手册；以原理图映射 + 公开驱动源码为准 |
| 与 NOTE4 | NOTE4 为黑白 / 灰阶另一套面板与镜像；**禁止混刷** |

**推荐复用（“显示现成拿来用”）：**

1. Wiki 官方收录的社区参考工程（MIT）：  
   https://github.com/LazyYoun/youn-ink-fourcolor-firmware （默认分支 `2bp`）
2. 板级定义：`firmware/main/boards/zectrix-s3-epaper-4.2/`（含 `config.h`、`custom_lcd_display.*`、电源 / RTC 等）
3. menuconfig 面板选项：`CONFIG_ZECTRIX_EPD_PANEL_4COLOR_SSD2683=y`  
   （勿选 1 bpp 黑白面板）
4. 面板初始化参考文件（仓库内）：`firmware/4D2_BWRY_400x300_2683_test(2)(1).c`
5. 构建环境：Wiki / 上游文档当前写的是 **ESP-IDF 6.0**，目标 `esp32s3`

**音频现成组件：** Espressif Component Registry 的 ES8311 组件 + Wiki 列出的 ES8311 / PCF8563 / ESP32-S3 器件资料。

---

## 8. 刷机与备份约定

| 项目 | 约定 |
| --- | --- |
| 合并镜像写入地址 | **`0x0`**（不要写到 `0x20000`） |
| 刷写前 | 完整备份 **16 MB** 原厂 Flash |
| 线缆 | 必须数据线；纯充电线无串口 |
| 浏览器工具 | Chrome / Edge + ZECTRIX Web 刷机工具 |
| CLI | `esptool --chip esp32s3 write-flash 0x0 <merge.bin>` |
| Wi-Fi | 仅 **2.4 GHz** |

历史 Demo 参考镜像（操作说明对应此版，到货后可用于亮屏验证）：

| 项目 | 值 |
| --- | --- |
| 文件名 | `notellm-2bp-v6.5.9-2bp-3fbbabb-20260705-mergebin.bin` |
| 大小 | 3,054,752 字节 |
| SHA-256 | `26f209f7cdea8f62bff73f2e877e3ab421917c17497a2a23340129c6ef43fc36` |
| 写入地址 | `0x0000` |

另有上游提交 `8907e593` 的测试构建；Wiki 标明 **未做 NOTE4C 真机验证**，建议先历史版，再视情况试新版。

---

## 9. 原理图与官方链接

| 资源 | 链接 |
| --- | --- |
| NOTE4C 开发指南（本机主要依据） | https://wiki.zectrix.com/zh/hardware/note4c/quick-start |
| V1.0 共用控制板原理图 PDF | 见 Wiki「原理图」小节下载（NOTE4 / NOTE4C 控制板共用） |
| 社区四色参考固件 | https://github.com/LazyYoun/youn-ink-fourcolor-firmware |
| ESP32-S3 数据手册 | Espressif 官网 |
| PCF8563 | NXP |
| ES8311 | Everest / Espressif Component Registry |

---

## 10. 从零开发时的硬件检查清单（货到后）

- [ ] 确认外壳 / 丝印为 **NOTE4C 四色**，不是黑白 NOTE4  
- [ ] USB 数据线可枚举串口；必要时 BOOT+RESET 进下载模式  
- [ ] 完整读出并保存 16 MB Flash 备份  
- [ ] 可选：先刷历史 Demo 合并镜像验证屏、键、Wi-Fi  
- [ ] 自研工程：`set-target esp32s3`，EPD 选 **SSD2683 四色**，引脚按本文第 6 节  
- [ ] 电池运行时确认 **GPIO17 电源保持** 行为正确  
- [ ] 勿向 GPIO 输入 5 V；勿混用 NOTE4 黑白固件  

---

## 11. 修订记录

| 日期 | 说明 |
| --- | --- |
| 2026-11-17 | 初版：根据 Wiki NOTE4C 指南 + 社区 `config.h` 整理；设备尚未到货，待实物复核 |
