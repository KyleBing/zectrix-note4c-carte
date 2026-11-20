# 固件功能说明

> 当前固件：**多 App**（待办 / 日记 / 天气 / 相册占位）+ **WiFi / Web 配网鉴权** + 和风天气 + 四色墨水屏  
> 主调：白底黑字，黄强调，红警示  
> 固件名：**Carte** · 版本：`{{DOC_VERSION}}` · 工程目录：`firmware/` · PlatformIO env：`carte`

运行时按键 / Web / 错误日志等「为什么会这样」见 **[固件行为机制](/features/behavior)**。

---

## 1. 定位

到货验收之后的主功能：从 [portal-go](https://github.com/KyleBing/portal-go) 拉取**指定 id 的日记**（待办 / 日记 App）；和风天气按配置拉取实况与预报；WiFi 与鉴权通过设备 Web 配置页完成。

| 能力 | 说明 |
| --- | --- |
| 多 App | 上下短按切换：TODO → CALENDAR → WEATHER → ALBUM；顶栏单行色点（白 3×7 / 选中 3×11，间隔 2px） |
| 日历 | header 显示年月；格内大字公历 + 小字农历；今天红底白字；Web 可配周起始 / 时区 / 手动时间 |
| 时钟 | PCF8563；复位后联网校一次，平时按周期 NTP（默认 30 天）；可离线手设 |
| 相册 | Web `/album` 页内编辑（裁切/色阶/取色映射）后抖动上传 `.2bp`；BOOT 翻页；最多 30 张；可取消「展示」但仍保留文件 |
| 多 WiFi | 最多 5 组 SSID；按 Active → 其余顺序自动连接 |
| 配网 Web | STA 同网或 AP 热点 `Note4C-Carte` / `Note4C-Carte`（`http://192.168.4.1`） |
| Portal 登录 | `POST /portal/user/login`，JWT 落盘 |
| Token 续签 | 响应头 `X-Access-Token`（剩余 &lt; 7 天）自动写回 |
| 待办页 | `GET /portal/diary/detail?diaryId=`；`-` 未完成 / `=` 已完成（未完成在前） |
| 天气 | 经 Portal `/weather/*` 代理和风；设备只配位置；BOOT 切换 NOW / 12H / 7D / 14D |
| 中文显示 | LittleFS：`HZK16` + `ASC16` + `uni2gb.bin` |

---

## 2. 按键

| 操作 | 行为 |
| --- | --- |
| 短按松开 **DOWN** | **下一个 App**；连按后停约 500ms 再跳过 N 个（同相册 BOOT） |
| 短按松开 **UP** | **上一个 App**；连按同样超时合并 |
| 长按 **UP ~1.5s** | 启动配置 Web（已在 Web 再长按 → 强制 AP） |
| 长按 **DOWN ~3s** | **关机**：释放 `PWR_HOLD` 并深睡；再按下键唤醒开机 |
| 关机后按 **DOWN** | **开机 / 唤醒**（含 USB 仍供电时的假关机） |
| 短按松开 **BOOT** | **仅 App 内功能键**：天气切 NOW→12H→7D→14D；待办/日历刷新；相册下一张；Web 中退出回 App |

### LED（空闲关闭）

设备 Web 导航 **LED**（`/led`）有完整示意动画说明。摘要：

| 图案 | 含义 |
| --- | --- |
| 按下即亮 | 任意键按下灯亮，松开才灭（含长按） |
| 上电三闪 | 已开机（不刷屏） |
| 长按确认两闪 | UP 开 Web / DOWN 关机阈值到达 |
| 呼吸 | 墨水屏刷新页面 |
| 慢闪 | 正在连 WiFi / 启动 Web |
| 三短闪 | 失败 |

---

## 3. Web 配置

浏览器打开设备显示的 URL（STA IP 或 `http://192.168.4.1`）：

| 路径 | 用途 |
| --- | --- |
| `/` | 状态：模式、IP、Token、Diary、天气、错误条数 |
| `/settings` | App 开关、电源 / 日历时间 |
| `/led` | LED 图案说明 |
| `/wifi` | 编辑最多 5 条 WiFi，选 Active |
| `/auth` | Portal Base URL、邮箱登录、Diary ID |
| `/todo` | 在线预览拉取到的日记正文 |
| `/weather` | 启用开关 + 城市搜索选 LocationID（经 Portal geo） |
| `/album` | 选图 → 页内抖动成 `.2bp` → 上传 / 列表 / 删除 |
| `/errors` | 过往错误事件（最多 40 条，可清空） |

默认 Portal Base URL：`https://kylebing.cn/portal`。

和风 JWT/Key 在 **portal-go** 服务端；设备用已登录 Token 调 `/portal/weather/now|hourly|daily|geo`。

---

## 4. 编译 / 刷写

```bash
cd firmware
pio run                         # 编译
pio run -t upload               # 刷固件
pio run -t uploadfs             # 刷 LittleFS 字库（首次必做）
pio device monitor              # 115200
```

| 操作 | 本工程 |
| --- | --- |
| Build / Upload | ✅ |
| Upload Filesystem (`data/`) | ✅ 字库：HZK16 / ASC16 / uni2gb.bin |
| 合并镜像 Release | ✅ `v*` tag |

认不到口时：按住 BOOT → 点 RESET → 松 BOOT，再 Upload。

---

## 5. 屏幕与字库

- 全刷约 **10s+**，属正常；勿高频刷新
- 中文 16×16（GB2312/HZK16），ASCII 8×16；需先 `uploadfs`
- 顶栏电池只画图标，不显示百分比（方便拍照）
- 未刷字库时 CJK 会显示为方框，ASCII 仍可用 6×8 回退

---

## 6. 自检残留

启动时仍会探测 I2C（RTC / NFC / ES8311）并打串口日志；主界面为多 App，默认进 TODO。
