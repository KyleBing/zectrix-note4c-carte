#!/usr/bin/env python3
"""本机模拟固件 Web 配置站（不连设备）。

从 firmware/src/app_web.cpp 抽取 CSS，配假数据渲染各页，方便调布局 / 移动端顶栏。

  python3 tools/web_preview.py
  python3 tools/web_preview.py --port 8766 --no-open

相册完整编辑仍用：python3 tools/album_preview.py
"""

from __future__ import annotations

import argparse
import re
import sys
import threading
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse

ROOT = Path(__file__).resolve().parents[1]
WEB_CPP = ROOT / "firmware" / "src" / "app_web.cpp"
VER_H = ROOT / "firmware" / "include" / "app_version.h"
FAVICON = ROOT / "firmware" / "data" / "favicon.svg"

GH = "https://github.com/KyleBing/zectrix-note4c-carte"
GH_SVG = (
    "<svg viewBox='0 0 16 16' width=22 height=22 fill=currentColor aria-hidden=true>"
    "<path d='M8 0C3.58 0 0 3.58 0 8c0 3.54 2.29 6.53 5.47 7.59.4.07.55-.17.55-.38 "
    "0-.19-.01-.82-.01-1.49-2.01.37-2.53-.49-2.69-.94-.09-.23-.48-.94-.82-1.13-.28-.15"
    "-.68-.52-.01-.53.63-.01 1.08.58 1.23.82.72 1.21 1.87.87 2.33.66.07-.52.28-.87.51"
    "-1.07-1.78-.2-3.64-.89-3.64-3.95 0-.87.31-1.59.82-2.15-.08-.2-.36-1.02.08-2.12 0 "
    "0 .67-.21 2.2.82.64-.18 1.32-.27 2-.27.68 0 1.36.09 2 .27 1.53-1.04 2.2-.82 "
    "2.2-.82.44 1.1.16 1.92.08 2.12.51.56.82 1.27.82 2.15 0 3.07-1.87 3.75-3.65 "
    "3.95.29.25.54.73.54 1.48 0 1.07-.01 1.93-.01 2.2 0 .21.15.46.55.38A8.013 8.013 "
    "0 0016 8c0-4.42-3.58-8-8-8z'/></svg>"
)

# path, label, group (sep before group when group changes)
NAV = [
    ("/", "状态", "sys"),
    ("/settings", "设置", "sys"),
    ("/wifi", "WiFi", "sys"),
    ("/auth", "鉴权", "sys"),
    ("/todo", "待办", "app"),
    ("/weather", "天气", "app"),
    ("/album", "相册", "app"),
    ("/chess", "象棋", "app"),
    ("/quote", "名言", "app"),
    ("/schedule", "日程", "misc"),
    ("/errors", "错误", "misc"),
    ("/help", "帮助", "misc"),
]


def extract_raw(src: str, tag: str) -> str:
    start = f'R"{tag}('
    end = f'){tag}"'
    i = src.find(start)
    j = src.find(end, i + len(start)) if i >= 0 else -1
    if i < 0 or j < 0:
        raise SystemExit(f"raw string {tag} not found in {WEB_CPP}")
    return src[i + len(start) : j]


def read_version() -> tuple[str, str]:
    text = VER_H.read_text(encoding="utf-8")
    ver = re.search(r'APP_VERSION\s*=\s*"([^"]+)"', text)
    build = re.search(r"APP_BUILD\s*=\s*(\d+)", text)
    return (ver.group(1) if ver else "0.0.0", build.group(1) if build else "0")


def nav_html(active: str) -> str:
    parts: list[str] = []
    prev = None
    for href, label, group in NAV:
        if prev is not None and group != prev:
            parts.append('<span class=nav-sep aria-hidden=true></span>')
        cls = " class=active" if href == active else ""
        parts.append(f"<a href={href}{cls}>{label}</a>")
        prev = group
    return "".join(parts)


