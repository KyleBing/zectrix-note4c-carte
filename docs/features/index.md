# 固件功能说明

> **Carte** · ZECTRIX NOTE4C 四色墨水屏开放固件  
> 版本：`{{DOC_VERSION}}` · 工程：`firmware/` · PlatformIO env：`carte`  
> 主调：白底黑字，黄强调，红警示

运行时「按下去为什么会这样」、以及**屏幕怎么刷、能不能中途打断**，见 **[固件行为机制](/features/behavior)**。

---

## 1. 定位

日常主功能：多 App 桌面 + 设备端 Web 配网 / 鉴权；待办从 [portal-go](https://github.com/KyleBing/portal-go) 拉指定日记；天气经 Portal 代理和风；相册在手机/电脑上抖动成 `.2bp` 后上传到设备。

| 能力 | 说明 |
| --- | --- |
| 多 App | 启用顺序循环：**TODO → CALENDAR → WEATHER → ALBUM**；顶栏色点指示（仅启用数 ≥ 2 时绘制） |
| 待办 | Portal 日记：`-` 未完成 / `=` 已完成（未完成在前）；BOOT 重新拉取 |
| 日历 | 格内大字公历 + 小字农历；今天红底白字；醒着跨日重绘；关机前台为日历时约次日 0 点 RTC 深睡唤醒刷新 |
| 天气 | Portal 代理和风；BOOT 切 **NOW / 7D**；独立「定时刷新」开关 + 间隔（醒着 / 关机均可） |
| 相册 | Web `/album` 编辑上传 `.2bp`；展示开关、批量导入导出删除；独立「定时翻页」；BOOT 下一张 |
| 时钟 | PCF8563；复位后联网校一次，平时按周期 NTP（默认可配）；可离线手设 |
| 配网 Web | 同 WiFi 站点，或 AP 热点 `Note4C-Carte` / 密码 `12345678`（`http://192.168.4.1`） |
| 多 WiFi | 最多 5 组；按 Active → 其余顺序自动连接 |
| Portal | 登录 JWT 落盘；响应头 `X-Access-Token`（剩余 &lt; 7 天）自动续写 |
| 中文 | LittleFS：`HZK16` + `ASC16` + `uni2gb.bin`（首次需 `uploadfs`） |
| 错误可查 | 屏上「错误详情」；事件环最多 40 条，Web `/errors` 查看 / 清空 |

---

## 2. 按键

| 操作 | 行为 |
| --- | --- |
| 短按 **DOWN** | **下一个 App**；连按后静默约 **500ms** 合并为跳过 N 个 |
| 短按 **UP** | **上一个 App**；同样连击合并 |
| 长按 **UP ~1.5s** | 开配置 Web；已在 Web 再长按 → AP ↔ 同 WiFi 站点 |
| 长按 **DOWN ~3s** | **关机**（见行为机制「两种软关机」）；再按 DOWN 开机 |
| 短按 **BOOT** | **从不切 App**：页内动作（见下）；Web 中 **立刻退出** 回 App |

### BOOT 页内动作

| App | BOOT |
| --- | --- |
| 待办 | 重新拉 Portal 并全刷 |
| 日历 | 重绘（必要时校时） |
| 天气 | NOW ↔ 7D，再拉数全刷 |
| 相册 | 下一张（仅「展示中」列表） |

### 刷屏中可打断

四色全刷约 **10s+**。刷新过程中再按导航键（UP / DOWN / BOOT）会**立刻中断当前全刷**，按连击结果切到目标页；**停手后**目标页会完整刷完并停留。开机 splash、配置页画面、定时唤醒那一刷默认不打断。细节见 [行为机制 · 屏幕刷新](/features/behavior#屏幕刷新机制)。

### LED（空闲关闭）

设备 Web **帮助**（`/help`）有示意动画。摘要：

| 图案 | 含义 |
| --- | --- |
| 按下即亮 | 任意键按下灯亮，松开才灭 |
| 上电三闪 | 已开机（默认不刷屏） |
| 长按确认两闪 | UP 开 Web / DOWN 关机阈值到达 |
| 刷完一闪 | 一次全刷**成功结束**（过程中不闪；被中断则不会为此次刷屏亮完成闪） |
| 慢闪 | 正在连 WiFi / 启动 Web |
| 三短闪 | 硬失败 |

---

## 3. Web 配置

墨水屏配置页会显示模式图标与 URL。浏览器打开后：

| 路径 | 用途 |
| --- | --- |
| `/` | 状态：联网 / 配置摘要；存储·内存·固件·电池 |
| `/settings` | App 开关、空闲关机、日历 / 时区 / 手动时间 |
| `/help` | 帮助（含 LED 图案） |
| `/wifi` | 最多 5 条 WiFi，选 Active |
| `/auth` | Portal Base URL、邮箱登录、Diary ID |
| `/todo` | 在线预览待办正文 |
| `/weather` | 启用 + 城市搜索选 LocationID；定时刷新开关 / 间隔 |
| `/album` | 选图编辑抖动上传；批量导入 / 导出 / 删除；定时翻页；设备是否叠标题栏 |
| `/errors` | 错误事件（最多 40 条，可清空） |

默认 Portal Base URL：`https://kylebing.cn/portal`。

和风 Key 在 **portal-go** 服务端；设备用已登录 Token 访问 `/portal/v7/weather/now|24h|7d`、`/portal/geo/v2/city/lookup` 等。

保存配置一般不自动退出 Web；退出靠 **BOOT**（或关机）。

---

## 4. 编译 / 刷写

```bash
# 仓库根目录（推荐）
./flash.sh        # 编译并刷固件
./flash.sh all    # 固件 + LittleFS（首次必做）
./flash.sh mon    # 串口 115200

# 或
cd firmware
pio run
pio run -t upload
pio run -t uploadfs
pio device monitor   # 115200
```

认不到口：按住 BOOT → 点 RESET → 松 BOOT，再 Upload。分区与地址见 [`firmware/README.md`](https://github.com/KyleBing/zectrix-note4c-carte/blob/main/firmware/README.md)。

---

## 5. 屏幕与字库

| 项 | 说明 |
| --- | --- |
| 面板 | 4.2″ 400×300，黑 / 白 / 红 / 黄（BWRY），SSD2683，2 bpp |
| 刷新 | **仅全刷**，约 10s+；可被导航键中断（见上） |
| 字库 | 中文 16×16（GB2312/HZK16），ASCII 8×16；未 `uploadfs` 时 CJK 方框，ASCII 可 6×8 回退 |
| 顶栏电池 | 只画图标，不显示百分比 |

---

## 6. 上电与自检

- 交互开机：**不自动刷屏**，LED 三闪；按键后再刷对应页  
- 新 `APP_BUILD` 首次上电可能先刷一帧 splash（版本 / LOGO），该帧不可打断  
- 启动仍探测 I2C（RTC / NFC / ES8311）并打串口日志；默认会话进第一个启用的 App（通常为 TODO）
