# ZECTRIX NOTE4C Display

ZECTRIX NOTE4C 四色墨水屏开放固件与文档。

- **固件**：`firmware/` — PlatformIO + Arduino 硬件自检（串口 + SSD2683 四色状态页）
- **文档**：VitePress → [GitHub Pages](https://kylebing.github.io/zectrix-note4c-display/)

## 快速开始

```bash
# 文档
npm install
npm run docs:dev          # http://localhost:3124

# 固件
cd firmware
pio run                   # 编译
pio run -t upload         # 刷写（env: note4c）
pio device monitor        # 115200
```

## 文档入口

| 页面 | 说明 |
| --- | --- |
| [固件功能](docs/firmware/features.md) | 当前自检能力、屏幕行为、刷写 |
| [从零开发](docs/guide/getting-started.md) | 工具链、验收顺序、复用策略 |
| [硬件参数](docs/hardware/note4c.md) | 完整规格 / 引脚 / bpp / BWRY |
| [硬件速览](docs/hardware/overview.md) | 预研速查与 Demo 约定 |

## Release

推送 `v*` tag 后，GitHub Actions 会：

1. 编译 `note4c` 固件并合并 `*-merged.bin`
2. 创建 GitHub Release
3. 触发文档站重新部署

```bash
git tag v0.1.0
git push origin v0.1.0
```

## 仓库

https://github.com/KyleBing/zectrix-note4c-display