def shell(active: str, body: str, *, body_class: str = "") -> str:
    css = extract_raw(WEB_CPP.read_text(encoding="utf-8"), "CSS")
    ver, build = read_version()
    bc = f" class={body_class}" if body_class else ""
    return (
        "<!DOCTYPE html><html><head><meta charset=utf-8>"
        "<meta name=viewport content='width=device-width,initial-scale=1'>"
        "<link rel=icon href=/favicon.svg type=image/svg+xml>"
        "<title>Carte 配置 · 本机预览</title><style>"
        + css
        + "</style></head><body"
        + bc
        + ">"
        + "<header><div class=brand>"
        "<img class=site-logo src=/favicon.svg alt='' width=28 height=28>"
        "<h1>Carte</h1></div><nav>"
        + nav_html(active)
        + "</nav><div class=meta><span class=ver>"
        + f"{ver}({build})</span>"
        + f"<a class=gh href='{GH}' target=_blank rel=noopener title=GitHub aria-label=GitHub>"
        + GH_SVG
        + "</a></div></header><main>"
        + body
        + "</main></body></html>"
    )


def meter(lab: str, val: str, pct: int, kind: str = "ok", note: str = "") -> str:
    note_h = f'<p class=meter-note>{note}</p>' if note else ""
    return (
        f'<div class=meter><div class=meter-h><span class=lab>{lab}</span>'
        f'<span class=val>{val}</span></div>'
        f'<div class=meter-track><div class="meter-fill {kind}" style="width:{pct}%"></div></div>'
        f"{note_h}</div>"
    )


def page_status() -> str:
    return (
        "<section><div class=status-hero>"
        "<div><span class='pill on'>同 WiFi 站点已连接</span></div>"
        "<div class=ver>本机模拟</div></div>"
        "<dl class=kv>"
        "<dt>模式</dt><dd>STA</dd>"
        "<dt>地址</dt><dd>http://192.168.1.42/</dd>"
        "<dt>网络</dt><dd>HomeWiFi · 192.168.1.42 · -52 dBm</dd>"
        "<dt>Portal</dt><dd>https://kylebing.cn</dd>"
        "<dt>邮箱</dt><dd>demo@example.com</dd>"
        "<dt>Token</dt><dd><span class='pill on'>已保存</span></dd>"
        "<dt>天气</dt><dd>杭州 · via Portal</dd>"
        "<dt>日天气</dt><dd><span class='pill on'>开 · 0天1时0分</span></dd>"
        "<dt>周天气</dt><dd><span class='pill on'>开 · 0天6时0分</span></dd>"
        "<dt>相册</dt><dd>6 张</dd>"
        "<dt>待办来源</dt><dd>本机正文</dd>"
        "<dt>自动关机</dt><dd>300 秒</dd>"
        "<dt>时区</dt><dd>UTC+8:00</dd>"
        "<dt>当前时间</dt><dd>2026-11-26 08:50:00</dd>"
        "<dt>应用</dt><dd>待办 · 日历 · 天气 · 相册 · 状态 · 象棋 · 名言</dd>"
        "</dl></section>"
        "<section><h2>设备</h2><div class=meters>"
        + meter("电池", "3.92 V · 78%", 78, "ok")
        + meter("Heap", "180 / 320 KB 可用", 56, "ok")
        + meter("PSRAM", "6.8 / 8.0 MB", 85, "warn", "偏高属正常（framebuffer）")
        + meter("LittleFS", "1.2 / 3.5 MB", 34, "ok")
        + "</div></section>"
    )


def app_card(name: str, desc: str, on: bool) -> str:
    st = "已启用 · 点击关闭" if on else "已关闭 · 点击启用"
    cls = "on" if on else "off"
    return (
        f"<div class='app-card {cls}'><span class=ico>■</span>"
        f"<p class=name>{name}</p><p class=desc>{desc}</p>"
        f"<span class=st>{st}</span></div>"
    )


