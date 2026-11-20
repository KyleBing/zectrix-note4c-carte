<img width="120" src="design/favicon.svg" alt="LOGO" />

# Carte

面向 **ZECTRIX NOTE4C** 四色墨水屏的开放固件（多 App + Web 配网）。

仓库按职责拆开：

| 目录 | 内容 |
| --- | --- |
| `firmware/` | PlatformIO 固件工程 **Carte**（env：`carte`） |
| `docs/` | VitePress 文档站（独立 npm 工程） |
| `design/` | 图标等设计源文件 |

线上文档：[GitHub Pages](https://kylebing.github.io/zectrix-note4c-display/)

## 文档

```bash
cd docs
npm install
npm run dev          # http://localhost:3124
npm run build
```

| 页面 | 说明 |
| --- | --- |
| [固件功能](docs/features/index.md) | 当前自检能力、屏幕行为、刷写 |
| [从零开发](docs/guide/getting-started.md) | 工具链、验收顺序、复用策略 |
| [硬件参数](docs/hardware/note4c.md) | 完整规格 / 引脚 / bpp / BWRY |
| [硬件速览](docs/hardware/overview.md) | 预研速查与 Demo 约定 |

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

https://github.com/KyleBing/zectrix-note4c-display
