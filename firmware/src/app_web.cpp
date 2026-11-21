#include "app_web.h"
#include "app_config.h"
#include "app_apps.h"
#include "app_wifi.h"
#include "app_diary.h"
#include "app_todo.h"
#include "app_weather.h"
#include "app_web_album.h"
#include "app_album.h"
#include "app_version.h"
#include "app_power.h"
#include "app_rtc.h"
#include "app_errorlog.h"

#include <WebServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <cstdio>
#include <cstring>

static WebServer g_server(80);
static bool g_running = false;
static bool g_ap = false;
static char g_url[40] = "";

enum class NavTab : uint8_t { Status, Settings, Help, Wifi, Auth, Todo, Weather, Album, Errors };

static const char *kCss = R"CSS(
*{box-sizing:border-box}
body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;margin:0;background:#f0eee8;color:#1a1a1a;line-height:1.45}
header{background:#111;color:#fff;padding:10px 14px;display:flex;align-items:center;gap:10px 14px;flex-wrap:wrap}
header .brand{display:flex;align-items:center;gap:8px;flex-shrink:0}
header .site-logo{width:28px;height:28px;object-fit:contain;flex-shrink:0;display:block}
header h1{font-size:16px;margin:0;font-weight:650;letter-spacing:.02em;white-space:nowrap}
header nav{display:flex;flex-wrap:wrap;align-items:center;gap:6px;margin:0;flex:1;min-width:0}
header nav a{display:inline-block;padding:5px 12px;border-radius:999px;font-size:13px;line-height:1.2;color:rgba(255,255,255,.55);text-decoration:none;border:1px solid transparent;background:transparent;transition:background .15s,color .15s,border-color .15s}
header nav a:hover{color:#fff;background:rgba(255,255,255,.08)}
header nav a.active{background:#ffcc00;color:#111;border-color:#ffcc00;font-weight:650}
header .meta{margin-left:auto;display:inline-flex;align-items:center;align-self:center;gap:10px;flex-shrink:0;line-height:1;min-height:28px}
header .meta .ver{font-size:12px;color:rgba(255,255,255,.55);font-variant-numeric:tabular-nums;letter-spacing:.02em;white-space:nowrap;line-height:1;display:inline-flex;align-items:center}
header .gh{display:inline-flex;align-items:center;justify-content:center;color:rgba(255,255,255,.7);line-height:0}
header .gh svg{display:block;flex-shrink:0}
header .gh:hover{color:#fff}
main{max-width:720px;margin:16px auto;padding:0 16px 28px}
section{display:block;background:#fff;border-radius:12px;padding:18px 16px;margin-bottom:14px;box-shadow:0 1px 2px rgba(0,0,0,.05)}
h2{font-size:15px;margin:0 0 14px;font-weight:650}
.hint{color:#6b6b6b;font-size:13px;margin:0 0 14px}
.ok{color:#0a7a4b}.err{color:#8b2010}
.flash{padding:10px 12px;border-radius:8px;margin:0 0 14px;font-size:13px}
.flash.ok{background:#e8f7ef;color:#0a7a4b}
.flash.err{background:#fdecec;color:#8b2010}
label{display:block;font-size:12px;margin:12px 0 5px;color:#555;font-weight:500}
input,textarea,select{width:100%;padding:10px 12px;border:1px solid #ddd;border-radius:8px;font-size:14px;background:#fafafa}
input:focus,textarea:focus,select:focus{outline:0;border-color:#1a1a1a;background:#fff}
.btn-row{display:flex;flex-wrap:wrap;gap:8px;margin-top:16px}
button,.btn{background:#1a1a1a;color:#fff;border:0;border-radius:8px;padding:11px 16px;font-size:14px;cursor:pointer}
button.secondary{background:#5c5c5c}
button.danger{background:#8b2010}
code{font-size:12px;background:#f3f3f3;padding:1px 5px;border-radius:4px}
pre{white-space:pre-wrap;background:#f7f7f5;padding:12px;border-radius:8px;font-size:13px;margin:0}
.kv{display:grid;grid-template-columns:88px 1fr;gap:8px 12px;align-items:baseline;margin:0}
.kv dt{font-size:12px;color:#888;margin:0}
.kv dd{margin:0;font-size:14px;word-break:break-all}
.pill{display:inline-flex;align-items:center;gap:6px;padding:3px 10px;border-radius:999px;font-size:12px;font-weight:600}
.pill::before{content:"";width:7px;height:7px;border-radius:50%;background:currentColor}
.pill.on{background:#e8f7ef;color:#0a7a4b}
.pill.off{background:#f3f3f3;color:#888}
.pill.ap{background:#fff6d6;color:#8a6d00}
.pill.warn{background:#fff1e6;color:#9a4b00}
.status-hero{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:16px;padding-bottom:14px;border-bottom:1px solid #eee}
.status-hero .ver{font-size:12px;color:#999}
.wifi-card{border:1px solid #e8e8e8;border-radius:12px;padding:14px;margin:0 0 12px;background:#fafafa;transition:border-color .15s,box-shadow .15s}
.wifi-card.active{border-color:#1a1a1a;background:#fff;box-shadow:0 0 0 1px #1a1a1a}
.wifi-card .wifi-top{display:flex;align-items:center;justify-content:space-between;margin-bottom:8px}
.wifi-card .wifi-active{display:inline-flex;align-items:center;gap:6px;font-size:12px;color:#666;cursor:pointer;user-select:none}
.wifi-card .wifi-active input{width:auto;margin:0;accent-color:#1a1a1a;padding:0}
.wifi-card .wifi-idx{font-size:11px;color:#aaa;letter-spacing:.04em}
.wifi-card .wifi-ssid{width:100%;border:0;background:transparent;padding:4px 0 8px;font-size:22px;font-weight:650;line-height:1.25;color:#1a1a1a;border-bottom:1px solid #e8e8e8;border-radius:0}
.wifi-card .wifi-ssid:focus{outline:0;border-bottom-color:#1a1a1a;background:transparent}
.wifi-card .wifi-ssid::placeholder{color:#cfcfcf;font-weight:500}
.wifi-card .wifi-pass{width:100%;border:0;background:transparent;padding:10px 0 2px;font-size:13px;color:#555;border-radius:0}
.wifi-card .wifi-pass:focus{outline:0;background:transparent}
.wifi-card .wifi-pass::placeholder{color:#cfcfcf}
.todo-list{list-style:none;padding:0;margin:0}
.todo-list li{display:flex;gap:10px;align-items:flex-start;padding:12px 0;border-bottom:1px solid #f0f0f0}
.todo-list li:last-child{border-bottom:0}
.todo-list .box{flex-shrink:0;width:18px;height:18px;margin-top:2px;border:1.5px solid #1a1a1a;border-radius:4px}
.todo-list li.done .box{background:#1a1a1a}
.todo-list li.done .txt{color:#999;text-decoration:line-through}
.todo-list .note{display:block;margin-top:4px;font-size:12px;color:#c9a000;text-decoration:none}
.meta{font-size:12px;color:#888;margin:0 0 12px}
.geo-list{list-style:none;padding:0;margin:12px 0 0}
.geo-list li{padding:10px 0;border-bottom:1px solid #f0f0f0}
.geo-list li:last-child{border-bottom:0}
.geo-list label{display:flex;gap:10px;align-items:flex-start;margin:0;cursor:pointer;color:#1a1a1a;font-size:14px;font-weight:500}
.geo-list input{width:auto;margin:3px 0 0;accent-color:#1a1a1a}
.geo-list .sub{display:block;margin-top:2px;font-size:12px;color:#888;font-weight:400}
.check-row{display:flex;align-items:center;gap:12px;padding:14px 16px;border:2px solid #1a1a1a;border-radius:10px;background:#fff;cursor:pointer;margin:4px 0 16px;user-select:none}
.check-row input{width:22px;height:22px;margin:0;accent-color:#0a7a4b;flex-shrink:0}
.check-row span{font-size:16px;font-weight:650;color:#1a1a1a}
.check-row.on,.check-row:has(input:checked){background:#e8f7ef;border-color:#0a7a4b}
.check-row.on span,.check-row:has(input:checked) span{color:#0a7a4b}
body.page-album{overflow-x:auto;overflow-y:auto;min-height:100vh;background:#e8e6e0}
body.page-album main{max-width:none;width:100%;margin:0;padding:0;display:block}
.ed-shell{display:flex;flex-direction:column;min-width:0;background:#fff}
.ed-work{display:flex;flex-direction:column;height:auto;overflow:visible;border-bottom:1px solid #d0cec8}
.ed-toolbar{display:flex;flex-wrap:nowrap;gap:8px;align-items:center;background:#fff;border-bottom:1px solid #d0cec8;padding:6px 10px;flex:0 0 auto;overflow-x:auto}
.ed-tool-main{display:flex;flex:1;min-width:0;gap:8px;align-items:center;overflow-x:auto}
.ed-tool-end{margin-left:8px;flex-shrink:0}
.ed-sep{width:1px;height:28px;background:#d0cec8;flex-shrink:0}
.ed-tool-group{display:flex;flex-wrap:nowrap;gap:4px;align-items:center;flex-shrink:0}
.ed-paint{gap:6px}
.ed-erase-sz{display:flex;align-items:center;gap:6px;padding:2px 8px;background:#f3f2ee;min-width:140px}
.ed-erase-sz span{font-size:12px;color:#555;white-space:nowrap}
.ed-erase-sz input[type=range]{flex:1;padding:0;border:0;accent-color:#1a1a1a;min-width:72px}
.ed-erase-sz em{font-style:normal;font-size:12px;color:#888;width:20px;text-align:right}
.ed-toolbar button{padding:7px 10px;font-size:12px;border-radius:0;background:#eceae4;color:#1a1a1a;white-space:nowrap}
.ed-toolbar button.on,.ed-map-bar>button.on{background:#1a1a1a;color:#fff;box-shadow:none}
.ed-body{flex:0 0 auto;display:grid;grid-template-columns:1fr;min-width:0;overflow:visible;align-items:start;padding-top:20px;background:#f0eee8}
@media(min-width:980px){.ed-body{grid-template-columns:max-content minmax(320px,1fr)}}
.ed-main{display:flex;flex-direction:column;width:max-content;max-width:100%;min-width:0;align-self:start}
.ed-pair{display:grid;grid-template-columns:400px 400px;grid-template-rows:auto auto;width:max-content;max-width:100%;min-height:0;height:max-content;overflow:hidden;background:#fff;justify-self:start;align-self:start}
.ed-stage{position:relative;width:400px;min-width:400px;line-height:0;overflow:hidden;display:flex;align-items:flex-start;justify-content:center;background:#fff;padding:0;grid-row:1}
.ed-stage:nth-child(2){border-left:1px solid #d0cec8}
.ed-fit{position:relative;width:400px;height:300px;max-width:400px;line-height:0;flex-shrink:0;overflow:hidden}
.ed-can,.ed-cur{position:absolute;inset:0;width:100%;height:100%;image-rendering:pixelated;border:1px solid #ccc;background:#fff;touch-action:none;display:block}
.ed-can{cursor:grab}
.ed-cur{pointer-events:none;border:0;background:transparent}
#pvStage .ed-fit{overflow:hidden}
.ed-zoom{position:absolute;inset:0;width:100%;height:100%;overflow:hidden}
.ed-toast{position:fixed;top:56px;left:50%;transform:translate(-50%,-12px);background:#0a7a4b;color:#fff;padding:12px 24px;font-size:16px;font-weight:700;z-index:80;opacity:0;pointer-events:none;box-shadow:0 8px 28px rgba(0,0,0,.22);transition:opacity .18s,transform .18s;letter-spacing:.02em}
.ed-toast.show{opacity:1;transform:translate(-50%,0)}
.ed-toast.err{background:#8b2010}
#loupe{position:absolute;width:96px;height:96px;border:2px solid #111;image-rendering:pixelated;background:#fff;pointer-events:none;display:none;box-shadow:0 4px 16px rgba(0,0,0,.25);z-index:2}
.ed-foot{display:flex;flex-wrap:nowrap;align-items:center;gap:6px;height:40px;padding:0 8px;overflow:visible;border-top:1px solid #d0cec8;grid-row:2;background:#fff;align-self:start}
.ed-foot:last-child{border-left:1px solid #d0cec8}
.ed-foot h3{margin:0;font-size:12px;font-weight:650;flex-shrink:0}
.ed-foot button{border-radius:0;padding:6px 10px;font-size:12px}
.ed-foot .meta{margin:0;font-size:11px;color:#888;min-width:0;flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.ed-side{background:#fff;padding:8px 12px 16px;overflow:auto;min-height:0;min-width:320px;border-left:1px solid #d0cec8;display:flex;flex-direction:column;gap:8px;align-self:stretch;container-type:inline-size;container-name:edside}
.ed-mode{display:inline-flex;gap:0;border:1px solid #111;align-self:flex-start;width:auto}
.ed-side-ops{display:grid;grid-template-columns:1fr;gap:8px;min-width:0}
.ed-levels{min-width:0}
@container edside (min-width:600px){.ed-side-ops{grid-template-columns:1fr 1fr;align-items:start}}
.ed-side-h{display:flex;align-items:center;justify-content:space-between;gap:8px}
.ed-side-h h3{margin:0;font-size:13px;font-weight:650}
.ed-side button{padding:4px 8px;font-size:12px;border-radius:0}
.ed-mode button{flex:0 0 auto;border:0;border-radius:0;padding:7px 14px;font-size:13px;background:#eceae4;color:#1a1a1a}
.ed-mode button.on{background:#1a1a1a;color:#fff}
.ed-block{padding:0}
.ed-maps{width:800px;max-width:100%;box-sizing:border-box;padding:10px 12px 16px;border-top:1px solid #d0cec8;background:#fff}
.ed-maps button{padding:4px 8px;font-size:12px;border-radius:0}
.ed-map-bar{display:flex;align-items:center;gap:8px;flex-wrap:wrap}
.ed-map-bar .ed-mixn{margin-left:auto}
.ed-map-bar .ed-mixn button{padding:5px 10px;font-size:12px}
.ed-map-body{display:flex;align-items:flex-start;gap:8px;margin-top:6px;min-width:0}
.ed-map-hint{margin:8px 0 0;font-size:11px;line-height:1.35;color:#6b6b6b}
.ed-cmap{position:relative;flex:1;min-width:0;height:380px;border:1px solid #ccc;background:linear-gradient(180deg,#f7f5ef,#f0eee8);overflow:hidden}
.ed-cmap-edge{position:absolute;z-index:3;font-size:11px;font-weight:650;color:#555;pointer-events:none;letter-spacing:.04em}
.ed-cmap-edge.e-tl{top:6px;left:10px}
.ed-cmap-edge.e-tr{top:6px;right:10px}
.ed-cmap-edge.e-bl{bottom:6px;left:10px}
.ed-cmap-edge.e-br{bottom:6px;right:10px}
.ed-cmap-plot{position:absolute;inset:28px 32px;z-index:2}
.ed-cmap .sw-mix,.ed-cmap .sw-pure,.ed-cmap .sw-drop{position:absolute;padding:0;margin:0;border-radius:2px;border:1px solid rgba(0,0,0,.35);cursor:pointer;box-shadow:none;transform:translate(-50%,-50%);z-index:2}
.ed-cmap .sw-mix{width:18px;height:18px}
.ed-cmap .sw-pure{width:26px;height:26px;border-width:1px;z-index:4}
.ed-cmap .sw-pure.pal-k{background:#111;border-color:#111}
.ed-cmap .sw-pure.pal-w{background:#fff;border-color:#888}
.ed-cmap .sw-pure.pal-y{background:#F6C749;border-color:#c9a02a}
.ed-cmap .sw-pure.pal-r{background:#8b2010;border-color:#5c150a}
.ed-cmap .sw-pure.arm,.ed-cmap .sw-mix.arm{box-shadow:0 0 0 2px #fff,0 0 0 4px #111;z-index:5}
.ed-cmap .sw-mix.on{box-shadow:0 0 0 2px #fff,0 0 0 3px #888}
.ed-cmap .sw-mix.sel,.ed-cmap .sw-drop.sel,.ed-cmap .sw-pure.sel{box-shadow:0 0 0 2px #fff,0 0 0 3px #111;z-index:6}
.ed-cmap .sw-drop{width:12px;height:12px;border-radius:50%;border:2px solid #111;z-index:5}
.ed-cmap .sw-drop.on{box-shadow:none}
.ed-wires{position:absolute;inset:0;width:100%;height:100%;overflow:visible;z-index:1;pointer-events:none}
.ed-wires line{pointer-events:stroke;cursor:pointer}
.tol-pop{display:flex;flex-direction:column;align-items:center;justify-content:flex-start;gap:4px;margin:0;padding:6px 4px;background:#f3f2ee;width:52px;flex:0 0 52px;box-sizing:border-box;overflow:hidden;min-height:0}
.tol-pop[hidden]{display:none}
.tol-pop span{flex:0 0 auto;font-size:11px;color:#333;text-align:center;line-height:1.2;white-space:normal;max-width:100%}
.tol-pop input[type=range].tol-v{flex:1 1 0%;width:24px;min-width:0;min-height:0;height:0;padding:0;border:0;background:transparent;accent-color:#1a1a1a;writing-mode:vertical-lr;direction:rtl;-webkit-appearance:slider-vertical;appearance:slider-vertical}
.tol-pop em{flex:0 0 auto;font-style:normal;font-size:12px;color:#555;text-align:center;width:100%}
.ed-maps .rules{display:none}
#srcStage .ed-can{image-rendering:auto}
.ed-help{margin:8px 0 0;font-size:11px;line-height:1.35}
.hist-wrap{position:relative;user-select:none;border:1px solid #c8c8c8;background:#fff}
#hist{width:100%;height:160px;display:block;background:#fff;cursor:ew-resize}
.hist-read{display:flex;justify-content:space-between;font-size:11px;color:#888;margin-top:2px;font-variant-numeric:tabular-nums}
.ink-hist-wrap{position:relative;user-select:none;border:1px solid #c8c8c8;background:#fff;margin:0 0 2px}
#inkHist{width:100%;height:48px;display:block;background:#fff}
.ink-hist-read{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));font-size:12px;line-height:1.3;color:#444;margin:0 0 6px;font-variant-numeric:tabular-nums;-webkit-font-smoothing:antialiased}
.ink-hist-read span{min-width:0;text-align:center}
.ed-row{display:flex;align-items:center;gap:6px;margin:4px 0}
.ed-row span{flex:0 0 40px;font-size:12px;color:#555}
.ed-row input[type=range]{flex:1;padding:0;border:0;background:transparent;accent-color:#1a1a1a;min-width:0}
.ed-row em{flex:0 0 42px;font-style:normal;font-size:11px;color:#888;text-align:right}
.ed-num{width:48px;flex:0 0 48px;padding:2px 4px;border:1px solid #ccc;font-size:12px;text-align:right;background:#fff}
.pal-sw{display:inline-flex;align-items:center;gap:12px;margin:0;padding:4px 2px;border:0;flex-shrink:0}
.pal-sw button,.ed-toolbar .pal-sw button{width:16px;height:16px;padding:0;margin:0;border-radius:50%;border:1px solid rgba(0,0,0,.4);font-size:0;line-height:0;cursor:pointer;box-shadow:none;outline:none;flex-shrink:0}
.pal-sw button.on,.ed-toolbar .pal-sw button.on{outline:none;box-shadow:0 0 0 2px #fff,0 0 0 4px #111;background-image:none}
.pal-sw button.pal-k,.ed-toolbar .pal-sw button.pal-k{background:#111;border-color:#111}
.pal-sw button.pal-w,.ed-toolbar .pal-sw button.pal-w{background:#fff;border-color:#888}
.pal-sw button.pal-y,.ed-toolbar .pal-sw button.pal-y{background:#F6C749;border-color:#c9a02a}
.pal-sw button.pal-r,.ed-toolbar .pal-sw button.pal-r{background:#8b2010;border-color:#5c150a}
.rules{font-size:11px;margin:4px 0 0;max-height:none;overflow:visible}
.rules .item{display:flex;align-items:center;gap:6px;padding:4px 4px;cursor:pointer;border:1px solid transparent}
.rules .item.on{background:#eceae4;border-color:#111}
.rules .item .lab{flex:1;min-width:0;color:#555}
.sw{width:14px;height:14px;border:1px solid #ccc;flex-shrink:0}
.ed-album{padding:14px 12px 24px;background:#f0eee8;flex:0 0 auto}
.ed-album-h{display:flex;align-items:center;gap:10px;margin-bottom:10px;flex-wrap:wrap}
.ed-album-h h2{margin:0;font-size:14px}
.ed-album-h #btnExportAll{margin-left:auto;padding:6px 10px;font-size:12px}
.ed-album-h .show-lab{margin-left:0}
.ed-album .check-row{border-radius:0;margin:0 0 8px;max-width:420px}
.ed-album .hdr-hint{margin:0 0 14px;font-size:12px;color:#6b6b6b;max-width:420px}
.ed-saved{display:grid;grid-template-columns:repeat(auto-fill,minmax(200px,1fr));gap:10px;overflow:visible;padding-bottom:4px}
.thumb{border:1px solid #ccc;padding:6px;background:#fff;width:auto;min-width:0;box-sizing:border-box}
.thumb.on{border-color:#111;outline:1px solid #111}
.thumb.off{filter:grayscale(1) opacity(.45)}
.thumb canvas{width:100%;height:auto;aspect-ratio:4/3;image-rendering:pixelated;display:block;background:#fff;cursor:pointer}
.thumb .row{display:flex;align-items:center;gap:6px;margin-top:6px;flex-wrap:wrap}
.thumb .row.meta-row{justify-content:space-between}
.thumb .row.acts{margin-top:4px;gap:6px;flex-wrap:nowrap}
.thumb .row.acts button{margin:0;padding:4px 6px;font-size:11px;border-radius:0;flex:1 1 0;min-width:0;white-space:nowrap}
.thumb .row code{font-size:10px}
.show-lab{display:inline-flex;align-items:center;gap:6px;font-size:12px;font-weight:600;color:#1a1a1a;cursor:pointer;user-select:none}
.show-lab input{width:16px;height:16px;margin:0;accent-color:#0a7a4b;padding:0;flex-shrink:0}
.file-pick{display:flex;flex-direction:row;align-items:center;gap:8px;margin:0;flex-shrink:0}
.file-cta{position:relative;display:flex;align-items:center;justify-content:center;height:36px;padding:0 16px;background:#1a1a1a;color:#fff;font-size:14px;font-weight:700;cursor:pointer;border:0;overflow:hidden;letter-spacing:.02em}
.file-cta:hover{background:#333}
.file-cta input{position:absolute;inset:0;opacity:0;cursor:pointer;width:100%;height:100%;padding:0;border:0;margin:0}
.file-name{display:block;font-size:11px;line-height:1.3;color:#555;max-width:140px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.file-name.empty{color:#aaa}
.app-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(148px,1fr));gap:10px;margin:0}
.app-card{display:flex;flex-direction:column;align-items:flex-start;gap:8px;padding:14px 12px;border:1px solid #e4e4e4;border-radius:12px;background:#fafafa;color:#1a1a1a;cursor:pointer;text-align:left;width:100%;font:inherit;box-shadow:none;transition:border-color .15s,background .15s,box-shadow .15s}
.app-card:hover{border-color:#bbb}
.app-card.on{border-color:#0a7a4b;background:#e8f7ef;box-shadow:0 0 0 1px #0a7a4b}
.app-card.off{opacity:.72}
.app-card .ico{width:36px;height:36px;border-radius:10px;background:#fff;border:1px solid #e8e8e8;display:flex;align-items:center;justify-content:center}
.app-card.on .ico{border-color:#b7e0c8}
.app-card .ico svg{width:20px;height:20px;display:block}
.app-card .name{font-size:15px;font-weight:650;margin:0}
.app-card .desc{font-size:12px;color:#888;margin:0;line-height:1.35}
.app-card .st{font-size:11px;font-weight:650;letter-spacing:.02em}
.app-card.on .st{color:#0a7a4b}
.app-card.off .st{color:#999}
.led-list{list-style:none;padding:0;margin:0}
.led-list li{display:grid;grid-template-columns:44px 1fr;gap:12px;align-items:center;padding:14px 0;border-bottom:1px solid #f0f0f0}
.led-list li:last-child{border-bottom:0}
.led-dot-wrap{width:44px;height:44px;border-radius:12px;background:#111;display:flex;align-items:center;justify-content:center;flex-shrink:0}
.led-dot{width:14px;height:14px;border-radius:50%;background:#1a3d1a;box-shadow:inset 0 0 0 1px rgba(255,255,255,.08)}
.led-dot.on{background:#3dff7a;box-shadow:0 0 10px rgba(61,255,122,.55),0 0 2px #3dff7a}
.led-dot.off{background:#1a3d1a;box-shadow:none}
.led-meta .name{font-size:15px;font-weight:650;margin:0 0 4px}
.led-meta .desc{font-size:13px;color:#555;margin:0;line-height:1.4}
.led-meta .when{display:block;margin-top:4px;font-size:12px;color:#888}
@keyframes led-slow{0%,49%{background:#3dff7a;box-shadow:0 0 10px rgba(61,255,122,.55)}50%,100%{background:#1a3d1a;box-shadow:none}}
@keyframes led-fast{0%,49%{background:#3dff7a;box-shadow:0 0 10px rgba(61,255,122,.55)}50%,100%{background:#1a3d1a;box-shadow:none}}
@keyframes led-done{0%,12%{background:#3dff7a;box-shadow:0 0 10px rgba(61,255,122,.55)}13%,100%{background:#1a3d1a;box-shadow:none}}
@keyframes led-boot{0%,8%{background:#3dff7a;box-shadow:0 0 10px rgba(61,255,122,.55)}9%,16%{background:#1a3d1a;box-shadow:none}17%,25%{background:#3dff7a;box-shadow:0 0 10px rgba(61,255,122,.55)}26%,33%{background:#1a3d1a;box-shadow:none}34%,42%{background:#3dff7a;box-shadow:0 0 10px rgba(61,255,122,.55)}43%,100%{background:#1a3d1a;box-shadow:none}}
@keyframes led-ack{0%,8%{background:#1a3d1a;box-shadow:none}9%,16%{background:#3dff7a;box-shadow:0 0 10px rgba(61,255,122,.55)}17%,25%{background:#1a3d1a;box-shadow:none}26%,33%{background:#3dff7a;box-shadow:0 0 10px rgba(61,255,122,.55)}34%,100%{background:#1a3d1a;box-shadow:none}}
@keyframes led-error{0%,6%{background:#3dff7a;box-shadow:0 0 10px rgba(61,255,122,.55)}7%,13%{background:#1a3d1a;box-shadow:none}14%,20%{background:#3dff7a;box-shadow:0 0 10px rgba(61,255,122,.55)}21%,27%{background:#1a3d1a;box-shadow:none}28%,34%{background:#3dff7a;box-shadow:0 0 10px rgba(61,255,122,.55)}35%,100%{background:#1a3d1a;box-shadow:none}}
@keyframes led-double{0%,8%{background:#3dff7a;box-shadow:0 0 10px rgba(61,255,122,.55)}9%,16%{background:#1a3d1a;box-shadow:none}17%,25%{background:#3dff7a;box-shadow:0 0 10px rgba(61,255,122,.55)}26%,100%{background:#1a3d1a;box-shadow:none}}
.led-anim-solid{background:#3dff7a;box-shadow:0 0 10px rgba(61,255,122,.55)}
.led-anim-slow{animation:led-slow .8s steps(1,end) infinite}
.led-anim-fast{animation:led-fast .24s steps(1,end) infinite}
.led-anim-done{animation:led-done 1.2s steps(1,end) infinite}
.led-anim-boot{animation:led-boot 1.6s steps(1,end) infinite}
.led-anim-ack{animation:led-ack 1.2s steps(1,end) infinite}
.led-anim-error{animation:led-error 2s steps(1,end) infinite}
.led-anim-double{animation:led-double .94s steps(1,end) infinite}
.err-list{list-style:none;padding:0;margin:0}
.err-list li{padding:12px 0;border-bottom:1px solid #f0f0f0}
.err-list li:last-child{border-bottom:0}
.err-top{display:flex;align-items:center;justify-content:space-between;gap:10px;margin-bottom:6px}
.err-src{display:inline-block;padding:2px 8px;border-radius:999px;background:#fdecec;color:#8b2010;font-size:12px;font-weight:650}
.err-when{font-size:12px;color:#888;font-variant-numeric:tabular-nums}
.err-msg{margin:0;white-space:pre-wrap;background:#faf7f5;border:1px solid #f0e6e4;padding:10px 12px;border-radius:8px;font-size:13px;color:#1a1a1a;line-height:1.4}
)CSS";

static String htmlEscapeAttr(const char *s) {
  String out;
  if (!s) return out;
  for (const char *p = s; *p; ++p) {
    if (*p == '&') out += F("&amp;");
    else if (*p == '"') out += F("&quot;");
    else if (*p == '\'') out += F("&#39;");
    else if (*p == '<') out += F("&lt;");
    else out += *p;
  }
  return out;
}

static void appendNavLink(String &page, NavTab tab, NavTab active, const char *href, const char *label) {
  page += F("<a href=");
  page += href;
  if (tab == active) page += F(" class=active");
  page += F(">");
  page += label;
  page += F("</a>");
}

static void redirectOk(const char *path, const char *code) {
  String loc = String(path);
  loc += F("?ok=");
  loc += code;
  g_server.sendHeader("Location", loc);
  g_server.send(303);
}

static void appendFlashFromQuery(String &b) {
  if (!g_server.hasArg("ok")) return;
  const String code = g_server.arg("ok");
  const char *msg = nullptr;
  bool err = false;
  if (code == "power") msg = "电源 / 日历设置已保存";
  else if (code == "tz") msg = "时区 / NTP 周期已保存";
  else if (code == "ntp") msg = "NTP 校时完成";
  else if (code == "ntp_fail") {
    msg = "NTP 校时失败（需联网）";
    err = true;
  } else if (code == "rtc") msg = "已写入 RTC";
  else if (code == "apps") msg = "应用开关已更新";
  else if (code == "apps_last") {
    msg = "至少保留一个应用";
    err = true;
  } else if (code == "wifi") msg = "WiFi 已保存";
  else if (code == "todo") msg = "待办已拉取";
  else if (code == "weather") msg = "天气开关已保存";
  else if (code == "pick") msg = "已选用位置";
  else if (code == "saved") msg = "已保存";
  else if (code == "err_cleared") msg = "错误记录已清空";
  if (!msg) return;
  b += err ? F("<div class='flash err'>") : F("<div class='flash ok'>");
  b += msg;
  b += F("</div>");
}

// Chunked send: avoid one giant String (album page ~70KB used to OOM → blank).
using SendHtmlExtraFn = void (*)(WebServer &);

static void sendHtml(NavTab active, const String &body, SendHtmlExtraFn extra = nullptr) {
  g_server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  g_server.send(200, "text/html; charset=utf-8", "");

  g_server.sendContent(F("<!DOCTYPE html><html><head><meta charset=utf-8>"
                         "<meta name=viewport content='width=device-width,initial-scale=1'>"
                         "<link rel=icon href=/favicon.svg type=image/svg+xml>"
                         "<title>Carte 配置</title><style>"));
  g_server.sendContent(kCss, strlen(kCss));
  g_server.sendContent(F("</style></head><body"));
  if (active == NavTab::Album) g_server.sendContent(F(" class=page-album"));

  {
    String nav;
    nav.reserve(1800);
    nav += F("><header>"
             "<div class=brand>"
             "<img class=site-logo src=/favicon.svg alt='' width=28 height=28>"
             "<h1>Carte</h1></div><nav>");
    appendNavLink(nav, NavTab::Status, active, "/", "状态");
    appendNavLink(nav, NavTab::Settings, active, "/settings", "设置");
    appendNavLink(nav, NavTab::Wifi, active, "/wifi", "WiFi");
    appendNavLink(nav, NavTab::Auth, active, "/auth", "鉴权");
    appendNavLink(nav, NavTab::Todo, active, "/todo", "待办");
    appendNavLink(nav, NavTab::Weather, active, "/weather", "天气");
    appendNavLink(nav, NavTab::Album, active, "/album", "相册");
    appendNavLink(nav, NavTab::Errors, active, "/errors", "错误");
    appendNavLink(nav, NavTab::Help, active, "/help", "帮助");
    nav += F("</nav><div class=meta><span class=ver>");
    nav += APP_VERSION;
    nav += F("(");
    nav += String(APP_BUILD);
    nav += F(")</span>"
             "<a class=gh href='https://github.com/KyleBing/zectrix-note4c-carte' target=_blank rel=noopener title=GitHub aria-label=GitHub>"
             "<svg viewBox='0 0 16 16' width=22 height=22 fill=currentColor aria-hidden=true>"
             "<path d='M8 0C3.58 0 0 3.58 0 8c0 3.54 2.29 6.53 5.47 7.59.4.07.55-.17.55-.38 0-.19-.01-.82-.01-1.49-2.01.37-2.53-.49-2.69-.94-.09-.23-.48-.94-.82-1.13-.28-.15-.68-.52-.01-.53.63-.01 1.08.58 1.23.82.72 1.21 1.87.87 2.33.66.07-.52.28-.87.51-1.07-1.78-.2-3.64-.89-3.64-3.95 0-.87.31-1.59.82-2.15-.08-.2-.36-1.02.08-2.12 0 0 .67-.21 2.2.82.64-.18 1.32-.27 2-.27.68 0 1.36.09 2 .27 1.53-1.04 2.2-.82 2.2-.82.44 1.1.16 1.92.08 2.12.51.56.82 1.27.82 2.15 0 3.07-1.87 3.75-3.65 3.95.29.25.54.73.54 1.48 0 1.07-.01 1.93-.01 2.2 0 .21.15.46.55.38A8.013 8.013 0 0016 8c0-4.42-3.58-8-8-8z'/>"
             "</svg></a></div></header><main>");
    g_server.sendContent(nav);
  }

  if (body.length()) g_server.sendContent(body);
  if (extra) extra(g_server);
  g_server.sendContent(F("</main></body></html>"));
  g_server.sendContent("");  // end chunked
}

static void kvRow(String &b, const char *k, const String &v) {
  b += F("<dt>");
  b += k;
  b += F("</dt><dd>");
  b += v;
  b += F("</dd>");
}

static void handleRoot() {
  const AppConfig &cfg = getAppConfig();
  String b;
  appendFlashFromQuery(b);
  b += F("<section><div class=status-hero><div>");
  if (wifiIsStaConnected()) {
    b += F("<span class='pill on'>同 WiFi 站点已连接</span>");
  } else if (g_ap) {
    b += F("<span class='pill ap'>AP 模式</span>");
  } else {
    b += F("<span class='pill off'>未联网</span>");
  }
  b += F("</div><div class=ver>");
  b += APP_VERSION;
  b += F("(");
  b += String(APP_BUILD);
  b += F(")</div></div><dl class=kv>");
  kvRow(b, "模式", String(webModeName()));
  kvRow(b, "地址", String(g_url));
  if (wifiIsStaConnected()) {
    String net = String(wifiCurrentSsid()) + " · " + wifiStaIp() + " · " + String(wifiStaRssi()) + " dBm";
    kvRow(b, "网络", net);
  } else if (g_ap) {
    String ap = String(WIFI_AP_SSID) + " / " + WIFI_AP_PASS + " → " + wifiApIp();
    kvRow(b, "热点", ap);
  }
  kvRow(b, "Portal", String(cfg.portal_base_url));
  kvRow(b, "邮箱", String(cfg.portal_email[0] ? cfg.portal_email : "—"));
  if (cfg.portal_token[0]) {
    kvRow(b, "Token", String("<span class='pill on'>已保存</span> <span class=hint>将自动续签</span>"));
  } else {
    kvRow(b, "Token", String("<span class='pill off'>未登录</span>"));
  }
  kvRow(b, "Diary", String(cfg.diary_id[0] ? cfg.diary_id : "—"));
  if (qweatherConfigured()) {
    String loc = String(cfg.qweather_location_name[0] ? cfg.qweather_location_name : cfg.qweather_location_id);
    loc += " · via Portal";
    kvRow(b, "天气", loc);
  } else if (cfg.qweather_enabled) {
    kvRow(b, "天气", String("<span class='pill warn'>需登录 + 选择位置</span>"));
  } else {
    kvRow(b, "天气", String("<span class='pill off'>关闭</span>"));
  }
  {
    if (cfg.weather_refresh_sec == 0) {
      kvRow(b, "天气刷新", String("关闭"));
    } else {
      uint32_t d = 0, h = 0, m = 0;
      weatherRefreshToDhm(cfg.weather_refresh_sec, d, h, m);
      char buf[48];
      snprintf(buf, sizeof(buf), "%u 天 %u 时 %u 分", (unsigned)d, (unsigned)h, (unsigned)m);
      kvRow(b, "天气刷新", String(buf));
    }
  }
  {
    AlbumList al;
    albumList(al);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d / %d", al.count, ALBUM_MAX);
    kvRow(b, "相册", String(buf));
    kvRow(b, "相册标题栏", String(cfg.album_show_header ? "显示" : "隐藏"));
  }
  {
    char idle[40];
    if (cfg.idle_off_sec == 0) {
      snprintf(idle, sizeof(idle), "关闭");
    } else {
      snprintf(idle, sizeof(idle), "%u 秒", (unsigned)cfg.idle_off_sec);
    }
    kvRow(b, "自动关机", String(idle));
    kvRow(b, "接电仍关机", String(cfg.idle_off_on_power ? "是" : "否"));
  }
  kvRow(b, "一周起始", String(cfg.week_start_monday ? "周一" : "周日"));
  {
    const int nerr = errorLogCount();
    if (nerr > 0) {
      kvRow(b, "错误", String("<a href=/errors><span class='pill warn'>") + String(nerr) +
                            " 条 · 查看</span></a>");
    } else {
      kvRow(b, "错误", String("<a href=/errors><span class='pill off'>无</span></a>"));
    }
  }
  {
    char tz[24];
    const int off = cfg.tz_offset_min;
    const int omin = (off < 0 ? -off : off) % 60;
    snprintf(tz, sizeof(tz), "UTC%+d:%02d", off / 60, omin);
    kvRow(b, "时区", String(tz));
    char ntp[48];
    const int ago = rtcDaysSinceNtp();
    if (ago < 0) {
      snprintf(ntp, sizeof(ntp), "从未 · 周期 %u 天", (unsigned)cfg.ntp_interval_days);
    } else {
      snprintf(ntp, sizeof(ntp), "%d 天前 · 周期 %u 天", ago, (unsigned)cfg.ntp_interval_days);
    }
    kvRow(b, "NTP", String(ntp));
    RtcDateTime now;
    if (rtcRead(now)) {
      char tbuf[40];
      snprintf(tbuf, sizeof(tbuf), "%04d-%02d-%02d %02d:%02d:%02d", now.year, now.month, now.day,
               now.hour, now.minute, now.second);
      kvRow(b, "当前时间", String(tbuf));
    } else {
      kvRow(b, "当前时间", String("<span class='pill warn'>未设置</span>"));
    }
  }
  {
    String apps;
    const int n = static_cast<int>(AppId::Count);
    for (int i = 0; i < n; i++) {
      const AppId id = static_cast<AppId>(i);
      if (!appIdEnabled(id)) continue;
      if (apps.length()) apps += " · ";
      apps += appIdLabelZh(id);
    }
    if (!apps.length()) apps = "—";
    kvRow(b, "应用", apps);
  }
  b += F("</dl></section>");
  sendHtml(NavTab::Status, b);
}

static const char *appCardSvg(AppId id) {
  switch (id) {
    case AppId::Todo:
      return "<svg viewBox='0 0 24 24' fill='none' stroke='#1a1a1a' stroke-width='2'>"
             "<rect x='4' y='4' width='16' height='16' rx='2'/>"
             "<path d='M8 12l2.5 2.5L16 9'/></svg>";
    case AppId::Calendar:
      return "<svg viewBox='0 0 24 24' fill='none' stroke='#1a1a1a' stroke-width='2'>"
             "<rect x='3' y='5' width='18' height='16' rx='2'/>"
             "<path d='M3 10h18M8 3v4M16 3v4'/></svg>";
    case AppId::Weather:
      return "<svg viewBox='0 0 24 24' fill='none' stroke='#1a1a1a' stroke-width='2'>"
             "<circle cx='10' cy='10' r='3'/>"
             "<path d='M10 3v2M10 15v2M3 10h2M15 10h2M5.2 5.2l1.4 1.4M13.4 13.4l1.4 1.4M5.2 14.8l1.4-1.4M13.4 6.6l1.4-1.4'/>"
             "<path d='M14 16.5a3.5 3.5 0 1 0 .3-7'/></svg>";
    case AppId::Album:
      return "<svg viewBox='0 0 24 24' fill='none' stroke='#1a1a1a' stroke-width='2'>"
             "<rect x='3' y='5' width='18' height='14' rx='2'/>"
             "<circle cx='9' cy='11' r='2'/>"
             "<path d='M3 16l5-4 4 3 3-2 6 4'/></svg>";
    default:
      return "";
  }
}

static void handleSettingsGet() {
  const AppConfig &cfg = getAppConfig();
  String b;
  appendFlashFromQuery(b);

  b += F("<section><h2>应用</h2>"
         "<p class=hint>点击卡片切换是否在设备上启用。上下键只在已启用应用间切换；至少保留一个。</p>"
         "<div class=app-grid>");
  const int n = static_cast<int>(AppId::Count);
  for (int i = 0; i < n; i++) {
    const AppId id = static_cast<AppId>(i);
    const bool on = appIdEnabled(id);
    b += F("<form method=POST action=/apps style=margin:0>"
           "<input type=hidden name=id value=");
    b += String(i);
    b += F("><button type=submit class='app-card ");
    b += on ? F("on") : F("off");
    b += F("'><span class=ico>");
    b += appCardSvg(id);
    b += F("</span><p class=name>");
    b += appIdLabelZh(id);
    b += F("</p><p class=desc>");
    b += appIdHintZh(id);
    b += F("</p><span class=st>");
    b += on ? F("已启用 · 点击关闭") : F("已关闭 · 点击启用");
    b += F("</span></button></form>");
  }
  b += F("</div></section>");

  b += F("<section><h2>电源</h2>"
         "<p class=hint>无操作自动关机是本固件自己的计时，不是芯片默认。按下任意键会重新计时。"
         "开启 Web 配置期间不会自动关机，方便慢慢配网。"
         "「连接电源时自动关机」关闭后，充电/充满（接 USB）时不会因空闲关机。</p>"
         "<form method=POST action=/power>"
         "<label>无操作自动关机</label>"
         "<select name=idle_off_sec>");
  const uint32_t idle_opts[] = {0, 60, 180, 300, 600, 900};
  const char *idle_labs[] = {"关闭", "1 分钟", "3 分钟", "5 分钟", "10 分钟", "15 分钟"};
  bool idle_hit = false;
  for (int i = 0; i < 6; i++) {
    b += F("<option value=");
    b += String(idle_opts[i]);
    if (cfg.idle_off_sec == idle_opts[i]) {
      b += F(" selected");
      idle_hit = true;
    }
    b += F(">");
    b += idle_labs[i];
    b += F("</option>");
  }
  if (!idle_hit) {
    b += F("<option value=");
    b += String(cfg.idle_off_sec);
    b += F(" selected>当前 ");
    b += String(cfg.idle_off_sec);
    b += F(" 秒</option>");
  }
  b += F("</select>");
  b += F("<label>连接电源时自动关机</label><select name=idle_off_on_power>");
  b += F("<option value=0");
  if (!cfg.idle_off_on_power) b += F(" selected");
  b += F(">关闭（接电保持开机）</option><option value=1");
  if (cfg.idle_off_on_power) b += F(" selected");
  b += F(">开启（接电也空闲关机）</option></select>");
  b += F("<label>日历一周起始</label><select name=week_start>");
  b += F("<option value=mon");
  if (cfg.week_start_monday) b += F(" selected");
  b += F(">周一</option><option value=sun");
  if (!cfg.week_start_monday) b += F(" selected");
  b += F(">周日</option></select>");
  b += F("<div class=btn-row><button type=submit>保存电源 / 日历设置</button></div></form></section>");

  b += F("<section><h2>时间 / 时区</h2>"
         "<p class=hint>板载 PCF8563 可在掉电后继续走时。复位后下次联网会校一次；平时按周期 NTP。"
         "离线可在下方手动设置本地时间。</p>");
  b += F("<form method=POST action=/time>");
  b += F("<label>时区（UTC 偏移）</label><select name=tz_offset_min>");
  static const int tz_opts[] = {-720, -600, -480, -300, 0, 60, 120, 180, 330, 480, 540, 600, 720};
  static const char *tz_labs[] = {"UTC-12", "UTC-10", "UTC-8", "UTC-5", "UTC+0", "UTC+1", "UTC+2",
                                  "UTC+3", "UTC+5:30", "UTC+8 中国", "UTC+9", "UTC+10", "UTC+12"};
  bool tz_hit = false;
  for (int i = 0; i < 13; i++) {
    b += F("<option value=");
    b += String(tz_opts[i]);
    if (cfg.tz_offset_min == tz_opts[i]) {
      b += F(" selected");
      tz_hit = true;
    }
    b += F(">");
    b += tz_labs[i];
    b += F("</option>");
  }
  if (!tz_hit) {
    b += F("<option value=");
    b += String(cfg.tz_offset_min);
    b += F(" selected>自定义 ");
    b += String(cfg.tz_offset_min);
    b += F(" 分</option>");
  }
  b += F("</select>");
  b += F("<label>NTP 周期</label><select name=ntp_interval_days>");
  const uint16_t ntp_opts[] = {30, 14, 7};
  const char *ntp_labs[] = {"30 天", "14 天", "7 天"};
  for (int i = 0; i < 3; i++) {
    b += F("<option value=");
    b += String(ntp_opts[i]);
    if (cfg.ntp_interval_days == ntp_opts[i]) b += F(" selected");
    b += F(">");
    b += ntp_labs[i];
    b += F("</option>");
  }
  b += F("</select>");
  b += F("<div class=btn-row>"
         "<button type=submit name=action value=save_cfg>保存时区 / 周期</button>"
         "<button type=submit name=action value=ntp_now class=secondary>立即 NTP 校时</button>"
         "</div>");
  b += F("<label>手动设置本地时间</label>");
  b += F("<input type=datetime-local name=manual_time step=1");
  RtcDateTime now;
  if (rtcRead(now)) {
    char v[32];
    snprintf(v, sizeof(v), " value=%04d-%02d-%02dT%02d:%02d:%02d", now.year, now.month, now.day,
             now.hour, now.minute, now.second);
    b += v;
  }
  b += F(">");
  b += F("<div class=btn-row><button type=submit name=action value=set_manual>写入 RTC</button></div>");
  b += F("</form></section>");
  sendHtml(NavTab::Settings, b);
}

static void handleAppsPost() {
  const uint8_t id = (uint8_t)g_server.arg("id").toInt();
  const bool currently = (getAppConfig().apps_enabled & (1u << id)) != 0;
  if (!setAppEnabled(id, !currently)) {
    redirectOk("/settings", "apps_last");
    return;
  }
  redirectOk("/settings", "apps");
}

static void handlePowerPost() {
  const uint32_t sec = (uint32_t)g_server.arg("idle_off_sec").toInt();
  saveIdleOffSec(sec);
  powerIdleSetTimeoutSec(sec);
  saveIdleOffOnPower(g_server.arg("idle_off_on_power") == "1");
  const String ws = g_server.arg("week_start");
  saveWeekStartMonday(ws != "sun");
  redirectOk("/settings", "power");
}

static void handleTimePost() {
  const String action = g_server.arg("action");
  if (action == "save_cfg" || action == "ntp_now") {
    saveTimezoneOffsetMin((int16_t)g_server.arg("tz_offset_min").toInt());
    saveNtpIntervalDays((uint16_t)g_server.arg("ntp_interval_days").toInt());
    rtcApplyTimezone();
  }
  if (action == "ntp_now") {
    bool ok = false;
    if (wifiIsStaConnected()) {
      ok = rtcSyncFromNtp(10000);
    } else if (hasSavedWifiConfig()) {
      if (wifiConnectSaved(15000)) ok = rtcSyncFromNtp(10000);
    }
    redirectOk("/settings", ok ? "ntp" : "ntp_fail");
    return;
  } else if (action == "set_manual") {
    // "2026-11-20T10:30" or with seconds
    const String s = g_server.arg("manual_time");
    if (s.length() >= 16) {
      RtcDateTime dt;
      dt.ok = true;
      dt.year = s.substring(0, 4).toInt();
      dt.month = s.substring(5, 7).toInt();
      dt.day = s.substring(8, 10).toInt();
      dt.hour = s.substring(11, 13).toInt();
      dt.minute = s.substring(14, 16).toInt();
      dt.second = (s.length() >= 19) ? s.substring(17, 19).toInt() : 0;
      dt.weekday = -1;
      rtcWrite(dt);
      Serial.printf("[RTC] manual set %04d-%02d-%02d %02d:%02d:%02d\n", dt.year, dt.month, dt.day,
                    dt.hour, dt.minute, dt.second);
    }
    // still allow saving tz if submitted together
    if (g_server.hasArg("tz_offset_min")) {
      saveTimezoneOffsetMin((int16_t)g_server.arg("tz_offset_min").toInt());
      rtcApplyTimezone();
    }
    redirectOk("/settings", "rtc");
    return;
  }
  redirectOk("/settings", "tz");
}

static void handleWifiGet() {
  const AppConfig &cfg = getAppConfig();
  String b;
  appendFlashFromQuery(b);
  b += F("<section><h2>WiFi（最多 5 条）</h2>");
  b += F("<p class=hint>大字为 SSID，下方小字为密码。保存后按 Active → 其它条目自动尝试连接。</p>");
  b += F("<form method=POST action=/wifi>");
  for (int i = 0; i < WIFI_PROFILE_MAX; i++) {
    const char *ssid = i < cfg.wifi_count ? cfg.wifis[i].ssid : "";
    const char *pass = i < cfg.wifi_count ? cfg.wifis[i].password : "";
    const bool active = ssid[0] && strcmp(ssid, cfg.wifi_active) == 0;
    b += F("<div class='wifi-card");
    if (active) b += F(" active");
    b += F("'><div class=wifi-top>"
          "<label class=wifi-active><input type=radio name=active value=");
    b += String(i);
    if (active) b += F(" checked");
    b += F("> Active</label><span class=wifi-idx>#");
    b += String(i + 1);
    b += F("</span></div>");
    b += F("<input class=wifi-ssid name=ssid");
    b += String(i);
    b += F(" value=\"");
    b += htmlEscapeAttr(ssid);
    b += F("\" placeholder=\"SSID\" autocomplete=off spellcheck=false>");
    b += F("<input class=wifi-pass name=pass");
    b += String(i);
    b += F(" type=password value=\"");
    b += htmlEscapeAttr(pass);
    b += F("\" placeholder=\"密码\" autocomplete=new-password>");
    b += F("</div>");
  }
  b += F("<div class=btn-row><button type=submit>保存 WiFi</button></div></form></section>");
  sendHtml(NavTab::Wifi, b);
}

static void handleWifiPost() {
  // Rebuild wifi list from form fields
  AppConfig &mutable_view = const_cast<AppConfig &>(getAppConfig());
  // We'll reconstruct via JSON path for simplicity
  JsonDocument doc;
  JsonArray arr = doc["wifis"].to<JsonArray>();
  int active_idx = g_server.hasArg("active") ? g_server.arg("active").toInt() : 0;
  String active_ssid;
  for (int i = 0; i < WIFI_PROFILE_MAX; i++) {
    const String ssid = g_server.arg(String("ssid") + i);
    const String pass = g_server.arg(String("pass") + i);
    String s = ssid;
    s.trim();
    if (s.isEmpty()) continue;
    JsonObject w = arr.add<JsonObject>();
    w["ssid"] = s;
    w["password"] = pass;
    if (i == active_idx) active_ssid = s;
  }
  if (active_ssid.isEmpty() && arr.size() > 0) {
    active_ssid = arr[0]["ssid"].as<String>();
  }
  doc["wifi_active"] = active_ssid;
  // preserve portal + qweather
  const AppConfig &cfg = getAppConfig();
  JsonObject portal = doc["portal"].to<JsonObject>();
  portal["base_url"] = cfg.portal_base_url;
  portal["email"] = cfg.portal_email;
  portal["token"] = cfg.portal_token;
  portal["diary_id"] = cfg.diary_id;
  JsonObject qw = doc["qweather"].to<JsonObject>();
  qw["enabled"] = cfg.qweather_enabled;
  qw["host"] = cfg.qweather_host;
  qw["key"] = cfg.qweather_key;
  qw["location_id"] = cfg.qweather_location_id;
  qw["location_name"] = cfg.qweather_location_name;
  doc["idle_off_sec"] = cfg.idle_off_sec;
  doc["album_show_header"] = cfg.album_show_header;

  String json;
  serializeJson(doc, json);
  appConfigFromJson(json.c_str());
  saveAppConfig();
  (void)mutable_view;
  redirectOk("/wifi", "wifi");
}

static void appendAuthForm(String &b, const AppConfig &cfg) {
  b += F("<section><h2>Portal 鉴权</h2>");
  b += F("<p class=hint>登录 <code>POST /user/login</code> 保存 JWT；响应头 "
         "<code>X-Access-Token</code>（剩余 &lt; 7 天）会自动续签。</p>");
  b += F("<form method=POST action=/auth>");
  b += F("<label>API Base URL</label><input name=base_url value=\"");
  b += htmlEscapeAttr(cfg.portal_base_url);
  b += F("\">");
  b += F("<label>邮箱</label><input name=email type=email value=\"");
  b += htmlEscapeAttr(cfg.portal_email);
  b += F("\">");
  b += F("<label>密码（仅本次登录，不落盘）</label><input name=password type=password autocomplete=current-password>");
  b += F("<label>Diary ID</label><input name=diary_id value=\"");
  b += htmlEscapeAttr(cfg.diary_id);
  b += F("\">");
  b += F("<div class=btn-row>");
  b += F("<button type=submit name=action value=save>保存 URL / ID</button>");
  b += F("<button type=submit name=action value=login>登录</button>");
  b += F("<button type=submit name=action value=logout class=danger>清除 Token</button>");
  b += F("</div></form></section>");
}

static void handleAuthGet() {
  String b;
  appendAuthForm(b, getAppConfig());
  sendHtml(NavTab::Auth, b);
}

static void handleAuthPost() {
  const String action = g_server.arg("action");
  const String base = g_server.arg("base_url");
  const String email = g_server.arg("email");
  const String password = g_server.arg("password");
  const String diary_id = g_server.arg("diary_id");

  if (base.length()) savePortalBaseUrl(base.c_str());
  if (diary_id.length()) saveDiaryId(diary_id.c_str());

  String b;
  if (action == "login") {
    String err;
    if (diaryLogin(email.c_str(), password.c_str(), err)) {
      b += F("<div class='flash ok'>登录成功，Token 已保存</div>");
    } else {
      b += F("<div class='flash err'>登录失败：");
      b += err;
      b += F("</div>");
    }
  } else if (action == "logout") {
    savePortalAuth(email.c_str(), "");
    b += F("<div class='flash ok'>已清除 Token</div>");
  } else {
    if (email.length()) savePortalAuth(email.c_str(), getAppConfig().portal_token);
    b += F("<div class='flash ok'>已保存</div>");
  }
  appendAuthForm(b, getAppConfig());
  sendHtml(NavTab::Auth, b);
}

static void handleTodoGet() {
  String b;
  b += F("<section><h2>待办预览</h2>");
  b += F("<p class=hint>从 portal <code>GET /diary/detail</code> 拉取，需「同 WiFi 站点」已连外网并登录。</p>");
  b += F("<form method=POST action=/todo><div class=btn-row>"
         "<button type=submit>立即拉取</button></div></form></section>");
  sendHtml(NavTab::Todo, b);
}

static void handleTodoPost() {
  String b;
  if (!wifiIsStaConnected()) {
    b += F("<section><div class='flash err'>需要「同 WiFi 站点」已连接外网才能拉日记</div>");
    b += F("<form method=POST action=/todo><div class=btn-row>"
           "<button type=submit>重试</button></div></form></section>");
    sendHtml(NavTab::Todo, b);
    return;
  }
  DiaryDetail d;
  if (!diaryFetchById(nullptr, d)) {
    b += F("<section><div class='flash err'>");
    b += d.message;
    b += F("</div><form method=POST action=/todo><div class=btn-row>"
           "<button type=submit>重试</button></div></form></section>");
  } else {
    TodoItem items[TODO_ITEM_MAX];
    const int n = todoParseContent(d.content, items, TODO_ITEM_MAX);
    int open_n = 0;
    for (int i = 0; i < n; i++) {
      if (!items[i].done) open_n++;
    }
    b += F("<div class='flash ok'>待办已拉取</div>");
    b += F("<section><h2>");
    b += d.title.length() ? d.title : String("Todo");
    b += F("</h2><p class=meta>#");
    b += d.id;
    b += F(" · ");
    b += d.date;
    b += F(" · ");
    b += d.category;
    b += F(" · ");
    b += String(open_n);
    b += F("/");
    b += String(n);
    b += F(" open</p>");
    if (n == 0) {
      b += F("<pre>");
      String c = d.content;
      c.replace("&", "&amp;");
      c.replace("<", "&lt;");
      b += c;
      b += F("</pre>");
    } else {
      b += F("<ul class=todo-list>");
      for (int i = 0; i < n; i++) {
        b += F("<li");
        if (items[i].done) b += F(" class=done");
        b += F("><span class=box></span><div class=txt>");
        String c = items[i].content;
        c.replace("&", "&amp;");
        c.replace("<", "&lt;");
        b += c;
        if (items[i].note.length()) {
          b += F("<span class=note>");
          String note = items[i].note;
          note.replace("&", "&amp;");
          note.replace("<", "&lt;");
          b += note;
          b += F("</span>");
        }
        b += F("</div></li>");
      }
      b += F("</ul>");
    }
    b += F("<form method=POST action=/todo><div class=btn-row style=margin-top:16px>"
           "<button type=submit>再拉一次</button></div></form></section>");
  }
  sendHtml(NavTab::Todo, b);
}

static void appendWeatherForm(String &b, const AppConfig &cfg, const WeatherGeoResult *geo,
                              const char *flash_html) {
  if (flash_html && flash_html[0]) b += flash_html;
  b += F("<section><h2>天气位置</h2>");
  b += F("<p class=hint>设备经 <strong>Portal</strong> 拉天气（和风官方路径 "
         "<code>/v7/weather/now|24h|7d</code>、<code>/geo/v2/city/lookup</code>），"
         "和风 JWT/Key 在服务器。请先在「鉴权」登录，再用下方搜索选位置。</p>");
  b += F("<form method=POST action=/weather>");
  b += F("<label class='check-row");
  if (cfg.qweather_enabled) b += F(" on");
  b += F("'><input type=checkbox name=enabled value=1");
  if (cfg.qweather_enabled) b += F(" checked");
  b += F("><span>启用天气</span></label>");
  b += F("<label>当前 LocationID</label><input name=location_id value=\"");
  b += htmlEscapeAttr(cfg.qweather_location_id);
  b += F("\" readonly>");
  b += F("<label>位置名称</label><input name=location_name value=\"");
  b += htmlEscapeAttr(cfg.qweather_location_name);
  b += F("\" readonly>");
  b += F("<div class=btn-row><button type=submit name=action value=save>保存开关</button></div>");
  b += F("</form></section>");

  {
    uint32_t d = 0, h = 0, m = 0;
    weatherRefreshToDhm(cfg.weather_refresh_sec, d, h, m);
    b += F("<section><h2>自动刷新</h2>"
           "<p class=hint>仅当设备<strong>停留在天气 App</strong>且未开配置页时，到期才自动拉数并刷屏；停留在其它 App 时不抢切。"
           "填 <strong>0 天 0 时 0 分</strong> 关闭；开启时合计至少 <strong>1 分钟</strong>。"
           "请把「无操作自动关机」设为关闭或长于本间隔，否则会先睡过去。</p>"
           "<form method=POST action=/weather>"
           "<div class=btn-row style=align-items:flex-end;gap:10px>"
           "<label style=margin:0>天<br><input name=refresh_d type=number min=0 max=365 step=1 value=");
    b += String(d);
    b += F(" style=width:5.5em></label>"
           "<label style=margin:0>时<br><input name=refresh_h type=number min=0 max=23 step=1 value=");
    b += String(h);
    b += F(" style=width:5.5em></label>"
           "<label style=margin:0>分<br><input name=refresh_m type=number min=0 max=59 step=1 value=");
    b += String(m);
    b += F(" style=width:5.5em></label>"
           "<button type=submit name=action value=refresh>保存间隔</button>"
           "</div></form></section>");
  }

  b += F("<section><h2>搜索位置</h2>");
  b += F("<p class=hint>经 Portal <code>/geo/v2/city/lookup</code> 调和风城市搜索；需「同 WiFi 站点」已连接并登录。</p>");
  b += F("<form method=POST action=/weather>");
  b += F("<label>城市 / 区县名</label><input name=geo_query placeholder=\"例如 杭州 或 西湖\">");
  b += F("<div class=btn-row><button type=submit name=action value=search>搜索</button></div>");
  b += F("</form>");

  if (geo && geo->ok && geo->count > 0) {
    b += F("<form method=POST action=/weather><ul class=geo-list>");
    for (int i = 0; i < geo->count; i++) {
      const WeatherGeoItem &it = geo->items[i];
      b += F("<li><label><input type=radio name=pick value=\"");
      b += htmlEscapeAttr(it.id.c_str());
      if (i == 0) b += F("\" checked");
      else b += F("\"");
      b += F("><span><strong>");
      String name = it.name;
      name.replace("&", "&amp;");
      name.replace("<", "&lt;");
      b += name;
      b += F("</strong> <code>");
      b += it.id;
      b += F("</code><span class=sub>");
      String sub = it.adm1;
      if (it.adm2.length()) {
        sub += " · ";
        sub += it.adm2;
      }
      if (it.country.length()) {
        sub += " · ";
        sub += it.country;
      }
      sub.replace("&", "&amp;");
      sub.replace("<", "&lt;");
      b += sub;
      b += F("</span></span></label>");
      // stash name in hidden field keyed by id
      b += F("<input type=hidden name=name_");
      b += it.id;
      b += F(" value=\"");
      b += htmlEscapeAttr(it.name.c_str());
      b += F("\">");
      b += F("</li>");
    }
    b += F("</ul><div class=btn-row>"
           "<button type=submit name=action value=pick>选用所选位置</button></div></form>");
  } else if (geo && !geo->ok && geo->message.length()) {
    b += F("<div class='flash err'>");
    b += geo->message;
    b += F("</div>");
  }
  b += F("</section>");
}

static void handleWeatherGet() {
  String b;
  appendWeatherForm(b, getAppConfig(), nullptr, nullptr);
  sendHtml(NavTab::Weather, b);
}

static void handleWeatherPost() {
  const String action = g_server.arg("action");
  String flash;
  WeatherGeoResult geo;

  if (action == "save") {
    const bool enabled = g_server.hasArg("enabled");
    const AppConfig &cfg = getAppConfig();
    saveQWeatherConfig(enabled, cfg.qweather_host, cfg.qweather_key, cfg.qweather_location_id,
                       cfg.qweather_location_name);
    flash = F("<div class='flash ok'>天气开关已保存</div>");
  } else if (action == "refresh") {
    const uint32_t d = (uint32_t)g_server.arg("refresh_d").toInt();
    const uint32_t h = (uint32_t)g_server.arg("refresh_h").toInt();
    const uint32_t m = (uint32_t)g_server.arg("refresh_m").toInt();
    const uint32_t sec = weatherRefreshSecFromDhm(d, h, m);
    saveWeatherRefreshSec(sec);
    if (sec == 0) {
      flash = F("<div class='flash ok'>已关闭天气自动刷新</div>");
    } else {
      uint32_t od = 0, oh = 0, om = 0;
      weatherRefreshToDhm(sec, od, oh, om);
      flash = F("<div class='flash ok'>自动刷新：");
      flash += String(od);
      flash += F(" 天 ");
      flash += String(oh);
      flash += F(" 时 ");
      flash += String(om);
      flash += F(" 分</div>");
    }
  } else if (action == "search") {
    const String q = g_server.arg("geo_query");
    if (!weatherGeoLookup(q.c_str(), geo)) {
      flash = F("<div class='flash err'>搜索失败");
      if (geo.message.length()) {
        flash += F("：");
        flash += geo.message;
      }
      flash += F("</div>");
    } else {
      flash = F("<div class='flash ok'>搜索完成，找到 ");
      flash += String(geo.count);
      flash += F(" 个位置</div>");
    }
  } else if (action == "pick") {
    const String id = g_server.arg("pick");
    const String name_key = String("name_") + id;
    const String name = g_server.hasArg(name_key) ? g_server.arg(name_key) : id;
    const AppConfig &cfg = getAppConfig();
    saveQWeatherConfig(true, cfg.qweather_host, cfg.qweather_key, id.c_str(), name.c_str());
    flash = F("<div class='flash ok'>已选用位置 ");
    flash += name;
    flash += F("（");
    flash += id;
    flash += F("）</div>");
  }

  String b;
  appendWeatherForm(b, getAppConfig(), (action == "search") ? &geo : nullptr, flash.c_str());
  sendHtml(NavTab::Weather, b);
}

static void handleAlbumGet() {
  albumEnsureDir();
  String b;
  b.reserve(10000);
  webAlbumAppendPage(b);
  sendHtml(NavTab::Album, b, webAlbumSendScript);
}

static void handleApiConfigGet() {
  g_server.send(200, "application/json", appConfigToJson(true));
}

static void handleFavicon() {
  File f = LittleFS.open("/favicon.svg", "r");
  if (!f) {
    g_server.send(404, "text/plain", "favicon missing — upload LittleFS");
    return;
  }
  g_server.streamFile(f, "image/svg+xml");
  f.close();
}

static void handleHelpGet() {
  String b;
  b += F("<section><h2>帮助</h2>"
         "<p class=hint style=margin:0>按键、电源与状态灯含义。长按上键约 1.5s 开配置网页；"
         "长按下键约 3s 关机（深睡，再按下键唤醒）。</p></section>");

  b += F("<section><h2>LED 含义</h2>"
         "<p class=hint>板载状态灯（GPIO3，低电平点亮）。空闲默认熄灭；"
         "任意按键按下会强制常亮，松开后恢复下方图案。左侧圆点为示意动画。</p>"
         "<ul class=led-list>");

  auto row = [&](const char *anim, const char *name, const char *desc, const char *when) {
    b += F("<li><div class=led-dot-wrap><span class='led-dot ");
    b += anim;
    b += F("'></span></div><div class=led-meta><p class=name>");
    b += name;
    b += F("</p><p class=desc>");
    b += desc;
    b += F("</p><span class=when>");
    b += when;
    b += F("</span></div></li>");
  };

  row("led-anim-solid", "常亮", "按键按下期间灯一直亮着，松开才灭。", "BOOT / UP / DOWN 任一按下");
  row("led-anim-boot", "上电三闪", "快闪 3 下后熄灭，表示固件已启动（上电不刷墨水屏）。", "开机 / 深睡唤醒");
  row("led-anim-ack", "长按确认两闪", "长按阈值到达时闪两下，可松手；随后执行对应动作。", "UP≈1.5s 开 Web · DOWN≈3s 关机");
  row("led-anim-slow", "慢闪", "约 400ms 亮灭交替。", "正在连 WiFi / 启动配置网页");
  row("led-anim-fast", "快闪", "约 120ms 亮灭交替。", "网络拉取等短时忙碌（如有）");
  row("led-anim-done", "刷完一闪", "全刷结束后短亮一下再灭；刷屏过程中不闪灯。", "切 App / BOOT 刷新 / 定时刷新完成");
  row("led-anim-double", "双脉冲", "两短亮一停，重复约 3 次后自动灭。", "配置 Web / 热点就绪提示");
  row("led-anim-error", "三短闪（失败）", "连续三下短亮，约 2s 内结束并熄灭。", "刷屏失败、开网页失败等");
  row("off", "熄灭", "无图案时灯关闭。", "空闲待机");

  b += F("</ul></section>");
  b += F("<section><h2>怎么记</h2>"
         "<p class=hint style=margin:0>按下有灯 → 松开看图案。"
         "慢闪在联网，刷完一闪表示页面已更新，三短闪是出错，上电三闪表示已开机。</p></section>");
  sendHtml(NavTab::Help, b);
}

static void handleLedRedirect() {
  g_server.sendHeader("Location", "/help");
  g_server.send(302);
}

static void handleErrorsGet() {
  String b;
  appendFlashFromQuery(b);
  b += F("<section><div class=status-hero><div><h2 style=margin:0>错误记录</h2>"
         "<p class=hint style=margin:6px 0 0>设备端最近事件（最多 ");
  b += String(ERROR_LOG_MAX);
  b += F(" 条），存于 LittleFS。墨水屏出错时也会写一条。</p></div>");
  const int n = errorLogCount();
  if (n > 0) {
    b += F("<span class='pill warn'>");
    b += String(n);
    b += F(" 条</span>");
  } else {
    b += F("<span class='pill off'>空</span>");
  }
  b += F("</div>");
  errorLogAppendHtml(b);
  b += F("<form method=POST action=/errors/clear class=btn-row style=margin-top:16px>"
         "<button type=submit class=danger>清空记录</button></form></section>");
  sendHtml(NavTab::Errors, b);
}

static void handleErrorsClear() {
  errorLogClear();
  redirectOk("/errors", "err_cleared");
}

static void registerRoutes() {
  g_server.on("/", HTTP_GET, handleRoot);
  g_server.on("/settings", HTTP_GET, handleSettingsGet);
  g_server.on("/help", HTTP_GET, handleHelpGet);
  g_server.on("/led", HTTP_GET, handleLedRedirect);
  g_server.on("/apps", HTTP_POST, handleAppsPost);
  g_server.on("/power", HTTP_POST, handlePowerPost);
  g_server.on("/time", HTTP_POST, handleTimePost);
  g_server.on("/favicon.svg", HTTP_GET, handleFavicon);
  g_server.on("/wifi", HTTP_GET, handleWifiGet);
  g_server.on("/wifi", HTTP_POST, handleWifiPost);
  g_server.on("/auth", HTTP_GET, handleAuthGet);
  g_server.on("/auth", HTTP_POST, handleAuthPost);
  g_server.on("/todo", HTTP_GET, handleTodoGet);
  g_server.on("/todo", HTTP_POST, handleTodoPost);
  g_server.on("/weather", HTTP_GET, handleWeatherGet);
  g_server.on("/weather", HTTP_POST, handleWeatherPost);
  g_server.on("/album", HTTP_GET, handleAlbumGet);
  g_server.on("/errors", HTTP_GET, handleErrorsGet);
  g_server.on("/errors/clear", HTTP_POST, handleErrorsClear);
  webAlbumRegisterRoutes(g_server);
  g_server.on("/api/config", HTTP_GET, handleApiConfigGet);
}

bool webStart(bool force_ap, bool allow_ap_fallback) {
  webStop();
  g_ap = false;
  bool ok = false;

  if (!force_ap && hasSavedWifiConfig()) {
    Serial.println("[Web] try WiFi...");
    wifiStopAp();
    ok = wifiConnectSaved(15000);
  }

  if (!ok && (force_ap || allow_ap_fallback)) {
    Serial.printf("[Web] start AP %s / %s\n", WIFI_AP_SSID, WIFI_AP_PASS);
    ok = wifiStartAp(WIFI_AP_SSID, WIFI_AP_PASS);
    g_ap = ok;
  }

  if (!ok) {
    Serial.println("[Web] WiFi and AP both failed");
    errorLog("web", "配置网页启动失败：同 WiFi 站点与 AP 模式均不可用（检查热点密码≥8位）");
    return false;
  }

  registerRoutes();
  g_server.begin();
  g_running = true;
  snprintf(g_url, sizeof(g_url), "http://%s",
           g_ap ? wifiApIp().c_str() : wifiStaIp().c_str());
  Serial.printf("[Web] %s mode url=%s\n", g_ap ? "AP" : "STA", g_url);
  return true;
}

void webLoop() {
  if (g_running) g_server.handleClient();
}

void webStop() {
  if (g_running) {
    g_server.stop();
    g_running = false;
  }
  g_url[0] = '\0';
}

bool webIsRunning() { return g_running; }
const char *webUrl() { return g_url; }
bool webIsAp() { return g_ap; }
const char *webModeName() { return g_ap ? "AP 模式" : "同 WiFi 站点"; }