def page_settings() -> str:
    apps = [
        ("待办", "日记待办列表", True),
        ("日历", "月历 + 农历", True),
        ("天气", "实况 / 预报", True),
        ("相册", "墨水屏图片", True),
        ("状态", "本机资源一览", True),
        ("象棋", "练习局面", True),
        ("名言", "名言与诗词", False),
    ]
    grid = "".join(app_card(n, d, on) for n, d, on in apps)
    return (
        f"<section><h2><span class=set-step>1</span>应用</h2>"
        f"<p class=hint>点击卡片切换是否在设备上启用。至少保留一个。</p>"
        f"<div class=app-grid>{grid}</div></section>"
        "<section><h2><span class=set-step>2</span>电源</h2>"
        "<p class=hint>无操作自动关机；开启 Web 配置期间不会自动关机。</p>"
        "<label>无操作自动关机</label>"
        "<select><option>关闭</option><option selected>5 分钟</option>"
        "<option>10 分钟</option></select>"
        "<label>连接电源时自动关机</label>"
        "<select><option selected>关闭（接电保持开机）</option>"
        "<option>开启</option></select>"
        "<div class=btn-row><button type=button>保存电源设置</button></div></section>"
        "<section><h2><span class=set-step>3</span>日历</h2>"
        "<label>一周起始</label><select><option selected>周一</option><option>周日</option></select>"
        "<div class=btn-row><button type=button>保存日历设置</button></div></section>"
        "<section><h2><span class=set-step>4</span>时区 / NTP</h2>"
        "<label>时区</label><select><option selected>UTC+8 中国</option></select>"
        "<label>NTP 周期</label><select><option selected>30 天</option></select>"
        "<div class=btn-row><button type=button>保存时区 / 周期</button>"
        "<button type=button class=secondary>立即 NTP 校时</button></div></section>"
    )


def page_wifi() -> str:
    cards = []
    samples = [
        (True, "HomeWiFi", "••••••••"),
        (False, "Office", ""),
        (False, "", ""),
    ]
    for i, (active, ssid, pass_) in enumerate(samples):
        cls = " active" if active else ""
        chk = " checked" if active else ""
        cards.append(
            f"<div class='wifi-card{cls}'><div class=wifi-top>"
            f"<label class=wifi-active><input type=radio name=active{chk}> Active</label>"
            f"<span class=wifi-idx>#{i+1}</span></div>"
            f'<input class=wifi-ssid value="{ssid}" placeholder="SSID">'
            f'<input class=wifi-pass type=password value="{pass_}" placeholder="密码">'
            f"</div>"
        )
    return (
        "<section><h2>WiFi</h2>"
        "<p class=hint>最多 5 条。大字为 SSID，下方小字为密码。</p>"
        + "".join(cards)
        + "<div class=btn-row><button type=button>保存 WiFi</button></div></section>"
    )


def page_auth() -> str:
    return (
        "<section><h2>鉴权</h2>"
        "<p class=hint>登录 Portal 后保存 JWT；业务响应带续签头时会自动更新。</p>"
        "<div class='set-status'><span class=lbl>状态</span>"
        "<span class=name>已登录</span><code class=id>demo@example.com</code></div>"
        "<label>Portal</label><input value='https://kylebing.cn'>"
        "<label>邮箱</label><input value='demo@example.com'>"
        "<label>密码</label><input type=password placeholder='••••••••'>"
        "<div class=btn-row><button type=button>登录并保存</button>"
        "<button type=button class=secondary>退出登录</button></div></section>"
    )


def page_todo() -> str:
    return (
        "<section><h2><span class=set-step>1</span>数据源</h2>"
        "<p class=hint>本机正文，或 Portal 网络日记。</p>"
        "<label class='check-row on'><input type=checkbox checked>"
        "<span>使用本机正文（不拉网络）</span></label>"
        "<div class=btn-row><button type=button>保存数据源</button></div></section>"
        "<section><h2><span class=set-step>2</span>本机正文</h2>"
        "<p class=hint>每行一项；<code>- </code>未完成、<code>= </code>已完成。</p>"
        "<label>正文</label>"
        "<textarea class=todo-src>- 买菜 #晚上\n- 写固件文档\n= 修顶栏布局</textarea>"
        "<div class=btn-row><button type=button>保存本机待办</button></div>"
        "<p class=meta style=margin-top:14px>本机预览</p>"
        "<ul class=todo-list>"
        "<li><span class=box></span><div class=txt>买菜<span class=note>晚上</span></div></li>"
        "<li><span class=box></span><div class=txt>写固件文档</div></li>"
        "<li class=done><span class=box></span><div class=txt>修顶栏布局</div></li>"
        "</ul></section>"
    )


