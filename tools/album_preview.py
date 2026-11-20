#!/usr/bin/env python3
"""Serve the firmware album editor in a desktop browser (no device / WiFi).

Reads CSS + HTML/JS from firmware/src so the preview matches the device page.
Image convert / dither / color replace all run in the browser; 上传/另存 stay
in this process memory (you can re-open them, or download PNG from the list).

  python3 tools/album_preview.py
  python3 tools/album_preview.py --port 8765 --no-open
"""

from __future__ import annotations

import argparse
import json
import re
import sys
import threading
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

ROOT = Path(__file__).resolve().parents[1]
WEB_CPP = ROOT / "firmware" / "src" / "app_web.cpp"
ALBUM_CPP = ROOT / "firmware" / "src" / "app_web_album.cpp"
FAVICON = ROOT / "firmware" / "data" / "favicon.svg"
ALBUM_MAX = 30
FRAME = 30000

STRING_RE = re.compile(r'"(?:\\.|[^"\\])*"')
ESCAPES = {"n": "\n", "t": "\t", "r": "\r", "\\": "\\", '"': '"', "'": "'"}


def decode_cpp_string(lit: str) -> str:
    body = lit[1:-1]
    out: list[str] = []
    i = 0
    while i < len(body):
        if body[i] == "\\" and i + 1 < len(body):
            out.append(ESCAPES.get(body[i + 1], body[i + 1]))
            i += 2
        else:
            out.append(body[i])
            i += 1
    return "".join(out)


def extract_quoted(chunk: str) -> str:
    parts: list[str] = []
    for m in STRING_RE.finditer(chunk):
        s = decode_cpp_string(m.group(0))
        if s == " checked":
            continue
        parts.append(s)
    return "".join(parts)


def extract_raw(src: str, tag: str) -> str:
    start = f'R"{tag}('
    end = f'){tag}"'
    i = src.find(start)
    j = src.find(end, i + len(start)) if i >= 0 else -1
    if i < 0 or j < 0:
        raise SystemExit(f"raw string {tag} not found")
    return src[i + len(start) : j]


def build_album_html() -> str:
    css = extract_raw(WEB_CPP.read_text(encoding="utf-8"), "CSS")
    album = ALBUM_CPP.read_text(encoding="utf-8")
    fn = album.find("void webAlbumAppendPage")
    js_at = album.find('R"ALBUMJS(', fn)
    if fn < 0 or js_at < 0:
        raise SystemExit("webAlbumAppendPage / ALBUMJS not found")
    html_src = album[fn:js_at].replace("String(ALBUM_MAX)", '"30"')
    body = extract_quoted(html_src)
    script = extract_raw(album, "ALBUMJS")
    return (
        "<!DOCTYPE html><html><head><meta charset=utf-8>"
        "<meta name=viewport content='width=device-width,initial-scale=1'>"
        "<link rel=icon href=/favicon.svg type=image/svg+xml>"
        "<title>Carte 相册 · 本机预览</title><style>"
        + css
        + "</style></head><body class=page-album>"
        "<header><div class=brand>"
        "<img class=site-logo src=/favicon.svg alt='' width=28 height=28>"
        "<h1>Carte</h1></div><nav>"
        '<a href="/" class=active>相册预览</a>'
        "</nav><span style='margin-left:auto;font-size:12px;opacity:.65'>"
        "本机预览 · 不连设备</span></header><main>"
        + body
        + script
        + "</main></body></html>"
    )


