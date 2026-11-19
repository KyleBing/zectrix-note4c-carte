# Changelog

## v0.2.0

- 待办看板：按配置的 diary id 拉取 portal-go 日记并全刷墨水屏
- Web 配网（STA / AP `note4c`），多 WiFi 档案自动连接
- Portal 登录鉴权，JWT 落盘；`X-Access-Token` 自动续签
- LittleFS 中文字库（HZK16 + ASC16 + uni2gb）

## v0.1.1

- 文档站独立为 `docs/` npm 工程，与 `firmware/` 目录职责分离
- 固件功能页路径改为 `/features/`
- 使用 `favicon.svg`（小图）与 `appicon-sketch.svg`（首页大图）

## v0.1.0

- 初始 NOTE4C 硬件自检固件（串口 + 四色状态页）
- VitePress 文档站（硬件参数 / 从零开发 / 固件功能）
- GitHub Actions：Release 自动打包 + Pages 部署
