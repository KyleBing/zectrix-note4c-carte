# NOTE4C 固件（待办看板）

PlatformIO + Arduino。**四色墨水屏待办页** + **WiFi / Web 配网鉴权**（portal-go diary）。

完整说明：[固件功能](../docs/features/index.md)

## 编译 / 刷写

| 操作 | 说明 |
| --- | --- |
| Build | `pio run`（env **`note4c`**） |
| Upload | `pio run -t upload` |
| Upload FS | `pio run -t uploadfs`（`data/` 字库，**首次必做**） |

```bash
cd firmware
pio run
pio run -t upload
pio run -t uploadfs
pio device monitor   # 115200
```

## 按键

- **BOOT** 松开：拉日记并全刷
- **UP** 短按：开配置 Web（有 WiFi→STA，否则 AP）
- **UP** 长按 ~1.5s：强制 AP（`note4c` / `note4c4c`，密码须 ≥8 位）
- **DOWN** 长按 ~3s：释放电源保持
- **LED**：空闲灭；慢闪=连 WiFi；快闪=拉取；双脉冲=Web 就绪；三短闪=失败

## Web

`/` 状态 · `/wifi` 多 WiFi · `/auth` 登录与 diary id · `/todo` 预览

JWT：登录保存；业务响应若带 `X-Access-Token` 则自动更新（portal-go 剩余 &lt; 7 天续签）。