class AlbumStore:
    def __init__(self) -> None:
        self.show_header = False
        self.files: list[tuple[str, bytes, bool]] = []  # name, payload, shown

    def list_json(self) -> bytes:
        return json.dumps(
            {
                "files": [{"name": n, "show": s} for n, _, s in self.files],
                "count": len(self.files),
                "max": ALBUM_MAX,
                "show_header": self.show_header,
            }
        ).encode()

    def get(self, name: str) -> bytes | None:
        for n, data, _ in self.files:
            if n == name:
                return data
        return None

    def delete(self, name: str) -> bool:
        before = len(self.files)
        self.files = [it for it in self.files if it[0] != name]
        return len(self.files) < before

    def set_shown(self, name: str, show: bool) -> bool:
        for i, (n, data, _) in enumerate(self.files):
            if n == name:
                self.files[i] = (n, data, show)
                return True
        return False

    def set_shown_all(self, show: bool) -> None:
        self.files = [(n, d, show) for n, d, _ in self.files]

    def next_name(self) -> str:
        used = {n for n, _, _ in self.files}
        for i in range(1, ALBUM_MAX + 1):
            name = f"{i:03d}.2bp"
            if name not in used:
                return name
        raise RuntimeError("album full")

    def put(self, data: bytes, name: str | None) -> str:
        if len(data) != FRAME:
            raise ValueError(f"bad size {len(data)}")
        if len(self.files) >= ALBUM_MAX and not (
            name and any(n == name for n, _, _ in self.files)
        ):
            raise RuntimeError("album full")
        if not name:
            name = self.next_name()
        for i, (n, _, shown) in enumerate(self.files):
            if n == name:
                self.files[i] = (n, data, shown)
                return name
        self.files.append((name, data, True))
        return name


STORE = AlbumStore()
PAGE = ""


def parse_multipart_file(headers: dict[str, str], body: bytes) -> bytes:
    ctype = headers.get("Content-Type", "")
    m = re.search(r"boundary=([^;]+)", ctype)
    if not m:
        raise ValueError("no multipart boundary")
    boundary = m.group(1).strip().strip('"').encode()
    for part in body.split(b"--" + boundary):
        head, sep, data = part.partition(b"\r\n\r\n")
        if not sep:
            continue
        if b"name=\"file\"" not in head and b"name=file" not in head:
            continue
        if data.endswith(b"\r\n"):
            data = data[:-2]
        return data
    raise ValueError("no file field")


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

    def _text(self, code: int, text: str) -> None:
        self._send(code, text.encode(), "text/plain; charset=utf-8")

    def do_GET(self) -> None:
        u = urlparse(self.path)
        if u.path in ("/", "/album"):
            self._send(200, PAGE.encode("utf-8"), "text/html; charset=utf-8")
            return
        if u.path == "/favicon.svg":
            self._send(200, FAVICON.read_bytes(), "image/svg+xml")
            return
        if u.path == "/album/list":
            self._send(200, STORE.list_json(), "application/json")
            return
        if u.path == "/album/file":
            name = parse_qs(u.query).get("name", [""])[0]
            data = STORE.get(name)
            if data is None:
                self._text(404, "not found")
                return
            self._send(200, data, "application/octet-stream")
            return
        self._text(404, "not found")

    def do_POST(self) -> None:
        u = urlparse(self.path)
        q = parse_qs(u.query)
        length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(length) if length else b""
        try:
            if u.path == "/album/header":
                STORE.show_header = q.get("show", ["0"])[0] == "1"
                self._text(200, "ok")
                return
            if u.path == "/album/show":
                name = q.get("name", [""])[0]
                show = q.get("show", ["1"])[0] == "1"
                if not STORE.set_shown(name, show):
                    self._text(400, "not found")
                    return
                self._text(200, "ok")
                return
            if u.path == "/album/show-all":
                STORE.set_shown_all(q.get("show", ["1"])[0] == "1")
                self._text(200, "ok")
                return
            if u.path == "/album/delete":
                name = q.get("name", [""])[0]
                if not STORE.delete(name):
                    self._text(400, "delete failed")
                    return
                self._text(200, "ok")
                return
            if u.path == "/album/upload":
                data = parse_multipart_file(self.headers, body)
                name = STORE.put(data, q.get("name", [None])[0])
                self._text(200, name)
                return
        except Exception as e:
            self._text(400, str(e))
            return
        self._text(404, "not found")


def main() -> None:
    global PAGE
    ap = argparse.ArgumentParser(description="本机预览固件相册编辑页")
    ap.add_argument("--port", type=int, default=8765)
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--no-open", action="store_true")
    args = ap.parse_args()
    PAGE = build_album_html()
    httpd = ThreadingHTTPServer((args.host, args.port), Handler)
    url = f"http://{args.host}:{args.port}/"
    print(f"album preview  {url}")
    print("选图即可测抖动 / 色阶 / 颜色替换；上传只存在本进程内存。Ctrl+C 结束。")
    if not args.no_open:
        threading.Timer(0.4, lambda: webbrowser.open(url)).start()
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print()
        httpd.shutdown()


if __name__ == "__main__":
    main()
