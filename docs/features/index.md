# 固件功能说明

> 当前固件：**待办看板**（portal-go diary）+ **WiFi / Web 配网鉴权** + 四色墨水屏  
> 版本：`{{DOC_VERSION}}` · 工程目录：`firmware/` · PlatformIO env：`note4c`

---

## 1. 定位

到货验收之后的主功能：从 [portal-go](https://github.com/KyleBing/portal-go) 拉取**指定 id 的日记**，全刷到 NOTE4C 四色屏；WiFi 与登录鉴权通过设备上的 Web 配置页完成（交互参考 Cardputer Sparks）。

| 能力 | 说明 |
| --- | --- |
| 多 WiFi | 最多 5 组 SSID；按 Active → 其余顺序自动连接 |
| 配网 Web | STA 同网或 AP 热点 `note4c` / `note4c4c`（`http://192.168.4.1`；密码须 ≥8 位） |
| Portal 登录 | `POST /portal/user/login`，JWT 落盘 |
| Token 续签 | 响应头 `X-Access-Token`（剩余 &lt; 7 天）自动写回 |
| 待办页 | `GET /portal/diary/detail?diaryId=`；按 diary 规则 `-` 未完成 / `=` 已完成（未完成在前，勾选框 + 删除线） |
| 中文显示 | LittleFS：`HZK16` + `ASC16` + `uni2gb.bin` |

---

## 2. 按键

| 操作 | 行为 |
| --- | --- |
| 短按松开 **BOOT** | 连网拉取 diary 并全刷待办页（约 10s+） |
| 短按松开 **UP** | 启动配置 Web（有 WiFi 则 STA，否则 AP） |
| 长按 **UP ~1.5s** | 强制 AP 配网（`note4c` / `note4c4c`） |
| 长按 **DOWN ~3s** | **关机**：释放 `PWR_HOLD`（刷屏 / 连网中也可；仅电池供电时真正断电，USB 供电可能仍亮） |

### LED（空闲关闭）

| 图案 | 含义 |
| --- | --- |
| 灭 | 空闲 / 无事 |
| 慢闪 | 正在连 WiFi / 启动 Web |
| 快闪 | 拉取日记 / 刷屏 |
| 双脉冲 | 配置 Web 已就绪（AP 或 STA） |
| 三短闪 | 失败 |

---

## 3. Web 配置

浏览器打开设备显示的 URL（STA IP 或 `http://192.168.4.1`）：

| 路径 | 用途 |
| --- | --- |
| `/` | 状态：模式、IP、Token、Diary ID |
| `/wifi` | 编辑最多 5 条 WiFi，选 Active |
| `/auth` | Portal Base URL、邮箱登录、Diary ID；密码仅本次提交不落盘 |
| `/todo` | 在线预览拉取到的日记正文 |

默认 Base URL：`https://kylebing.cn/portal`（与 portal-go 文档一致）。

鉴权约定见 portal-go `docs/api.md`：`Authorization: Bearer <jwt>`；快过期时服务端在 `X-Access-Token` 下发新 JWT。

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
- 未刷字库时 CJK 会显示为方框，ASCII 仍可用 6×8 回退

---

## 6. 自检残留

启动时仍会探测 I2C（RTC / NFC / ES8311）并打串口日志；主界面已改为待办页，不再默认刷硬件色条自检页。
