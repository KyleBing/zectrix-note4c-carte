<img width="120" src="design/favicon.svg" alt="LOGO" />

# ZECTRIX NOTE4C Display

仓库按职责拆开：

| 目录 | 内容 |
| --- | --- |
| `firmware/` | PlatformIO 固件工程（硬件自检） |
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

```bash
cd firmware
pio run                   # 编译
pio run -t upload         # 刷写（env: note4c）
pio device monitor        # 115200
```

## Release

推送 `v*` tag 后，GitHub Actions 会编译固件、发布 Release，并触发文档部署。

```bash
git tag v0.1.0
git push origin v0.1.0
```

## 仓库

https://github.com/KyleBing/zectrix-note4c-display
