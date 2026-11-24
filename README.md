<img width="120" src="design/favicon.svg" alt="LOGO" />


#  Carte
面向 **ZECTRIX NOTE4C** 四色墨水屏的开放固件（多 App + Web 配网）。

## 一、主要功能
- 相册
- 日历
- 设备状态（内存 / 定时刷新等）
- 国际象棋练习（开局 / 战术 / 中局 / 残局）
- 名言警句（内置种子 + Web 左列表右编辑；可定时翻条）
- 日程（定时刷新总览、静默时段、每日 App 时间表）
- web配置页面
- 待办事项（Portal 日记，或本机正文；Diary 暂未免费公开）
- 天气预报(基于 Diary，暂未免费公开)
    - 未来 6 小时
    - 未来 7 天


## 二、一些实际显示效果

<img width="6000" height="4000" alt="2026-11-20 zectrix note4c-天气预报-06" src="https://github.com/user-attachments/assets/5aa36b6a-1ee2-4a6b-ae88-1ceaf5656505" />
<img width="6000" height="4000" alt="2026-11-19 zectrix note4c-06" src="https://github.com/user-attachments/assets/4e880ca3-fbe2-4deb-9fbc-f8948c13cea9" />
<img width="6000" height="4000" alt="2026-11-22 zectrix note4c -相册-03" src="https://github.com/user-attachments/assets/6b17bb4b-0f9d-43ae-86e5-d1f2cf849fcc" />
<img width="6000" height="4000" alt="2026-11-22 zectrix note4c -相册-04" src="https://github.com/user-attachments/assets/b3b12a39-48a4-4533-8a2c-0f4f44525c62" />
<img width="6000" height="4000" alt="2026-11-22 zectrix note4c -相册-05" src="https://github.com/user-attachments/assets/57fcd833-bdf9-4c5c-9fa5-058503b24ed8" />
<img width="6000" height="4000" alt="2026-11-22 zectrix note4c -相册-06" src="https://github.com/user-attachments/assets/be8a6da3-a726-4f33-af7f-13a222460af5" />
<img width="6000" height="4000" alt="2026-11-22 zectrix note4c -相册-07" src="https://github.com/user-attachments/assets/adc7c4f0-3294-4d87-9ca9-b4892612e9db" />
<img width="6000" height="4000" alt="2026-11-22 zectrix note4c -相册-08" src="https://github.com/user-attachments/assets/ad611321-0ca5-444c-ab16-9073da9164a4" />
<img width="6000" height="4000" alt="2026-11-22 zectrix note4c -相册-09" src="https://github.com/user-attachments/assets/b00dd909-a683-433c-8a3c-f13778f46a08" />
<img width="6000" height="4000" alt="2026-11-22 zectrix note4c -相册-10" src="https://github.com/user-attachments/assets/ad5cd7ed-94bf-46ae-b1db-c4dd372983b3" />
<img width="6000" height="4000" alt="2026-11-22 zectrix note4c -相册-12" src="https://github.com/user-attachments/assets/290b722f-d515-432d-9e8e-94607d7dfa4a" />
<img width="6000" height="4000" alt="2026-11-22 zectrix note4c -相册-13" src="https://github.com/user-attachments/assets/e9549886-67ab-4cda-9f91-fa8e5d5620cf" />
<img width="6000" height="4000" alt="2026-11-22 zectrix note4c -相册-14" src="https://github.com/user-attachments/assets/e23ce7b4-7bcb-4fd0-b18f-27843af02d4c" />
<img width="6000" height="4000" alt="2026-11-22 zectrix note4c -相册-15" src="https://github.com/user-attachments/assets/6d5bcdb9-9f70-4a8d-9dba-69ee7ed0c8e2" />
<img width="6000" height="4000" alt="2026-11-22 zectrix note4c -相册-17" src="https://github.com/user-attachments/assets/3cd2457c-f245-4eaa-88cf-05f6e27e8573" />



仓库按职责拆开：

| 目录 | 内容 |
| --- | --- |
| `firmware/` | PlatformIO 固件工程 **Carte**（env：`carte`） |
| `docs/` | VitePress 文档站（独立 npm 工程） |
| `design/` | 图标等设计源文件 |

线上文档：[GitHub Pages](https://kylebing.github.io/zectrix-note4c-carte/)

## 三、文档

```bash
cd docs
npm install
npm run dev          # http://localhost:3124
npm run build
```

| 页面 | 说明 |
| --- | --- |
| [固件功能](docs/features/index.md) | 总览、按键、刷写与字库 |
| [App 功能](docs/features/apps.md) | 待办 / 日历 / 天气 / 相册 / 状态 / 象棋 / 名言 / 日程 / Web |
| [行为机制](docs/features/behavior.md) | 上电/按键/屏幕刷新（可中断全刷）/错误日志 |
| [硬件参数](docs/hardware/note4c.md) | 完整规格 / 引脚 / bpp / BWRY |

## 四、固件

芯片 ESP32-S3 **16 MB Flash**，分区表 `default_16MB.csv`。详情与刷写地址见 [firmware/README.md](firmware/README.md)。

```bash
./flash.sh                # 编译并刷固件（推荐）
./flash.sh all            # 固件 + LittleFS 字库（首次）
./flash.sh mon            # 串口 115200

# 或手动：
cd firmware
pio run                   # 编译
pio run -t upload         # 刷写（env: carte）→ app @ 0x10000
pio run -t uploadfs       # 字库 LittleFS @ 0xC90000（首次必做）
pio device monitor        # 115200

# 本机预览相册编辑页（测抖动/出图，不连设备）
python3 tools/album_preview.py
```

| 区 | 地址 | 大小 |
| --- | --- | --- |
| bootloader | `0x0` | 32 KB |
| 分区表 | `0x8000` | 4 KB |
| nvs | `0x9000` | 20 KB |
| otadata | `0xE000` | 8 KB |
| **app0（固件）** | **`0x10000`** | **6.25 MiB** |
| app1（OTA 备用） | `0x650000` | 6.25 MiB |
| **LittleFS（字库/相册）** | **`0xC90000`** | **3.375 MiB** |
| coredump | `0xFF0000` | 64 KB |

整片 `0x1000000`（16 MiB）。
## 五、Release

推送 `v*` tag 后，GitHub Actions 会编译固件、发布 Release，并触发文档部署。

```bash
git tag v0.1.0
git push origin v0.1.0
```

## 六、仓库

https://github.com/KyleBing/zectrix-note4c-carte