def page_weather() -> str:
    return (
        "<section><h2><span class=set-step>1</span>数据源</h2>"
        "<label class='check-row on'><input type=checkbox checked><span>启用天气</span></label>"
        "<label class='check-row on'><input type=radio checked><span>Portal 代理（JWT）</span></label>"
        "<label class='check-row'><input type=radio><span>和风普通 API Key</span></label>"
        "<div class=btn-row><button type=button>保存数据源</button></div></section>"
        "<section><h2><span class=set-step>2</span>位置</h2>"
        "<div class='set-status'><span class=lbl>当前</span>"
        "<span class=name>杭州</span><code class=id>101210101</code></div>"
        "<label>搜索城市 / 区县</label>"
        "<div class=wx-search-row><input placeholder='例如 杭州 或 西湖' value='杭州'>"
        "<button type=button>搜索</button></div>"
        "<ul class=geo-list>"
        "<li><label><input type=radio checked><span><strong>杭州</strong> "
        "<code>101210101</code> <span class='pill on'>当前</span>"
        "<span class=sub>浙江 · 杭州 · 中国</span></span></label></li>"
        "<li><label><input type=radio><span><strong>西湖</strong> "
        "<code>101210106</code><span class=sub>浙江 · 杭州 · 中国</span></span></label></li>"
        "</ul>"
        "<div class=btn-row><button type=button>设为当前位置</button></div></section>"
        "<section><h2><span class=set-step>3</span>日天气定时</h2>"
        "<p class=hint>停留在 <strong>NOW</strong> 时按此间隔拉实况/小时。</p>"
        "<label class='check-row on'><input type=checkbox checked>"
        "<span>启用定时刷新</span></label>"
        "<div class=btn-row style=align-items:flex-end;gap:10px>"
        "<label style=margin:0>天<br><input type=number value=0 style=width:5.5em></label>"
        "<label style=margin:0>时<br><input type=number value=1 style=width:5.5em></label>"
        "<label style=margin:0>分<br><input type=number value=0 style=width:5.5em></label>"
        "</div><div class=btn-row><button type=button>保存日天气定时</button></div></section>"
        "<section><h2><span class=set-step>4</span>周天气定时</h2>"
        "<p class=hint>停留在 <strong>7D</strong> 时按此间隔拉七日预报（建议更长）。</p>"
        "<label class='check-row on'><input type=checkbox checked>"
        "<span>启用定时刷新</span></label>"
        "<div class=btn-row style=align-items:flex-end;gap:10px>"
        "<label style=margin:0>天<br><input type=number value=0 style=width:5.5em></label>"
        "<label style=margin:0>时<br><input type=number value=6 style=width:5.5em></label>"
        "<label style=margin:0>分<br><input type=number value=0 style=width:5.5em></label>"
        "</div><div class=btn-row><button type=button>保存周天气定时</button></div></section>"
    )


def page_album() -> str:
    return (
        "<section><h2>相册</h2>"
        "<p class=hint>本页仅为布局占位。完整选图 / 抖动 / 色阶编辑请运行：</p>"
        "<pre>python3 tools/album_preview.py</pre>"
        "<div class=btn-row><button type=button class=secondary disabled>编辑器未接入本预览</button></div>"
        "</section>"
        "<section><h2>展示设置（示意）</h2>"
        "<label class='check-row on'><input type=checkbox checked><span>显示标题栏</span></label>"
        "<label>播放顺序</label><select><option>顺序</option><option selected>乱序</option></select>"
        "<div class=btn-row><button type=button>保存</button></div></section>"
    )


