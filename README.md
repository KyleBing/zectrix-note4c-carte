<img width="120" src="design/favicon.svg" alt="LOGO" />

<img width="6000" height="4000" alt="2026-11-20 zectrix note4c-天气预报-06" src="https://github.com/user-attachments/assets/5aa36b6a-1ee2-4a6b-ae88-1ceaf5656505" />

<img width="6000" height="4000" alt="2026-11-19 zectrix note4c-06" src="https://github.com/user-attachments/assets/4e880ca3-fbe2-4deb-9fbc-f8948c13cea9" />



# Carte

面向 **ZECTRIX NOTE4C** 四色墨水屏的开放固件（多 App + Web 配网）。

仓库按职责拆开：

| 目录 | 内容 |
| --- | --- |
| `firmware/` | PlatformIO 固件工程 **Carte**（env：`carte`） |
| `docs/` | VitePress 文档站（独立 npm 工程） |
| `design/` | 图标等设计源文件 |

线上文档：[GitHub Pages](https://kylebing.github.io/zectrix-note4c-carte/)

## 文档

```bash
cd docs
npm install
npm run dev          # http://localhost:3124
npm run build
```

| 页面 | 说明 |
| --- | --- |
| [固件功能](docs/features/index.md) | 能力表、按键、Web 路径、刷写与字库 |
| [行为机制](docs/features/behavior.md) | 上电/按键/屏幕刷新（可中断全刷）/App/Web/错误日志 |
| [硬件参数](docs/hardware/note4c.md) | 完整规格 / 引脚 / bpp / BWRY |

## 固件

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
## Release

推送 `v*` tag 后，GitHub Actions 会编译固件、发布 Release，并触发文档部署。

```bash
git tag v0.1.0
git push origin v0.1.0
```

## 仓库

https://github.com/KyleBing/zectrix-note4c-carte
