# Changelog

## Unreleased

- Web 相册：原图 / 四色预览不再用滚轮缩放（页内滚动、滑条缩放仍可用）
- Web 顶栏右侧增加 GitHub 仓库链接
- 仓库更名为 [zectrix-note4c-carte](https://github.com/KyleBing/zectrix-note4c-carte)
- 看门狗：长操作结束不再误开 loop/idle1 TWDT（Arduino 默认只盯 CPU0 idle）；超时 5s→30s；404 不再解析 HTML。修 12H 记完错误仍复位
- 天气改为和风官方路径：`/v7/weather/now`、`/v7/weather/24h`、`/v7/weather/7d|15d`、`/geo/v2/city/lookup`（不再打自造的 `/weather/hourly`）
- 天气：兼容无 `success`、顶层即和风 payload；失败日志带 HTTP / qcode，便于区分服务端失败与看门狗
- Web：LED 并入 **帮助** 页（`/help`，旧 `/led` 跳转）；相册「设备显示标题栏」改为醒目开关；原图滚轮缩放、空格+拖动平移（类 PS）
- 文档：[固件行为机制](docs/features/behavior.md)（按键 / Web / 错误日志等运行时说明）
- 错误事件环（最多 40 条）写入 LittleFS；墨水屏显示「错误详情」；Web **/错误** 页可查看/清空
- Web 退出 / BOOT：当前 App 在配置里被关掉后，落到第一个可用 App，不再把会话还原到已禁用项
- 仅 1 个可显示 App 时不画顶部 indicator
- 天气：流式解析 Portal JSON、兼容 code/字段数字类型；拉失败仍刷屏（避免灯三短闪像没反应）
- 天气：header 的 OK 不再盖住右侧 app indicator；页内 NOW/12H/7D/14D 黄标切换；BOOT 切模式先落盘再拉预报
- Web 新增 **LED** 页（`/led`）：各闪烁图案示意动画 + 含义说明
- 固件正式命名 **Carte**（Web 标题 / env `carte` / AP `Note4C-Carte` / Release `note4c-carte-*`）；NVS 从 `note4c` 自动迁移
- Web：状态与设置拆分；设置页卡片开关各 App
- 关机改为深睡，按下键（GPIO18）即可唤醒开机；避免 USB 供电时卡在死循环、只能按 RESET
- 上电不自动刷屏，LED 快闪 3 下表示开机；按键后再刷新页面
- 顶栏 App 色点：未选 3×7 / 选中 3×11，间隔 2px；footer 等腰三角 + 方底居中
- 天气 header 用中文字库（修 ???）；BOOT 切模式只要刷屏成功即反馈，标题带当前 mode
- 上下键切 App 改为与相册 BOOT 相同的连击超时（500ms 静默后一次跳过 N 个）
- 充电指示对齐 Cardputer：左侧黄闪电 + 黄电池框（EPD 无绿），几何同 `drawIconChargingBolt`；CHRG 内部上拉防浮空误显
- 日历年月在 header；日期格内大字公历 + 小字农历
- 日历：大字公历 + 小字农历；周字与横线 +3px；Web 可配周起始 / 时区 / NTP 周期 / 手动设时
- NTP：复位后校一次，平时按周期（默认 30 天，可选 14/7）；RTC 无效时也会校
- 新增日历 App

## v0.0.15

- 相册上限 30 张；每张可勾选「展示」（默认开），取消后文件保留、设备翻页跳过
- 相册编辑首屏：左右图等大保比例尽量铺满，工具栏分组，色盘纯色块无字；相册列表沉到首屏外
- 侧栏色阶/映射可单独复位

## v0.0.14

- 相册预览四色改为墨水色 `#701B0E` / `#F6C749`；取色放大镜只在左右上角，挡住取样点才换边，中心像素绿框
- 画笔（四色）+ 橡皮改四色成品，支持像素缩放；工具选中纯色底；橡皮滑条同行独立一组
- 相册页去圆角卡片：编辑区一屏铺满，色阶在侧栏，相册列表沉底
- 任意按键按下灯亮、松开灯灭；刷屏用呼吸灯
- 无操作自动关机可在 Web 状态页配置（配置网页期间不关）；配置屏说明改为中文

## v0.0.13

- 已存相册照片可下载为 PNG（设备上的 `.2bp` 在浏览器解码）

## v0.0.12

- 相册编辑：源图/预览对齐；左侧选图与已存缩略图（可再编辑覆盖）
- 色阶改为直方图 + 黑/灰/白三点；取色放大镜躲鼠标；橡皮擦

## v0.0.11

- 相册编辑页电脑端全宽：源图 / 四色预览并排，色阶与取色在侧栏

## v0.0.10

- Web `/album` 照片编辑：缩放平移裁切、整体色阶、近黑/近白拉纯、黄红偏向、取色映射到四色
- 已存照片改为 1:1 缩略图列表；上限 10 张

## v0.0.9

- 天气列表：12H / 7D / 14D 一行一条；日期格式化为 `MM-DD`（日预报带星期），时间 `MM-DD HH:MM`
- 14 天列表用小字以塞满行；实况仍为单页详情

## v0.0.8

- 修复 BOOT 处理后同一轮 loop 仍处理上键短按、误切回 TODO 的快捷键竞态
- BOOT 按住期间冻结上/下键；松开后 `return` 跳过本帧 UP/DOWN；抑制窗加长到 2s
- 当前 App / 天气模式落盘，复位后恢复（避免 BOOT 若触发重启也回到 TODO）

## v0.0.7

- 去掉设备端 **Diary** App 槽位；现为 TODO / WEATHER / ALBUM（`n/3`）
- 刷屏时下键只响应长按关机、不记短按；BOOT 后忽略上下键
- BOOT 处理结束强制校验仍停留在原 App

## v0.0.6

- 下键短按：往下翻 App；上键短按：往上翻
- BOOT 刷屏后丢弃误触短按；天气末档 **14D**

## v0.0.5

- 顶栏显示当前 App 槽位 `n/N`（电池左侧）
- **相册**：Web `/album` 页内浏览器缩放 + Floyd–Steinberg 四色抖动，上传专用 `.2bp`（400×300×2bpp，30KB）
- 设备相册 App：读 LittleFS `/album/*.2bp` 全刷显示；BOOT 翻下一张；最多 12 张
- 固件不解 JPG；状态页显示相册占用张数

## v0.0.4

- BOOT 明确为 **App 内功能键**（天气切模式 / 待办·日记刷新 / 相册翻页；Web 中退出回 App）
- 天气经 Portal `/weather/hourly` 支持 12 小时预报；模式 NOW → 12H → 7D → 15D（本版末档后改为 14D 见 v0.0.6）

## v0.0.3

- 天气改走 Portal `/weather/*` 代理和风（设备只配位置 + 登录 Token，不再存和风 Key）
- Web `/weather`：启用开关 + 经 Portal geo 搜城选 LocationID

## v0.0.2

- 多 App：上下短按切换 TODO / DIARY / WEATHER / ALBUM；长按上=Web，长按下=关机
- 和风天气初版：Web 配 Host/Key + GeoAPI 选位置；BOOT 切换 NOW/TODAY/7D/15D 按需拉取
- 待办间距：主项与 note 2px，项与项 4px；顶栏电池不显示百分比
- 主调：白底黑字 / 黄强调 / 红警示
- LED：空闲灭；慢闪连 WiFi；快闪拉取/刷屏；双脉冲 Web 就绪；三短闪失败

## v0.2.0

- 待办看板：按配置的 diary id 拉取 portal-go 日记并全刷墨水屏
- Web 配网（STA / AP），多 WiFi 档案自动连接
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