def page_chess() -> str:
    levels = [("开局", True), ("战术", True), ("中局", False), ("残局", True)]
    checks = "".join(
        f"<label class='check-row{' on' if on else ''}' style=margin:0>"
        f"<input type=checkbox{' checked' if on else ''}><span>{name}</span></label>"
        for name, on in levels
    )
    return (
        "<section><h2><span class=set-step>1</span>练习内容</h2>"
        "<p class=hint>播放顺序与局面等级；等级可多选。</p>"
        "<label>播放顺序</label><select><option selected>顺序</option><option>乱序</option></select>"
        f"<label>内容等级</label><div class=btn-row style=flex-wrap:wrap;gap:8px>{checks}</div>"
        "<div class=btn-row><button type=button>保存练习内容</button></div></section>"
        "<section><h2><span class=set-step>2</span>定时翻局</h2>"
        "<label class='check-row'><input type=checkbox><span>启用定时翻局</span></label>"
        "<div class=btn-row><button type=button>保存定时设置</button></div></section>"
    )


def page_quote() -> str:
    return (
        "<section><h2>名言</h2>"
        "<p class=hint>播放顺序与列表；设备 BOOT 翻下一条。</p>"
        "<label>播放顺序</label><select><option>顺序</option><option selected>乱序</option></select>"
        "<label class='check-row on'><input type=checkbox checked><span>启用定时翻条</span></label>"
        "<div class=quote-layout style=margin-top:14px>"
        "<div class=quote-side><div class=quote-side-h>列表 · 3</div>"
        "<ul class=quote-list>"
        "<li><button type=button class=on><span class=q-i>#1</span>"
        "<span class=q-t>Stay hungry, stay foolish.</span>"
        "<span class=q-a>Steve Jobs</span></button></li>"
        "<li><button type=button><span class=q-i>#2</span>"
        "<span class=q-t>海内存知己，天涯若比邻。</span>"
        "<span class=q-a>王勃</span></button></li>"
        "<li><button type=button><span class=q-i>#3</span>"
        "<span class=q-t>Done is better than perfect.</span>"
        "<span class=q-a>—</span></button></li>"
        "</ul></div>"
        "<div class=quote-editor>"
        "<p class=q-ed-title>编辑 #1</p>"
        "<p class=q-ed-hint>正文与作者</p>"
        "<label>正文</label><textarea>Stay hungry, stay foolish.</textarea>"
        "<label>作者</label><input value='Steve Jobs'>"
        "<div class=btn-row><button type=button>保存本条</button>"
        "<button type=button class=danger>删除</button></div>"
        "</div></div></section>"
    )


def page_schedule() -> str:
    return (
        "<section><h2><span class=set-step>1</span>静默时段</h2>"
        "<p class=hint>静默内不作定时动作；结束早于开始表示跨午夜。</p>"
        "<label class='check-row on'><input type=checkbox checked><span>启用静默</span></label>"
        "<div class='sched-row quiet'>"
        "<label>开始<br><input value='22:00'></label>"
        "<label>结束<br><input value='06:00'></label></div>"
        "<div class=btn-row><button type=button>保存静默时段</button></div></section>"
        "<section><h2><span class=set-step>2</span>每日 App 时间表</h2>"
        "<label class='check-row on'><input type=checkbox checked><span>启用时间表</span></label>"
        "<div class=sched-row>"
        "<label>时间<br><input value='08:00'></label>"
        "<label>App<br><select><option>天气</option><option selected>待办</option></select></label>"
        "<label>界面<br><select><option selected>默认</option></select></label></div>"
        "<div class=btn-row><button type=button>保存 App 时间表</button></div></section>"
        "<section><h2><span class=set-step>3</span>一日时间轴</h2>"
        "<div class=sched-day>"
        "<span class=q style='left:0;width:25%'></span>"
        "<span class=q style='left:91.67%;width:8.33%'></span>"
        "<span class=m style='left:33.33%' title='待办'></span>"
        "<div class=lab><span>00:00</span><span>06:00</span><span>12:00</span>"
        "<span>18:00</span><span>24:00</span></div></div>"
        "<p class='hint sm'>红块=静默；黑竖线=切 App。</p></section>"
    )


