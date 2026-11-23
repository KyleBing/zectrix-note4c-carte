# 固件功能说明

> **Carte** · ZECTRIX NOTE4C 四色墨水屏开放固件  
> 版本：`{{DOC_VERSION}}` · 工程：`firmware/` · PlatformIO env：`carte`  
> 主调：白底黑字，黄强调，红警示

- 各 App / Web 配置**做什么** → **[App 功能说明](/features/apps)**  
- 按键、刷屏、关机等**为什么会这样** → **[固件行为机制](/features/behavior)**

---

## 1. 定位

日常桌面是四个可开关的 App，用上下键切换；长按上键打开设备自带的 **Web 配置站** 做配网、登录 Portal、管相册和天气城市等。墨水屏只做展示与轻交互，重编辑（尤其相册）在手机/电脑浏览器里完成。

| 能力 | 一句话 |
| --- | --- |
| 多 App | **待办 → 日历 → 天气 → 相册**（可在设置里关掉不用的） |
| 待办 | 从 Portal 拉一篇日记当待办列表 |
| 日历 | 月历 + 农历；可跨日 / 半夜自动换「今天」 |
| 天气 | 和风实况 / 7 日（经 Portal 代理，设备不存 Key） |
| 相册 | 四色照片轮播；Web 里编辑成 `.2bp` 再上传 |
| Web 配置 | 同 WiFi 站点，或热点 `Note4C-Carte` / `12345678` |
| 电源 | 下键长按关机；日历/定时天气·相册可 RTC 深睡醒刷 |

详细用法见 **[App 功能说明](/features/apps)**。

---

## 2. 按键

| 操作 | 行为 |
| --- | --- |
| 短按 **DOWN** | **下一个 App**；连按后静默约 **500ms** 合并为跳过 N 个 |
| 短按 **UP** | **上一个 App**；同样连击合并 |
| 长按 **UP ~1.5s** | 开配置 Web；已在 Web 再长按 → AP ↔ 同 WiFi 站点 |
| 长按 **DOWN ~3s** | **关机**（见行为机制「两种软关机」）；再按 DOWN 开机 |
| 短按 **BOOT** | **页内动作**（各 App 见 [App 功能说明](/features/apps)）；Web 中 **立刻退出** |

### 刷屏中可打断

四色全刷约 **16 秒**。刷新中再按 UP / DOWN / BOOT 会**立刻中断**，按连击切到目标页；**停手后**该页完整刷完并停留。splash、配置说明页、定时唤醒那一刷默认不打断。详见 [行为机制 · 屏幕刷新](/features/behavior#屏幕刷新机制)。

### LED（空闲关闭）

| 图案 | 含义 |
| --- | --- |
| 按下即亮 | 任意键按下灯亮，松开才灭 |
| 上电三闪 | 已开机（默认不刷屏） |
| 长按确认两闪 | UP 开 Web / DOWN 关机阈值到达 |
| 刷完一闪 | 一次全刷**成功结束**（被中断则无此次完成闪） |
| 慢闪 | 正在连 WiFi / 启动 Web |
| 三短闪 | 硬失败 |

设备 Web `/help` 有示意动画。

---

## 3. 编译 / 刷写

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

## 4. 屏幕与字库

| 项 | 说明 |
| --- | --- |
| 面板 | 4.2″ 400×300，黑 / 白 / 红 / 黄（BWRY），SSD2683，2 bpp |
| 刷新 | **仅全刷**，约 16 秒；交互刷屏可被导航键中断 |
| 字库 | 中文 16×16（GB2312/HZK16），ASCII 8×16；未 `uploadfs` 时 CJK 方框 |
| 顶栏电池 | 只画图标，不显示百分比 |

---

## 5. 上电与自检

- 交互开机：**不自动刷屏**，LED 三闪；按键后再刷对应页  
- 新 `APP_BUILD` 首次上电可能先刷 splash（版本 / LOGO），该帧不可打断  
- 启动探测 I2C（RTC / NFC / ES8311）并打串口日志；会话恢复到上次 App（若已禁用则落到第一个启用的）