def page_errors() -> str:
    return (
        "<section><div class=status-hero><div><h2 style=margin:0>错误记录</h2>"
        "<p class=hint style=margin:6px 0 0>设备端最近事件示意。</p></div>"
        "<span class='pill warn'>2 条</span></div>"
        "<ul class=err-list>"
        "<li><div class=err-top><span class=err-src>wifi</span>"
        "<span class=err-when>2026-11-26 08:12:03</span></div>"
        "<pre class=err-msg>STA connect timeout (HomeWiFi)</pre></li>"
        "<li><div class=err-top><span class=err-src>weather</span>"
        "<span class=err-when>2026-11-25 22:01:11</span></div>"
        "<pre class=err-msg>HTTP 401 · token expired</pre></li>"
        "</ul>"
        "<div class=btn-row style=margin-top:16px>"
        "<button type=button class=danger>清空记录</button></div></section>"
    )


def page_help() -> str:
    rows = [
        ("led-anim-solid", "常亮", "按键按下期间灯一直亮着。", "BOOT / UP / DOWN"),
        ("led-anim-boot", "上电三闪", "快闪 3 下后熄灭。", "开机 / 深睡唤醒"),
        ("led-anim-ack", "长按确认两闪", "长按阈值到达时闪两下。", "UP≈1.5s 开 Web"),
        ("led-anim-slow", "慢闪", "约 400ms 亮灭交替。", "连 WiFi / 开配置网页"),
        ("led-anim-fast", "快闪", "约 120ms 亮灭交替。", "HTTPS 请求中"),
        ("off", "熄灭", "无图案时灯关闭。", "空闲待机"),
    ]
    items = "".join(
        f"<li><div class=led-dot-wrap><span class='led-dot {anim}'></span></div>"
        f"<div class=led-meta><p class=name>{name}</p><p class=desc>{desc}</p>"
        f"<span class=when>{when}</span></div></li>"
        for anim, name, desc, when in rows
    )
    return (
        "<section><h2>帮助</h2>"
        "<p class=hint style=margin:0>长按上键约 1.5s 开配置网页；长按下键约 2s 关机。</p></section>"
        f"<section><h2>LED 含义</h2><ul class=led-list>{items}</ul></section>"
    )


PAGES = {
    "/": page_status,
    "/settings": page_settings,
    "/wifi": page_wifi,
    "/auth": page_auth,
    "/todo": page_todo,
    "/weather": page_weather,
    "/album": page_album,
    "/chess": page_chess,
    "/quote": page_quote,
    "/schedule": page_schedule,
    "/errors": page_errors,
    "/help": page_help,
}

BODY_CLASS = {
    "/album": "page-album",
    "/quote": "page-quote",
}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt: str, *args) -> None:
        sys.stderr.write("%s - %s\n" % (self.address_string(), fmt % args))

    def _send(self, code: int, body: bytes, ctype: str) -> None:
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:
        u = urlparse(self.path)
        if u.path == "/favicon.svg":
            self._send(200, FAVICON.read_bytes(), "image/svg+xml")
            return
        path = u.path if u.path in PAGES else "/"
        if u.path not in PAGES and u.path != "/":
            body = "<section><h2>404</h2><p class=hint>本机预览无此页。</p></section>"
            html = shell("/", body)
            self._send(404, html.encode("utf-8"), "text/html; charset=utf-8")
            return
        html = shell(path, PAGES[path](), body_class=BODY_CLASS.get(path, ""))
        self._send(200, html.encode("utf-8"), "text/html; charset=utf-8")

    def do_POST(self) -> None:
        length = int(self.headers.get("Content-Length", 0))
        if length:
            self.rfile.read(length)
        loc = urlparse(self.path).path or "/"
        self.send_response(303)
        self.send_header("Location", loc)
        self.end_headers()


def main() -> None:
    ap = argparse.ArgumentParser(description="本机模拟固件 Web 配置站")
    ap.add_argument("--port", type=int, default=8766)
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--no-open", action="store_true")
    args = ap.parse_args()
    # 启动时校验 CSS 可抽取
    extract_raw(WEB_CPP.read_text(encoding="utf-8"), "CSS")
    httpd = ThreadingHTTPServer((args.host, args.port), Handler)
    url = f"http://{args.host}:{args.port}/"
    print(f"web preview  {url}")
    print("用浏览器 DevTools 切设备宽度即可。Ctrl+C 结束。")
    if not args.no_open:
        threading.Timer(0.4, lambda: webbrowser.open(url)).start()
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print()
        httpd.shutdown()


if __name__ == "__main__":
    main()
