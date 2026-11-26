#include "app_web.h"
#include "app_config.h"
#include "app_apps.h"
#include "app_chess.h"
#include "app_quote.h"
#include "app_schedule.h"
#include "app_todo_store.h"
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
#include "app_battery.h"

#include <WebServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <esp_system.h>
#include <cstdio>
#include <cstring>
#include <new>

static WebServer g_server(80);
static bool g_running = false;
static bool g_ap = false;
static char g_url[40] = "";

enum class NavTab : uint8_t {
  Status,
  Settings,
  Help,
  Wifi,
  Auth,
  Todo,
  Weather,
  Album,
  Chess,
  Quote,
  Schedule,
  Errors
};

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
header nav .nav-sep{width:1px;height:14px;margin:0 6px;background:rgba(255,255,255,.22);flex-shrink:0;align-self:center}
header .meta{margin:0 0 0 auto;padding:0;display:inline-flex;align-items:center;align-self:center;gap:10px;flex-shrink:0;line-height:1;height:28px;min-height:28px;color:inherit}
header .meta .ver{font-size:12px;color:rgba(255,255,255,.55);font-variant-numeric:tabular-nums;letter-spacing:.02em;white-space:nowrap;line-height:1;height:28px;display:inline-flex;align-items:center}
header .gh{display:inline-flex;align-items:center;justify-content:center;color:rgba(255,255,255,.7);line-height:0;width:28px;height:28px;flex-shrink:0}
header .gh svg{display:block;flex-shrink:0}
header .gh:hover{color:#fff}
/* 窄屏：标题 + GitHub 置顶一行，功能导航整行在下 */
@media(max-width:720px){
  header{display:grid;grid-template-columns:1fr auto;grid-template-areas:"brand meta" "nav nav";align-items:center;gap:10px 12px;padding:12px 14px}
  header .brand{grid-area:brand;min-width:0}
  header .meta{grid-area:meta;margin:0}
  header nav{grid-area:nav;flex:none;width:100%;min-width:0}
  header nav .nav-sep{margin:0 2px}
}
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
.meters{display:flex;flex-direction:column;gap:14px;margin:0}
.meter{display:flex;flex-direction:column;gap:6px;min-width:0}
.meter-h{display:flex;align-items:baseline;justify-content:space-between;gap:10px;flex-wrap:wrap}
.meter-h .lab{font-size:13px;font-weight:650;color:#1a1a1a}
.meter-h .val{font-size:12px;color:#666;font-variant-numeric:tabular-nums;text-align:right}
.meter-track{height:10px;border-radius:5px;background:#eceae4;overflow:hidden}
.meter-fill{height:100%;border-radius:5px;background:#1a1a1a;width:0;max-width:100%;transition:width .2s}
.meter-fill.ok{background:#0a7a4b}
.meter-fill.warn{background:#c98500}
.meter-fill.bad{background:#8b2010}
.meter-note{margin:0;font-size:12px;color:#888}
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
.set-step{display:inline-flex;align-items:center;justify-content:center;width:22px;height:22px;margin-right:8px;border-radius:50%;background:#1a1a1a;color:#fff;font-size:12px;font-weight:700;vertical-align:middle}
.set-status{display:flex;flex-wrap:wrap;align-items:center;gap:8px 12px;padding:14px 16px;margin:0 0 16px;border-radius:10px;background:#f7f7f5;border:1px solid #ebebe8}
.set-status .lbl{font-size:12px;color:#888;font-weight:500}
.set-status .name{font-size:18px;font-weight:650;color:#1a1a1a}
.set-status .id{font-size:12px;color:#888}
.set-status.empty .name{font-size:14px;font-weight:500;color:#888}
.wx-search-row{display:flex;gap:8px;align-items:stretch;margin-top:4px}
.wx-search-row input{flex:1;margin:0}
.wx-search-row button{flex-shrink:0;margin:0;white-space:nowrap}
.check-row{display:flex;align-items:center;gap:12px;padding:14px 16px;border:2px solid #1a1a1a;border-radius:10px;background:#fff;cursor:pointer;margin:4px 0 16px;user-select:none}
.check-row input{width:22px;height:22px;margin:0;accent-color:#0a7a4b;flex-shrink:0}
.check-row span{font-size:16px;font-weight:650;color:#1a1a1a}
.check-row.on,.check-row:has(input:checked){background:#e8f7ef;border-color:#0a7a4b}
.check-row.on span,.check-row:has(input:checked) span{color:#0a7a4b}
body.page-album{overflow-x:auto;overflow-y:auto;min-height:100vh;background:#e8e6e0}
body.page-album main{max-width:none;width:100%;margin:0;padding:0;display:block}
body.page-quote main{max-width:1100px}
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
.ed-body{flex:0 0 auto;display:grid;grid-template-columns:1fr;min-width:0;overflow:visible;align-items:start;padding-top:0;background:#f0eee8}
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
.ed-side{background:#fff;padding:8px 20px 16px;overflow:auto;min-height:0;min-width:320px;border-left:1px solid #d0cec8;display:flex;flex-direction:column;gap:30px;align-self:stretch;container-type:inline-size;container-name:edside}
.ed-mode{display:inline-flex;gap:0;border:1px solid #111;align-self:flex-start;width:auto}
.ed-side-ops{display:grid;grid-template-columns:1fr;gap:30px;min-width:0}
.ed-levels{min-width:0}
@container edside (min-width:600px){.ed-side-ops{grid-template-columns:1fr 1fr;align-items:start}}
.ed-side button{padding:4px 8px;font-size:12px;border-radius:0}
.ed-mode button{flex:0 0 auto;border:0;border-radius:0;padding:7px 14px;font-size:13px;background:#eceae4;color:#1a1a1a}
.ed-mode button.on{background:#1a1a1a;color:#fff}
.ed-block{padding:0}
.ed-maps{width:800px;max-width:100%;box-sizing:border-box;padding:10px 12px 16px;border-top:1px solid #d0cec8;background:#fff}
.ed-maps button{padding:4px 8px;font-size:12px;border-radius:0}
.ed-map-bar{display:flex;align-items:center;gap:8px;flex-wrap:wrap}
.ed-map-bar .ed-mixn{margin-left:auto;flex-wrap:wrap}
.ed-map-bar .ed-mixn button{padding:5px 8px;font-size:12px}
.ed-cmap.mix-dense{height:440px}
.ed-cmap.mix-dense .sw-mix{width:11px;height:11px}
.ed-cmap.mix-dense .sw-pure{width:20px;height:20px}
.ed-cmap.mix-dense .sw-drop{width:10px;height:10px}
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
#hist,#inkHist{width:100%;height:160px;display:block;background:#fff}
#hist{cursor:ew-resize}
.hist-read{display:flex;justify-content:space-between;font-size:11px;color:#888;margin-top:2px;min-height:16px;font-variant-numeric:tabular-nums}
.ink-hist-read{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));font-size:11px;line-height:1.3;color:#888;margin-top:2px;min-height:16px;font-variant-numeric:tabular-nums;-webkit-font-smoothing:antialiased}
.ink-hist-read span{min-width:0;text-align:center}
.ed-row{display:flex;align-items:center;gap:6px;margin:4px 0}
.ed-row span{flex:0 0 36px;font-size:12px;color:#555}
.ed-row input[type=range]{
  flex:1;padding:0;border:0;min-width:0;height:10px;border-radius:5px;
  -webkit-appearance:none;appearance:none;accent-color:#1a1a1a;
  background:#ccc;box-shadow:inset 0 0 0 1px rgba(0,0,0,.14);
}
.ed-row input[type=range]::-webkit-slider-thumb{
  -webkit-appearance:none;appearance:none;width:14px;height:14px;border-radius:50%;
  background:#fff;border:2px solid #1a1a1a;box-shadow:0 1px 2px rgba(0,0,0,.28);cursor:pointer;
}
.ed-row input[type=range]::-moz-range-thumb{
  width:14px;height:14px;border-radius:50%;background:#fff;border:2px solid #1a1a1a;
  box-shadow:0 1px 2px rgba(0,0,0,.28);cursor:pointer;
}
.ed-row input[type=range]::-moz-range-track{height:10px;border-radius:5px;background:inherit;border:0}
#brt{background:linear-gradient(90deg,#0a0a0a,#7a7a7a,#f2f2f2)}
#con{background:linear-gradient(90deg,#8a8a8a,#2a2a2a 45%,#efefef 55%,#111)}
#gma{background:linear-gradient(90deg,#111 0%,#666 35%,#bbb 65%,#fff 100%)}
#sat{background:linear-gradient(90deg,#9a9a9a,#c9b,#e55,#e8c44a,#5bbd5b,#4a9be8,#b56be0)}
#hue{background:linear-gradient(90deg,#e33,#e8c44a,#3a3,#3cc,#36c,#c3c,#e33)}
#tmp{background:linear-gradient(90deg,#3b6db3,#d9d7d0,#e0a020)}
#tint{background:linear-gradient(90deg,#2f9e5b,#d9d7d0,#c43b8c)}
#dith{background:linear-gradient(90deg,#d8d8d8,#9a9a9a 40%,#555 70%,#222)}
#inBlk{background:linear-gradient(90deg,#000,#6a6a6a)}
#inMid{background:linear-gradient(90deg,#1a1a1a,#808080,#f0f0f0)}
#inWht{background:linear-gradient(90deg,#8a8a8a,#fff)}
#outBlk{background:linear-gradient(90deg,#000,#5a5a5a)}
#outWht{background:linear-gradient(90deg,#b0b0b0,#fff)}
#yel{background:linear-gradient(90deg,#cfcfcf,#F6C749)}
#red{background:linear-gradient(90deg,#cfcfcf,#8b2010)}
#chr{background:linear-gradient(90deg,#e66,#c9a,#9a9a9a)}
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
.ed-album-bar{display:flex;flex-wrap:wrap;gap:8px;margin:0 0 12px;align-items:center}
.ed-album-panel{flex:0 1 auto;margin:0;padding:6px 10px;background:#e8e6e0;border:1px solid #d8d5ce;min-width:0;display:inline-flex;flex-wrap:wrap;align-items:center;gap:6px 8px}
.ed-album-panel h3{margin:0;font-size:11px;font-weight:700;color:#666;letter-spacing:.06em;flex:0 0 auto;min-width:2.5em}
.ed-album-panel .show-lab{margin:0;height:28px;padding:0 2px;display:inline-flex;align-items:center;font-size:12px}
.ed-sw{display:inline-flex;align-items:center;gap:8px;margin:0;height:28px;padding:0 10px;border:2px solid #0a7a4b;background:#e8f7ef;cursor:pointer;user-select:none;font-size:12px;font-weight:650;color:#0a7a4b;box-sizing:border-box}
.ed-sw input{width:16px;height:16px;margin:0;accent-color:#0a7a4b;flex-shrink:0;padding:0}
.ed-sw:has(input:checked),.ed-sw.on{background:#cfeedd;border-color:#08663f;color:#08663f}
.ed-album-acts{display:flex;align-items:center;gap:6px;flex-wrap:wrap;min-height:28px;flex:1 1 auto}
.ed-album-acts #btnExportAll,.ed-album-acts #btnDeleteSel,.ed-album-import,.ed-album-sched button{margin:0;height:28px;padding:0 10px;font-size:12px;font-weight:650;border-radius:0;line-height:1;display:inline-flex;align-items:center;justify-content:center;box-sizing:border-box}
.ed-album-import{position:relative;overflow:hidden;background:#1a1a1a;color:#fff;cursor:pointer;letter-spacing:.02em}
.ed-album-import:hover{background:#333}
.ed-album-import input{position:absolute;inset:0;opacity:0;cursor:pointer;width:100%;height:100%;padding:0;border:0;margin:0}
.ed-album-acts .ed-sep{width:1px;height:16px;background:#c8c4bc;margin:0 2px;flex-shrink:0}
.ed-album-sched{margin:0;padding:0;display:inline-flex;align-items:center;gap:6px;flex-wrap:wrap;flex:1 1 auto}
.ed-album-sched .ed-dhm{display:inline-flex;align-items:center;gap:6px}
.ed-album-sched .ed-dhm label{margin:0;display:inline-flex;align-items:center;gap:4px;font-size:11px;color:#555;font-weight:600}
.ed-album-sched input[type=number]{width:3.2em;padding:4px 6px;height:28px;box-sizing:border-box;font-size:12px;border-radius:0;border:1px solid #b0aea6;background:#fff;box-shadow:inset 0 1px 2px rgba(0,0,0,.1),0 0 0 1px rgba(0,0,0,.04);outline:none;-moz-appearance:textfield;appearance:textfield}
.ed-album-sched input[type=number]::-webkit-outer-spin-button,.ed-album-sched input[type=number]::-webkit-inner-spin-button{-webkit-appearance:none;margin:0}
.ed-album-sched input[type=number]:focus{border-color:#0a7a4b;box-shadow:inset 0 1px 2px rgba(0,0,0,.08),0 0 0 2px rgba(10,122,75,.22)}
.ed-saved{display:grid;grid-template-columns:repeat(auto-fill,minmax(200px,1fr));gap:10px;overflow:visible;padding-bottom:4px}
.thumb{border:2px solid #ccc;padding:6px;background:#fff;width:auto;min-width:0;box-sizing:border-box}
.thumb.on{border-color:#111}
.thumb.picked{border-color:#8b2010;box-shadow:inset 0 0 0 1px #8b2010}
.thumb.picked.on{border-color:#8b2010}
.thumb.off{filter:grayscale(1) opacity(.45)}
.thumb.off.picked{filter:none;opacity:1;border-color:#8b2010}
.thumb canvas{width:100%;height:auto;aspect-ratio:4/3;image-rendering:pixelated;display:block;background:#fff;cursor:pointer}
.thumb .row{display:flex;align-items:center;gap:6px;margin-top:6px;flex-wrap:wrap}
.thumb .row.meta-row{justify-content:space-between}
.thumb .row.pick-row{gap:10px}
.thumb .row.acts{margin-top:4px;gap:6px;flex-wrap:nowrap}
.thumb .row.acts button{margin:0;padding:4px 6px;font-size:11px;border-radius:0;flex:1 1 0;min-width:0;white-space:nowrap}
.thumb .row code{font-size:10px}
.show-lab{display:inline-flex;align-items:center;gap:6px;font-size:12px;font-weight:600;color:#1a1a1a;cursor:pointer;user-select:none}
.show-lab input{width:16px;height:16px;margin:0;accent-color:#0a7a4b;padding:0;flex-shrink:0}
.pick-lab input{accent-color:#8b2010}
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
.led-dot-wrap{width:44px;height:44px;border-radius:0;background:#111;display:flex;align-items:center;justify-content:center;flex-shrink:0}
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
.sched-day{position:relative;height:36px;margin:10px 0 18px;background:#eceae4;border-radius:6px}
.sched-day .q{position:absolute;top:0;bottom:0;background:rgba(139,32,16,.28);border-radius:0}
.sched-day .m{position:absolute;top:2px;bottom:2px;width:3px;background:#1a1a1a;border-radius:1px}
.sched-day .lab{position:absolute;left:0;right:0;top:calc(100% + 2px);display:flex;justify-content:space-between;font-size:10px;color:#888}
.sched-row{display:grid;grid-template-columns:1fr 1fr 1.2fr .9fr auto;gap:8px;align-items:end;margin:0 0 10px}
.sched-row.quiet{grid-template-columns:1fr 1fr auto}
.sched-row label{margin:0}
.quote-toolbar{display:flex;flex-wrap:wrap;gap:10px;align-items:center;justify-content:space-between;margin:0 0 14px}
.quote-toolbar .meta{margin:0}
.quote-toolbar .btn-row{margin:0}
.quote-layout{display:grid;grid-template-columns:minmax(200px,280px) 1fr;gap:0;border:1px solid #e8e8e8;border-radius:12px;overflow:hidden;min-height:440px;background:#fff}
@media(max-width:720px){.quote-layout{grid-template-columns:1fr;min-height:0}}
.quote-side{border-right:1px solid #e8e8e8;display:flex;flex-direction:column;min-height:0;background:#fafafa}
@media(max-width:720px){.quote-side{border-right:0;border-bottom:1px solid #e8e8e8;max-height:240px}}
.quote-side-h{padding:10px 12px;border-bottom:1px solid #e8e8e8;font-size:12px;color:#888;font-weight:650;letter-spacing:.02em}
.quote-list{list-style:none;margin:0;padding:0;overflow:auto;flex:1;max-height:560px}
.quote-list button{display:block;width:100%;text-align:left;border:0;border-radius:0;border-bottom:1px solid #eee;background:transparent;color:#1a1a1a;padding:12px 14px;cursor:pointer;font:inherit}
.quote-list button:hover{background:#f0eee8}
.quote-list button.on{background:#1a1a1a;color:#fff}
.quote-list .q-i{font-size:11px;opacity:.55;font-variant-numeric:tabular-nums}
.quote-list button.on .q-i{opacity:.85;color:#ffcc00}
.quote-list .q-t{display:block;margin-top:4px;font-size:13px;font-weight:650;line-height:1.35;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.quote-list .q-a{display:block;margin-top:3px;font-size:11px;opacity:.65;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.quote-list button.on .q-a{opacity:.8}
.quote-editor{padding:16px 18px;display:flex;flex-direction:column;min-width:0}
.quote-editor .q-ed-title{margin:0 0 2px;font-size:14px;font-weight:650}
.quote-editor .q-ed-hint{margin:0 0 12px;font-size:12px;color:#888}
.quote-editor label{margin-top:10px}
.quote-editor textarea{min-height:180px;resize:vertical;line-height:1.55;font-size:15px}
.quote-editor .q-ed-empty{display:none;margin:28px 0;text-align:center;color:#888;font-size:13px;line-height:1.5}
.quote-editor.is-empty .q-ed-form{display:none}
.quote-editor.is-empty .q-ed-empty{display:block}
.quote-editor.is-empty .q-ed-actions .q-del{display:none}
.hint.sm{font-size:12px;color:#888;margin:6px 0 0;line-height:1.4}
textarea.todo-src{min-height:220px;font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;font-size:13px;line-height:1.45}
#device-meters .meter-loading{margin:0;font-size:13px;color:#888}
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

// HTML 正文转义（非属性）
static void appendHtmlText(String &out, const String &s) {
  for (size_t i = 0; i < s.length(); i++) {
    const char c = s[i];
    if (c == '&') out += F("&amp;");
    else if (c == '<') out += F("&lt;");
    else if (c == '>') out += F("&gt;");
    else out += c;
  }
}

// JS 字符串字面量内容转义（不含外层引号）
static void appendJsStringContent(String &out, const String &s) {
  for (size_t i = 0; i < s.length(); i++) {
    const unsigned char c = (unsigned char)s[i];
    if (c == '\\' || c == '"' || c == '\'') {
      out += '\\';
      out += (char)c;
    } else if (c == '\n') {
      out += F("\\n");
    } else if (c == '\r') {
      out += F("\\r");
    } else if (c == '\t') {
      out += F("\\t");
    } else if (c < 0x20) {
      char buf[7];
      snprintf(buf, sizeof(buf), "\\u%04x", c);
      out += buf;
    } else {
      out += (char)c;
    }
  }
}

// 列表预览：取首行并截断
static String quotePreviewLine(const String &text, size_t max_chars = 36) {
  String line = text;
  const int nl = line.indexOf('\n');
  if (nl >= 0) line.remove(nl);
  line.trim();
  if (line.length() > max_chars) {
    line.remove(max_chars);
    line += F("…");
  }
  return line;
}

static void appendNavLink(String &page, NavTab tab, NavTab active, const char *href, const char *label) {
  page += F("<a href=");
  page += href;
  if (tab == active) page += F(" class=active");
  page += F(">");
  page += label;
  page += F("</a>");
}

static void appendNavSep(String &page) { page += F("<span class=nav-sep aria-hidden=true></span>"); }

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
  if (code == "power") msg = "电源设置已保存";
  else if (code == "calendar") msg = "日历设置已保存";
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
  else if (code == "chess" || code == "chess_content") msg = "象棋练习内容已保存";
  else if (code == "chess_refresh") msg = "象棋定时翻局已保存";
  else if (code == "quote" || code == "quote_play") msg = "名言播放顺序已保存";
  else if (code == "quote_refresh") msg = "名言定时翻条已保存";
  else if (code == "quote_list") msg = "名言列表已保存";
  else if (code == "quote_one") msg = "名言已保存";
  else if (code == "quote_add") msg = "已添加名言";
  else if (code == "quote_del") msg = "已删除名言";
  else if (code == "quote_reset") msg = "已恢复内置名言与诗词";
  else if (code == "todo_local") msg = "本机待办已保存";
  else if (code == "todo_mode") msg = "待办数据源已切换";
  else if (code == "quiet") msg = "静默时段已保存";
  else if (code == "appsched") msg = "App 时间表已保存";
  else if (code == "refresh_all") msg = "定时刷新设置已保存";
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
  else if (active == NavTab::Quote) g_server.sendContent(F(" class=page-quote"));

  {
    String nav;
    nav.reserve(1800);
    nav += F("><header>"
             "<div class=brand>"
             "<img class=site-logo src=/favicon.svg alt='' width=28 height=28>"
             "<h1>Carte</h1></div><nav>");
    // 系统：状态 / 设置 / WiFi / 鉴权
    appendNavLink(nav, NavTab::Status, active, "/", "状态");
    appendNavLink(nav, NavTab::Settings, active, "/settings", "设置");
    appendNavLink(nav, NavTab::Wifi, active, "/wifi", "WiFi");
    appendNavLink(nav, NavTab::Auth, active, "/auth", "鉴权");
    appendNavSep(nav);
    // App：待办 / 天气 / 相册 / 象棋
    appendNavLink(nav, NavTab::Todo, active, "/todo", "待办");
    appendNavLink(nav, NavTab::Weather, active, "/weather", "天气");
    appendNavLink(nav, NavTab::Album, active, "/album", "相册");
    appendNavLink(nav, NavTab::Chess, active, "/chess", "象棋");
    appendNavLink(nav, NavTab::Quote, active, "/quote", "名言");
    appendNavSep(nav);
    // 其它：日程 / 错误 / 帮助
    appendNavLink(nav, NavTab::Schedule, active, "/schedule", "日程");
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

// 设备指标 JSON（状态页异步拉取，避免首屏阻塞）
static void handleApiDeviceGet() {
  JsonDocument doc;
  {
    const uint64_t total = LittleFS.totalBytes();
    const uint64_t used = LittleFS.usedBytes();
    AlbumList al;
    albumList(al);
    JsonObject fs = doc["fs"].to<JsonObject>();
    fs["used"] = used;
    fs["total"] = total;
    fs["album"] = al.count;
  }
  {
    const uint32_t total = ESP.getHeapSize();
    const uint32_t free_h = ESP.getFreeHeap();
    JsonObject heap = doc["heap"].to<JsonObject>();
    heap["used"] = total > free_h ? (total - free_h) : 0;
    heap["total"] = total;
    heap["free"] = free_h;
  }
  {
    const uint32_t total = ESP.getPsramSize();
    if (total > 0) {
      const uint32_t free_p = ESP.getFreePsram();
      JsonObject ps = doc["psram"].to<JsonObject>();
      ps["used"] = total > free_p ? (total - free_p) : 0;
      ps["total"] = total;
      ps["free"] = free_p;
    }
  }
  {
    const uint32_t used = ESP.getSketchSize();
    const uint32_t free_s = ESP.getFreeSketchSpace();
    JsonObject flash = doc["flash"].to<JsonObject>();
    flash["used"] = used;
    flash["total"] = used + free_s;
  }
  {
    const BatteryStatus bat = batteryRead();
    JsonObject battery = doc["battery"].to<JsonObject>();
    battery["ok"] = bat.ok;
    if (bat.ok) {
      battery["percent"] = bat.percent;
      battery["mv"] = bat.mv;
      battery["charge"] = batteryChargeLabel(bat.charge);
    }
  }
  doc["chip_rev"] = (int)ESP.getChipRevision();
  doc["cpu_mhz"] = (unsigned)ESP.getCpuFreqMHz();

  String out;
  serializeJson(doc, out);
  g_server.send(200, "application/json", out);
}

static void webStatusSendScript(WebServer &server) {
  static const char kJs[] = R"STJS(<script>
(function(){
  const box=document.getElementById('device-meters');
  if(!box) return;
  function fmtBytes(n){
    n=+n||0;
    if(n>=1048576) return (n/1048576).toFixed(2)+' MB';
    if(n>=1024) return (n/1024).toFixed(1)+' KB';
    return n+' B';
  }
  function tone(pct){return pct>=90?'bad':pct>=75?'warn':'ok';}
  function meter(lab,used,total,extra,pctOverride){
    let pct=pctOverride!=null?pctOverride:(total>0?Math.min(100,Math.floor(used*100/total)):0);
    let val=fmtBytes(used)+' / '+fmtBytes(total)+' · '+pct+'%';
    if(extra) val+=' · '+extra;
    return '<div class=meter><div class=meter-h><span class=lab>'+lab+
      '</span><span class=val>'+val+'</span></div><div class=meter-track><div class="meter-fill '+
      tone(pct)+'" style="width:'+pct+'%"></div></div></div>';
  }
  function batMeter(pct,extra){
    const t=pct<=15?'bad':pct<=30?'warn':'ok';
    return '<div class=meter><div class=meter-h><span class=lab>电池</span><span class=val>'+
      pct+'% · '+extra+'</span></div><div class=meter-track><div class="meter-fill '+t+
      '" style="width:'+pct+'%"></div></div></div>';
  }
  fetch('/api/device').then(r=>r.json()).then(d=>{
    let h='<div class=meters>';
    if(d.fs) h+=meter('存储 LittleFS',d.fs.used,d.fs.total,'相册 '+(d.fs.album|0)+' 张');
    if(d.heap) h+=meter('内存 Heap',d.heap.used,d.heap.total,'空闲 '+Math.floor((d.heap.free||0)/1024)+' KB');
    if(d.psram) h+=meter('内存 PSRAM',d.psram.used,d.psram.total,'空闲 '+Math.floor((d.psram.free||0)/1024)+' KB');
    if(d.flash) h+=meter('固件 Flash',d.flash.used,d.flash.total,'含 OTA 余量');
    if(d.battery&&d.battery.ok) h+=batMeter(d.battery.percent|0,(d.battery.mv|0)+' mV · '+(d.battery.charge||''));
    else h+='<p class=meter-note>电池：暂不可读</p>';
    h+='<p class=meter-note>芯片 rev '+(d.chip_rev|0)+' · CPU '+(d.cpu_mhz|0)+' MHz</p></div>';
    box.innerHTML=h;
  }).catch(()=>{box.innerHTML='<p class=meter-note>设备数据加载失败</p>';});
})();
</script>)STJS";
  server.sendContent(kJs, strlen(kJs));
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
    loc += qweatherSourceIsApiKey() ? " · 和风 API" : " · via Portal";
    kvRow(b, "天气", loc);
  } else if (cfg.qweather_enabled) {
    kvRow(b, "天气",
          String(qweatherSourceIsApiKey() ? "<span class='pill warn'>需 Host/Key + 位置</span>"
                                          : "<span class='pill warn'>需登录 + 选择位置</span>"));
  } else {
    kvRow(b, "天气", String("<span class='pill off'>关闭</span>"));
  }
  {
    auto kvWxTimer = [&](const char *lab, bool en, uint32_t sec) {
      if (!en) {
        kvRow(b, lab, String("<span class='pill off'>关</span>"));
        return;
      }
      uint32_t d = 0, h = 0, m = 0;
      weatherRefreshToDhm(sec, d, h, m);
      char buf[80];
      snprintf(buf, sizeof(buf), "<span class='pill on'>开 · %u天%u时%u分</span>", (unsigned)d,
               (unsigned)h, (unsigned)m);
      kvRow(b, lab, String(buf));
    };
    kvWxTimer("日天气", cfg.weather_now_refresh_en, cfg.weather_now_refresh_sec);
    kvWxTimer("周天气", cfg.weather_week_refresh_en, cfg.weather_week_refresh_sec);
  }
  {
    AlbumList al;
    albumList(al);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d 张", al.count);
    kvRow(b, "相册", String(buf));
    kvRow(b, "相册标题栏", String(cfg.album_show_header ? "显示" : "隐藏"));
    kvRow(b, "相册顺序", String(cfg.album_random ? "乱序" : "顺序"));
    kvRow(b, "待办来源", String(cfg.todo_use_local ? "本机正文" : "网络日记"));
    if (!cfg.album_refresh_en) {
      kvRow(b, "相册定时", String("<span class='pill off'>关</span>"));
    } else {
      uint32_t d = 0, h = 0, m = 0;
      weatherRefreshToDhm(cfg.album_refresh_sec, d, h, m);
      char buf2[80];
      snprintf(buf2, sizeof(buf2), "<span class='pill on'>开 · %u天%u时%u分</span>", (unsigned)d,
               (unsigned)h, (unsigned)m);
      kvRow(b, "相册定时", String(buf2));
    }
  }
  {
    kvRow(b, "象棋顺序", String(cfg.chess_random ? "乱序" : "顺序"));
    String lv;
    if (cfg.chess_levels & CHESS_LV_OPENING) {
      lv += "开局";
    }
    if (cfg.chess_levels & CHESS_LV_TACTIC) {
      if (lv.length()) lv += " · ";
      lv += "战术";
    }
    if (cfg.chess_levels & CHESS_LV_MIDDLE) {
      if (lv.length()) lv += " · ";
      lv += "中局";
    }
    if (cfg.chess_levels & CHESS_LV_ENDGAME) {
      if (lv.length()) lv += " · ";
      lv += "残局";
    }
    if (!lv.length()) lv = "全部";
    kvRow(b, "象棋内容", lv);
    if (!cfg.chess_refresh_en) {
      kvRow(b, "象棋定时", String("<span class='pill off'>关</span>"));
    } else {
      uint32_t d = 0, h = 0, m = 0;
      weatherRefreshToDhm(cfg.chess_refresh_sec, d, h, m);
      char buf3[80];
      snprintf(buf3, sizeof(buf3), "<span class='pill on'>开 · %u天%u时%u分</span>", (unsigned)d,
               (unsigned)h, (unsigned)m);
      kvRow(b, "象棋定时", String(buf3));
    }
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
  // 设备指标异步加载：先出页面骨架，再请求 /api/device
  b += F("<section><h2>设备</h2><div id=device-meters>"
         "<p class=meter-loading>加载中…</p></div></section>");
  sendHtml(NavTab::Status, b, webStatusSendScript);
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
    case AppId::Status:
      return "<svg viewBox='0 0 24 24' fill='none' stroke='#1a1a1a' stroke-width='2'>"
             "<rect x='4' y='4' width='16' height='16' rx='2'/>"
             "<path d='M8 14v-4M12 16V8M16 13v-2'/></svg>";
    case AppId::Chess:
      return "<svg viewBox='0 0 24 24' fill='none' stroke='#1a1a1a' stroke-width='2'>"
             "<path d='M8 20h8M10 20v-3h4v3'/>"
             "<path d='M9 17c0-2 1-3 3-4 2 1 3 2 3 4'/>"
             "<circle cx='12' cy='9' r='2'/></svg>";
    case AppId::Quote:
      return "<svg viewBox='0 0 24 24' fill='none' stroke='#1a1a1a' stroke-width='2'>"
             "<path d='M7 8h5v5H8a1 1 0 0 1-1-1V8zM14 8h5v5h-4a1 1 0 0 1-1-1V8z'/>"
             "<path d='M8 13c0 3 2 5 4 6M16 13c0 3 2 5 4 6'/></svg>";
    default:
      return "";
  }
}

static void handleSettingsGet() {
  const AppConfig &cfg = getAppConfig();
  String b;
  appendFlashFromQuery(b);

  // ① 应用开关
  b += F("<section><h2><span class=set-step>1</span>应用</h2>"
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

  // ② 电源（与日历拆开）
  b += F("<section><h2><span class=set-step>2</span>电源</h2>"
         "<p class=hint>无操作自动关机是本固件自己的计时；按任意键会重新计时。"
         "开启 Web 配置期间不会自动关机。"
         "「连接电源时自动关机」关闭后，接 USB 时不会因空闲关机。</p>"
         "<form method=POST action=/power>"
         "<input type=hidden name=action value=power>"
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
  b += F("<div class=btn-row><button type=submit>保存电源设置</button></div></form></section>");

  // ③ 日历
  b += F("<section><h2><span class=set-step>3</span>日历</h2>"
         "<p class=hint>仅影响日历 App 一周的起始日。</p>"
         "<form method=POST action=/power>"
         "<input type=hidden name=action value=calendar>"
         "<label>一周起始</label><select name=week_start>"
         "<option value=mon");
  if (cfg.week_start_monday) b += F(" selected");
  b += F(">周一</option><option value=sun");
  if (!cfg.week_start_monday) b += F(" selected");
  b += F(">周日</option></select>"
         "<div class=btn-row><button type=submit>保存日历设置</button></div></form></section>");

  // ④ 时区 / NTP
  b += F("<section><h2><span class=set-step>4</span>时区 / NTP</h2>"
         "<p class=hint>板载 PCF8563 掉电后仍走时。复位后下次联网会校一次；平时按周期 NTP。</p>");
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
         "</div></form></section>");

  // ⑤ 手动校时（离线）
  b += F("<section><h2><span class=set-step>5</span>手动校时</h2>"
         "<p class=hint>无网时直接写入本地时间到 RTC。</p>"
         "<form method=POST action=/time>"
         "<label>本地时间</label>"
         "<input type=datetime-local name=manual_time step=1");
  RtcDateTime now;
  if (rtcRead(now)) {
    char v[32];
    snprintf(v, sizeof(v), " value=%04d-%02d-%02dT%02d:%02d:%02d", now.year, now.month, now.day,
             now.hour, now.minute, now.second);
    b += v;
  }
  b += F(">"
         "<div class=btn-row><button type=submit name=action value=set_manual>写入 RTC</button></div>"
         "</form></section>");

  sendHtml(NavTab::Settings, b);
}

static void handleChessGet() {
  const AppConfig &cfg = getAppConfig();
  String b;
  appendFlashFromQuery(b);

  uint32_t d = 0, h = 0, m = 0;
  weatherRefreshToDhm(cfg.chess_refresh_sec, d, h, m);
  const uint8_t lv = cfg.chess_levels ? cfg.chess_levels : CHESS_LV_ALL;

  // ① 练习内容
  b += F("<section><h2><span class=set-step>1</span>练习内容</h2>"
         "<p class=hint>先选播放顺序与局面等级。等级可多选；全不选则视为全部。</p>"
         "<form method=POST action=/chess>"
         "<input type=hidden name=action value=content>"
         "<label>播放顺序</label><select name=order>"
         "<option value=seq");
  if (!cfg.chess_random) b += F(" selected");
  b += F(">顺序</option><option value=rand");
  if (cfg.chess_random) b += F(" selected");
  b += F(">乱序</option></select>"
         "<label>内容等级</label>"
         "<div class=btn-row style=flex-wrap:wrap;gap:8px>");
  const struct {
    uint8_t bit;
    const char *name;
    const char *lab;
  } lvItems[] = {
      {CHESS_LV_OPENING, "lv_opening", "开局"},
      {CHESS_LV_TACTIC, "lv_tactic", "战术"},
      {CHESS_LV_MIDDLE, "lv_middle", "中局"},
      {CHESS_LV_ENDGAME, "lv_endgame", "残局"},
  };
  for (const auto &it : lvItems) {
    const int cnt = chessCountForLevel(it.bit);
    b += F("<label class='check-row");
    if (lv & it.bit) b += F(" on");
    b += F("' style=margin:0><input type=checkbox name=");
    b += it.name;
    b += F(" value=1");
    if (lv & it.bit) b += F(" checked");
    b += F("><span>");
    b += it.lab;
    b += F(" <small style=font-weight:500;opacity:.75>(");
    b += String(cnt);
    b += F(")</small></span></label>");
  }
  b += F("<p class=meta style=margin:8px 0 0>当前可选局面合计 ");
  b += String(chessPositionCount());
  b += F(" / 内置 ");
  b += String(chessCountForLevel(CHESS_LV_ALL));
  b += F("</p></div>"
         "<div class=btn-row><button type=submit>保存练习内容</button></div></form></section>");

  // ② 定时翻局
  b += F("<section><h2><span class=set-step>2</span>定时翻局</h2>"
         "<p class=hint>停留在象棋 App 时按间隔自动翻局；关机亦可 RTC 定时醒。"
         "间隔至少 <strong>1 分钟</strong>。</p>"
         "<form method=POST action=/chess>"
         "<input type=hidden name=action value=refresh>"
         "<label class='check-row");
  if (cfg.chess_refresh_en) b += F(" on");
  b += F("'><input type=checkbox name=refresh_en value=1");
  if (cfg.chess_refresh_en) b += F(" checked");
  b += F("><span>启用定时翻局</span></label>"
         "<div class=btn-row style=align-items:flex-end;gap:10px>"
         "<label style=margin:0>天<br><input name=refresh_d type=number min=0 max=365 step=1 value=");
  b += String(d);
  b += F(" style=width:5.5em></label>"
         "<label style=margin:0>时<br><input name=refresh_h type=number min=0 max=23 step=1 value=");
  b += String(h);
  b += F(" style=width:5.5em></label>"
         "<label style=margin:0>分<br><input name=refresh_m type=number min=0 max=59 step=1 value=");
  b += String(m);
  b += F(" style=width:5.5em></label></div>"
         "<div class=btn-row><button type=submit>保存定时设置</button></div></form></section>");

  sendHtml(NavTab::Chess, b);
}

static void handleChessPost() {
  const AppConfig &cfg = getAppConfig();
  const String action = g_server.hasArg("action") ? g_server.arg("action") : String("content");

  if (action == "refresh") {
    const bool refresh_en = g_server.hasArg("refresh_en");
    const uint32_t d = (uint32_t)g_server.arg("refresh_d").toInt();
    const uint32_t h = (uint32_t)g_server.arg("refresh_h").toInt();
    const uint32_t m = (uint32_t)g_server.arg("refresh_m").toInt();
    saveChessSettings(cfg.chess_random, cfg.chess_levels ? cfg.chess_levels : CHESS_LV_ALL,
                      refresh_en, weatherRefreshSecFromDhm(d, h, m));
    redirectOk("/chess", "chess_refresh");
    return;
  }

  // content
  const bool random_order = g_server.arg("order") == "rand";
  uint8_t levels = 0;
  if (g_server.hasArg("lv_opening")) levels |= CHESS_LV_OPENING;
  if (g_server.hasArg("lv_tactic")) levels |= CHESS_LV_TACTIC;
  if (g_server.hasArg("lv_middle")) levels |= CHESS_LV_MIDDLE;
  if (g_server.hasArg("lv_endgame")) levels |= CHESS_LV_ENDGAME;
  if (levels == 0) levels = CHESS_LV_ALL;
  saveChessSettings(random_order, levels, cfg.chess_refresh_en, cfg.chess_refresh_sec);
  redirectOk("/chess", "chess_content");
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
  const String action = g_server.hasArg("action") ? g_server.arg("action") : String("power");
  // 日历一周起始单独表单，避免与电源字段互相覆盖
  if (action == "calendar") {
    saveWeekStartMonday(g_server.arg("week_start") != "sun");
    redirectOk("/settings", "calendar");
    return;
  }
  const uint32_t sec = (uint32_t)g_server.arg("idle_off_sec").toInt();
  saveIdleOffSec(sec);
  powerIdleSetTimeoutSec(sec);
  saveIdleOffOnPower(g_server.arg("idle_off_on_power") == "1");
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
  b += F("<section><h2>WiFi</h2>");
  b += F("<p class=hint>最多 5 条。大字为 SSID，下方小字为密码。保存后按 Active → 其它条目依次尝试连接。</p>");
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
  qw["source"] = cfg.qweather_source;
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
  const bool has_token = cfg.portal_token[0] != '\0';

  // ① 服务与账号
  b += F("<section><h2><span class=set-step>1</span>服务与账号</h2>");
  b += F("<p class=hint>先填 Portal 地址与邮箱；Diary ID 给待办网络日记用。</p>");
  b += F("<form method=POST action=/auth>");
  b += F("<input type=hidden name=action value=save>");
  b += F("<label>API Base URL</label><input name=base_url value=\"");
  b += htmlEscapeAttr(cfg.portal_base_url);
  b += F("\">");
  b += F("<label>邮箱</label><input name=email type=email value=\"");
  b += htmlEscapeAttr(cfg.portal_email);
  b += F("\">");
  b += F("<label>Diary ID</label><input name=diary_id value=\"");
  b += htmlEscapeAttr(cfg.diary_id);
  b += F("\">");
  b += F("<div class=btn-row><button type=submit>保存服务配置</button></div></form></section>");

  // ② 登录
  b += F("<section><h2><span class=set-step>2</span>登录</h2>");
  b += F("<p class=hint>登录 <code>POST /user/login</code> 保存 JWT；响应头 "
         "<code>X-Access-Token</code>（剩余 &lt; 7 天）会自动续签。密码仅本次提交，不落盘。</p>");
  b += F("<div class='set-status");
  if (!has_token) b += F(" empty");
  b += F("'><span class=lbl>Token</span>");
  if (has_token) {
    b += F("<span class=name>已登录</span><code class=id>");
    b += htmlEscapeAttr(cfg.portal_email);
    b += F("</code>");
  } else {
    b += F("<span class=name>未登录 — 填写密码后登录</span>");
  }
  b += F("</div>");
  b += F("<form method=POST action=/auth>");
  b += F("<label>密码</label><input name=password type=password autocomplete=current-password>");
  // 登录时带上当前 URL/邮箱，避免只填密码时丢配置
  b += F("<input type=hidden name=base_url value=\"");
  b += htmlEscapeAttr(cfg.portal_base_url);
  b += F("\"><input type=hidden name=email value=\"");
  b += htmlEscapeAttr(cfg.portal_email);
  b += F("\"><input type=hidden name=diary_id value=\"");
  b += htmlEscapeAttr(cfg.diary_id);
  b += F("\">");
  b += F("<div class=btn-row>");
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

static void appendTodoPreviewList(String &b, const String &content) {
  TodoItem items[TODO_ITEM_MAX];
  const int n = todoParseContent(content, items, TODO_ITEM_MAX);
  int open_n = 0;
  for (int i = 0; i < n; i++) {
    if (!items[i].done) open_n++;
  }
  b += F("<p class=meta>");
  b += String(open_n);
  b += F("/");
  b += String(n);
  b += F(" open</p>");
  if (n == 0) {
    b += F("<pre>");
    String c = content;
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
}

static void appendTodoPage(String &b, const char *flash_html = nullptr) {
  const AppConfig &cfg = getAppConfig();
  if (flash_html && flash_html[0]) b += flash_html;
  appendFlashFromQuery(b);

  // ① 数据源
  b += F("<section><h2><span class=set-step>1</span>数据源</h2>"
         "<p class=hint>先选从哪读待办：本机正文，或 Portal 网络日记。</p>"
         "<form method=POST action=/todo>"
         "<input type=hidden name=action value=mode>"
         "<label class='check-row");
  if (cfg.todo_use_local) b += F(" on");
  b += F("'><input type=checkbox name=use_local value=1");
  if (cfg.todo_use_local) b += F(" checked");
  b += F("><span>使用本机正文（不拉网络）</span></label>"
         "<div class=btn-row><button type=submit>保存数据源</button></div></form></section>");

  // ② 本机正文
  String local;
  todoLocalLoad(local);
  b += F("<section><h2><span class=set-step>2</span>本机正文</h2>"
         "<p class=hint>规则：每行一项；<code>- </code>未完成、<code>= </code>已完成；"
         "可选 <code> #备注</code>（黄字）。例：<code>- 买菜 #晚上</code></p>");
  if (!cfg.todo_use_local) {
    b += F("<p class='hint sm'>当前数据源为网络日记；本机正文仍可编辑备用，启用上方开关后才会显示在设备上。</p>");
  }
  b += F("<form method=POST action=/todo>"
         "<input type=hidden name=action value=save_local>"
         "<label>正文</label><textarea class=todo-src name=content>");
  String esc = local;
  esc.replace("&", "&amp;");
  esc.replace("<", "&lt;");
  esc.replace(">", "&gt;");
  b += esc;
  b += F("</textarea>"
         "<div class=btn-row><button type=submit>保存本机待办</button></div></form>");
  if (local.length()) {
    b += F("<p class=meta style=margin-top:14px>本机预览</p>");
    appendTodoPreviewList(b, local);
  }
  b += F("</section>");

  // ③ 网络日记
  b += F("<section><h2><span class=set-step>3</span>网络日记</h2>"
         "<p class=hint>从 Portal <code>GET /diary/detail</code> 拉取；需「同 WiFi 站点」已连外网并在「鉴权」页登录。"
         "拉取结果仅预览，不会覆盖本机正文。</p>");
  if (cfg.todo_use_local) {
    b += F("<p class='hint sm'>当前使用本机正文；关闭步骤 1 的开关后，设备才会显示网络日记。</p>");
  }
  b += F("<form method=POST action=/todo><input type=hidden name=action value=fetch>"
         "<div class=btn-row><button type=submit>立即拉取预览</button></div></form></section>");
}

static void handleTodoGet() {
  String b;
  appendTodoPage(b);
  sendHtml(NavTab::Todo, b);
}

static void handleTodoPost() {
  const String action = g_server.hasArg("action") ? g_server.arg("action") : String("fetch");
  String b;

  if (action == "mode") {
    saveTodoUseLocal(g_server.hasArg("use_local"));
    redirectOk("/todo", "todo_mode");
    return;
  }
  if (action == "save_local") {
    const String content = g_server.arg("content");
    if (!todoLocalSave(content)) {
      appendTodoPage(b, "<div class='flash err'>本机待办保存失败</div>");
      sendHtml(NavTab::Todo, b);
      return;
    }
    redirectOk("/todo", "todo_local");
    return;
  }

  // fetch
  if (!wifiIsStaConnected()) {
    appendTodoPage(b, "<div class='flash err'>需要「同 WiFi 站点」已连接外网才能拉日记</div>");
    sendHtml(NavTab::Todo, b);
    return;
  }
  DiaryDetail d;
  if (!diaryFetchById(nullptr, d)) {
    String flash = "<div class='flash err'>";
    flash += d.message;
    flash += "</div>";
    appendTodoPage(b, flash.c_str());
    sendHtml(NavTab::Todo, b);
    return;
  }
  appendTodoPage(b, "<div class='flash ok'>待办已拉取（仅预览）</div>");
  b += F("<section><h2>");
  b += d.title.length() ? d.title : String("Todo");
  b += F("</h2><p class=meta>#");
  b += d.id;
  b += F(" · ");
  b += d.date;
  b += F(" · ");
  b += d.category;
  b += F("</p>");
  appendTodoPreviewList(b, d.content);
  b += F("</section>");
  sendHtml(NavTab::Todo, b);
}

// 天气页：① 数据源 → ② 位置（当前+搜索+选用）→ ③ 定时；geo_query 用于搜索后回填
static void appendWeatherForm(String &b, const AppConfig &cfg, const WeatherGeoResult *geo,
                              const char *flash_html, const char *geo_query = nullptr) {
  if (flash_html && flash_html[0]) b += flash_html;
  const bool src_api = (cfg.qweather_source == QW_SRC_APIKEY);
  const bool has_loc = cfg.qweather_location_id[0] != '\0';

  // —— ① 数据源 / 开关 ——
  b += F("<section><h2><span class=set-step>1</span>数据源</h2>");
  b += F("<p class=hint>先选请求方式并启用天气。<strong>Portal</strong> 用登录 JWT 代理和风；"
         "<strong>和风 API Key</strong> 用普通 <code>key=</code> 直连（非 JWT）。</p>");
  b += F("<form method=POST action=/weather id=wx-source-form>");
  b += F("<label class='check-row");
  if (cfg.qweather_enabled) b += F(" on");
  b += F("'><input type=checkbox name=enabled value=1");
  if (cfg.qweather_enabled) b += F(" checked");
  b += F("><span>启用天气</span></label>");

  b += F("<label class='check-row");
  if (!src_api) b += F(" on");
  b += F("'><input type=radio name=source value=0");
  if (!src_api) b += F(" checked");
  b += F("><span>Portal 代理（JWT）</span></label>");
  b += F("<label class='check-row");
  if (src_api) b += F(" on");
  b += F("'><input type=radio name=source value=1");
  if (src_api) b += F(" checked");
  b += F("><span>和风普通 API Key</span></label>");

  b += F("<div class=qw-api-fields style=\"");
  b += src_api ? F("display:block") : F("display:none");
  b += F("\">");
  b += F("<label>API Host</label><input name=host placeholder=\"控制台 API Host 或 devapi.qweather.com\" value=\"");
  b += htmlEscapeAttr(cfg.qweather_host);
  b += F("\">");
  b += F("<label>API Key");
  if (cfg.qweather_key[0]) b += F(" <span class=hint>（已保存，留空不改）</span>");
  b += F("</label><input name=key type=password autocomplete=off placeholder=\"和风 API KEY\" value=\"\">");
  b += F("</div>");

  b += F("<div class=btn-row><button type=submit name=action value=save>保存数据源</button></div>");
  b += F("</form>");
  // 切换 radio 时显示/隐藏 Host+Key
  b += F("<script>(function(){var f=document.getElementById('wx-source-form');"
         "if(!f)return;var box=f.querySelector('.qw-api-fields');"
         "function sync(){var a=f.querySelector('input[name=source][value=\"1\"]');"
         "if(box)box.style.display=(a&&a.checked)?'block':'none';}"
         "f.querySelectorAll('input[name=source]').forEach(function(r){r.addEventListener('change',sync);});"
         "sync();})();</script>");
  b += F("</section>");

  // —— ② 位置：当前 + 搜索 + 选用 放一起 ——
  b += F("<section><h2><span class=set-step>2</span>位置</h2>");
  if (src_api) {
    b += F("<p class=hint>搜索城市后点选用即可写入 LocationID。"
           "需已填 Host + Key，且设备「同 WiFi 站点」已连外网。</p>");
  } else {
    b += F("<p class=hint>搜索城市后点选用即可写入 LocationID。"
           "需「同 WiFi 站点」已连接并登录 Portal。</p>");
  }

  b += F("<div class='set-status");
  if (!has_loc) b += F(" empty");
  b += F("'><span class=lbl>当前</span>");
  if (has_loc) {
    b += F("<span class=name>");
    b += htmlEscapeAttr(cfg.qweather_location_name[0] ? cfg.qweather_location_name
                                                       : cfg.qweather_location_id);
    b += F("</span><code class=id>");
    b += htmlEscapeAttr(cfg.qweather_location_id);
    b += F("</code>");
  } else {
    b += F("<span class=name>尚未设置 — 请下方搜索并选用</span>");
  }
  b += F("</div>");

  b += F("<form method=POST action=/weather>");
  b += F("<label>搜索城市 / 区县</label>");
  b += F("<div class=wx-search-row>");
  b += F("<input name=geo_query placeholder=\"例如 杭州 或 西湖\" value=\"");
  if (geo_query && geo_query[0]) b += htmlEscapeAttr(geo_query);
  b += F("\">");
  b += F("<button type=submit name=action value=search>搜索</button>");
  b += F("</div></form>");

  if (geo && geo->ok && geo->count > 0) {
    // 有当前城市则默认勾中它，否则勾第一项
    int checked_i = 0;
    if (has_loc) {
      for (int i = 0; i < geo->count; i++) {
        if (geo->items[i].id.equals(cfg.qweather_location_id)) {
          checked_i = i;
          break;
        }
      }
    }
    b += F("<form method=POST action=/weather><p class=meta style=margin-top:14px>找到 ");
    b += String(geo->count);
    b += F(" 个结果，选一个设为当前位置：</p><ul class=geo-list>");
    for (int i = 0; i < geo->count; i++) {
      const WeatherGeoItem &it = geo->items[i];
      const bool is_cur = has_loc && it.id.equals(cfg.qweather_location_id);
      b += F("<li><label><input type=radio name=pick value=\"");
      b += htmlEscapeAttr(it.id.c_str());
      if (i == checked_i) b += F("\" checked");
      else b += F("\"");
      b += F("><span><strong>");
      String name = it.name;
      name.replace("&", "&amp;");
      name.replace("<", "&lt;");
      b += name;
      b += F("</strong> <code>");
      b += it.id;
      b += F("</code>");
      if (is_cur) b += F(" <span class='pill on'>当前</span>");
      b += F("<span class=sub>");
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
      b += F("<input type=hidden name=name_");
      b += it.id;
      b += F(" value=\"");
      b += htmlEscapeAttr(it.name.c_str());
      b += F("\">");
      b += F("</li>");
    }
    b += F("</ul><div class=btn-row>"
           "<button type=submit name=action value=pick>设为当前位置</button></div></form>");
  } else if (geo && !geo->ok && geo->message.length()) {
    b += F("<div class='flash err' style=margin-top:12px>");
    b += geo->message;
    b += F("</div>");
  }
  b += F("</section>");

  // —— ③ 定时刷新：日天气 / 周天气各自间隔 ——
  {
    auto appendRefreshBlock = [&](const char *step, const char *title, const char *hint,
                                  const char *en_name, bool en, const char *prefix, uint32_t sec) {
      uint32_t d = 0, h = 0, m = 0;
      weatherRefreshToDhm(sec, d, h, m);
      b += F("<section><h2><span class=set-step>");
      b += step;
      b += F("</span>");
      b += title;
      b += F("</h2><p class=hint>");
      b += hint;
      b += F("</p><form method=POST action=/weather>"
             "<input type=hidden name=action value=refresh>"
             "<label class='check-row");
      if (en) b += F(" on");
      b += F("'><input type=checkbox name=");
      b += en_name;
      b += F(" value=1");
      if (en) b += F(" checked");
      b += F("><span>启用定时刷新</span></label>"
             "<div class=btn-row style=align-items:flex-end;gap:10px>"
             "<label style=margin:0>天<br><input name=");
      b += prefix;
      b += F("_d type=number min=0 max=365 step=1 value=");
      b += String(d);
      b += F(" style=width:5.5em></label>"
             "<label style=margin:0>时<br><input name=");
      b += prefix;
      b += F("_h type=number min=0 max=23 step=1 value=");
      b += String(h);
      b += F(" style=width:5.5em></label>"
             "<label style=margin:0>分<br><input name=");
      b += prefix;
      b += F("_m type=number min=0 max=59 step=1 value=");
      b += String(m);
      b += F(" style=width:5.5em></label></div>"
             "<div class=btn-row><button type=submit>保存");
      b += title;
      b += F("</button></div></form></section>");
    };
    appendRefreshBlock(
        "3", "日天气定时",
        "停留在 <strong>NOW</strong> 时按此间隔拉实况/小时；关机且当前是日天气则 RTC 到点醒刷。"
        "间隔至少 <strong>1 分钟</strong>。",
        "now_en", cfg.weather_now_refresh_en, "now", cfg.weather_now_refresh_sec);
    appendRefreshBlock(
        "4", "周天气定时",
        "停留在 <strong>7D</strong> 时按此间隔拉七日预报；关机且当前是周天气则 RTC 到点醒刷。"
        "建议比日天气更长（默认 6 小时）。间隔至少 <strong>1 分钟</strong>。",
        "week_en", cfg.weather_week_refresh_en, "week", cfg.weather_week_refresh_sec);
  }
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
  String last_query;

  if (action == "save") {
    const bool enabled = g_server.hasArg("enabled");
    const uint8_t source =
        (g_server.arg("source").toInt() == (int)QW_SRC_APIKEY) ? QW_SRC_APIKEY : QW_SRC_PORTAL;
    const AppConfig &cfg = getAppConfig();
    const String host = g_server.hasArg("host") ? g_server.arg("host") : String(cfg.qweather_host);
    // key 留空则保留已有（saveQWeatherConfig 内处理）
    const String key = g_server.hasArg("key") ? g_server.arg("key") : String("");
    saveQWeatherConfig(enabled, source, host.c_str(), key.c_str(), cfg.qweather_location_id,
                       cfg.qweather_location_name);
    flash = F("<div class='flash ok'>数据源已保存</div>");
  } else if (action == "refresh") {
    // 表单带 now_* 或 week_*；一次只改对应档
    const bool has_now = g_server.hasArg("now_d") || g_server.hasArg("now_h") ||
                         g_server.hasArg("now_m") || g_server.hasArg("now_en");
    const bool has_week = g_server.hasArg("week_d") || g_server.hasArg("week_h") ||
                          g_server.hasArg("week_m") || g_server.hasArg("week_en");
    auto flashTimer = [&](const char *lab, bool en, uint32_t sec) {
      uint32_t od = 0, oh = 0, om = 0;
      weatherRefreshToDhm(sec, od, oh, om);
      flash = F("<div class='flash ok'>");
      flash += lab;
      flash += F("：");
      flash += en ? F("开") : F("关");
      flash += F(" · ");
      flash += String(od);
      flash += F(" 天 ");
      flash += String(oh);
      flash += F(" 时 ");
      flash += String(om);
      flash += F(" 分</div>");
    };
    if (has_week && !has_now) {
      const bool en = g_server.hasArg("week_en");
      const uint32_t sec = weatherRefreshSecFromDhm((uint32_t)g_server.arg("week_d").toInt(),
                                                   (uint32_t)g_server.arg("week_h").toInt(),
                                                   (uint32_t)g_server.arg("week_m").toInt());
      saveWeatherWeekRefresh(en, sec);
      flashTimer("周天气定时", en, getAppConfig().weather_week_refresh_sec);
    } else {
      const bool en = g_server.hasArg("now_en");
      const uint32_t sec = weatherRefreshSecFromDhm((uint32_t)g_server.arg("now_d").toInt(),
                                                   (uint32_t)g_server.arg("now_h").toInt(),
                                                   (uint32_t)g_server.arg("now_m").toInt());
      saveWeatherNowRefresh(en, sec);
      flashTimer("日天气定时", en, getAppConfig().weather_now_refresh_sec);
    }
  } else if (action == "search") {
    last_query = g_server.arg("geo_query");
    if (!weatherGeoLookup(last_query.c_str(), geo)) {
      flash = F("<div class='flash err'>搜索失败");
      if (geo.message.length()) {
        flash += F("：");
        flash += geo.message;
      }
      flash += F("</div>");
    } else {
      flash = F("<div class='flash ok'>找到 ");
      flash += String(geo.count);
      flash += F(" 个位置，请选用</div>");
    }
  } else if (action == "pick") {
    const String id = g_server.arg("pick");
    const String name_key = String("name_") + id;
    const String name = g_server.hasArg(name_key) ? g_server.arg(name_key) : id;
    const AppConfig &cfg = getAppConfig();
    // 选用位置时一并打开天气开关
    saveQWeatherConfig(true, cfg.qweather_source, cfg.qweather_host, cfg.qweather_key, id.c_str(),
                       name.c_str());
    flash = F("<div class='flash ok'>已设为当前位置：");
    flash += name;
    flash += F("（");
    flash += id;
    flash += F("）</div>");
  }

  String b;
  appendWeatherForm(b, getAppConfig(), (action == "search") ? &geo : nullptr, flash.c_str(),
                    last_query.length() ? last_query.c_str() : nullptr);
  sendHtml(NavTab::Weather, b);
}

static void handleAlbumGet() {
  albumEnsureDir();
  String b;
  b.reserve(10000);
  webAlbumAppendPage(b);
  sendHtml(NavTab::Album, b, webAlbumSendScript);
}

static void webQuoteSendScript(WebServer &server) {
  // 左列表点选 → 右栏编辑；新增 / 删除由表单 POST
  static const char kJs[] = R"QJS(<script>
(function(){
  const root=document.getElementById('quote-editor');
  const list=document.getElementById('quote-list');
  const form=document.getElementById('quote-form');
  if(!root||!list||!form||typeof QUOTES==='undefined') return;
  const title=document.getElementById('q-ed-title');
  const hint=document.getElementById('q-ed-hint');
  const action=document.getElementById('q-action');
  const idxEl=document.getElementById('q-idx');
  const textEl=document.getElementById('q-text');
  const authorEl=document.getElementById('q-author');
  const saveBtn=document.getElementById('q-save');
  const delBtn=document.getElementById('q-del');
  let cur=-1;
  function setOn(i){
    list.querySelectorAll('button[data-i]').forEach(function(btn){
      btn.classList.toggle('on', +btn.getAttribute('data-i')===i);
    });
  }
  function select(i){
    if(i<0||i>=QUOTES.length){ startAdd(); return; }
    cur=i;
    root.classList.remove('is-empty');
    setOn(i);
    action.value='save_one';
    idxEl.value=String(i);
    textEl.value=QUOTES[i].t||'';
    authorEl.value=QUOTES[i].a||'';
    title.textContent='编辑 #'+(i+1);
    hint.textContent='修改后点「保存」；诗词请在正文里换行分句。';
    saveBtn.textContent='保存';
    delBtn.style.display='';
    delBtn.disabled=false;
  }
  function startAdd(){
    cur=-1;
    root.classList.remove('is-empty');
    setOn(-1);
    action.value='add';
    idxEl.value='-1';
    textEl.value='';
    authorEl.value='';
    title.textContent='新增名言';
    hint.textContent='填写正文后点「添加」。最多 '+MAX_QUOTES+' 条。';
    saveBtn.textContent='添加';
    delBtn.style.display='none';
    textEl.focus();
  }
  list.addEventListener('click',function(e){
    const btn=e.target.closest('button[data-i]');
    if(!btn) return;
    select(+btn.getAttribute('data-i'));
  });
  const addBtn=document.getElementById('q-add');
  if(addBtn) addBtn.addEventListener('click',function(e){ e.preventDefault(); startAdd(); });
  if(delBtn) delBtn.addEventListener('click',function(){
    if(cur<0) return;
    if(!confirm('删除第 '+(cur+1)+' 条？')){ return; }
    action.value='delete';
    idxEl.value=String(cur);
  });
  form.addEventListener('submit',function(e){
    if(action.value==='delete') return;
    const t=(textEl.value||'').trim();
    if(!t){ e.preventDefault(); alert('正文不能为空'); }
  });
  const init=typeof INIT_QUOTE_IDX==='number'?INIT_QUOTE_IDX:-1;
  if(QUOTES.length===0) startAdd();
  else if(init>=0&&init<QUOTES.length) select(init);
  else select(0);
})();
</script>)QJS";
  server.sendContent(kJs, strlen(kJs));
}

static void handleQuoteGet() {
  const AppConfig &cfg = getAppConfig();
  String b;
  appendFlashFromQuery(b);

  quoteEnsureDefaults();
  // 列表缓冲走堆，避免与 WebServer/String 叠爆 loopTask 栈
  QuoteItem *items = new (std::nothrow) QuoteItem[QUOTE_ITEM_MAX];
  if (!items) {
    String err;
    err += F("<div class='flash err'>名言列表内存不足</div>");
    sendHtml(NavTab::Quote, err);
    return;
  }
  int n = 0, idx = 0;
  quoteLoad(items, QUOTE_ITEM_MAX, n, idx);

  // 保存后回跳选中该条：?i=N
  int sel = g_server.hasArg("i") ? g_server.arg("i").toInt() : 0;
  if (n <= 0) sel = -1;
  else if (sel < 0) sel = 0;
  else if (sel >= n) sel = n - 1;

  uint32_t d = 0, h = 0, m = 0;
  weatherRefreshToDhm(cfg.quote_refresh_sec, d, h, m);

  // ① 播放顺序
  b += F("<section><h2><span class=set-step>1</span>播放顺序</h2>"
         "<p class=hint>设备停留在「名言」App 时按此顺序翻条。</p>"
         "<form method=POST action=/quote>"
         "<input type=hidden name=action value=play>"
         "<label>顺序</label><select name=order>"
         "<option value=seq");
  if (!cfg.quote_random) b += F(" selected");
  b += F(">顺序</option><option value=rand");
  if (cfg.quote_random) b += F(" selected");
  b += F(">乱序</option></select>"
         "<div class=btn-row><button type=submit>保存播放顺序</button></div></form></section>");

  // ② 定时翻条
  b += F("<section><h2><span class=set-step>2</span>定时翻条</h2>"
         "<p class=hint>开启后按间隔自动翻条；关机亦可 RTC 定时醒。间隔至少 <strong>1 分钟</strong>。</p>"
         "<form method=POST action=/quote>"
         "<input type=hidden name=action value=refresh>"
         "<label class='check-row");
  if (cfg.quote_refresh_en) b += F(" on");
  b += F("'><input type=checkbox name=refresh_en value=1");
  if (cfg.quote_refresh_en) b += F(" checked");
  b += F("><span>启用定时翻条</span></label>"
         "<div class=btn-row style=align-items:flex-end;gap:10px>"
         "<label style=margin:0>天<br><input name=refresh_d type=number min=0 max=365 step=1 value=");
  b += String(d);
  b += F(" style=width:5.5em></label>"
         "<label style=margin:0>时<br><input name=refresh_h type=number min=0 max=23 step=1 value=");
  b += String(h);
  b += F(" style=width:5.5em></label>"
         "<label style=margin:0>分<br><input name=refresh_m type=number min=0 max=59 step=1 value=");
  b += String(m);
  b += F(" style=width:5.5em></label></div>"
         "<div class=btn-row><button type=submit>保存定时设置</button></div></form></section>");

  // ③ 名言列表
  b += F("<section><h2><span class=set-step>3</span>名言列表</h2>"
         "<p class='hint sm'>左侧点选一条，右侧修改或删除；「新增」追加空条。诗词在正文换行分句。最多 ");
  b += String(QUOTE_ITEM_MAX);
  b += F(" 条。</p>"
         "<div class=quote-toolbar><p class=meta>当前 ");
  b += String(n);
  b += F(" 条");
  if (n > 0) {
    b += F(" · 展示第 ");
    b += String(idx + 1);
    b += F(" 条");
  }
  b += F("</p><div class=btn-row>"
         "<button type=button id=q-add class=secondary>新增</button>"
         "<form method=POST action=/quote onsubmit=\"return confirm('恢复内置列表？将覆盖当前全部条目。');\">"
         "<input type=hidden name=action value=reset>"
         "<button type=submit class=danger>恢复内置列表</button></form></div></div>"
         "<div class=quote-layout>"
         "<aside class=quote-side><div class=quote-side-h>条目</div><div id=quote-list class=quote-list>");
  for (int i = 0; i < n; i++) {
    b += F("<button type=button data-i=");
    b += String(i);
    if (i == sel) b += F(" class=on");
    b += F("><span class=q-i>#");
    b += String(i + 1);
    b += F("</span><span class=q-t>");
    appendHtmlText(b, quotePreviewLine(items[i].text));
    b += F("</span><span class=q-a>");
    appendHtmlText(b, items[i].author.length() ? items[i].author : String(F("（无作者）")));
    b += F("</span></button>");
  }
  if (n == 0) {
    b += F("<p class='hint sm' style=padding:16px>暂无条目，点右上「新增」。</p>");
  }
  b += F("</div></aside>"
         "<div id=quote-editor class=quote-editor");
  if (n == 0) b += F(" is-empty");
  b += F(">"
         "<div class=q-ed-empty>从左侧选择一条，或点「新增」。</div>"
         "<form id=quote-form class=q-ed-form method=POST action=/quote>"
         "<input type=hidden name=action id=q-action value=save_one>"
         "<input type=hidden name=idx id=q-idx value=");
  b += String(sel >= 0 ? sel : -1);
  b += F(">"
         "<h3 id=q-ed-title class=q-ed-title>");
  if (sel >= 0) {
    b += F("编辑 #");
    b += String(sel + 1);
  } else {
    b += F("新增名言");
  }
  b += F("</h3>"
         "<p id=q-ed-hint class=q-ed-hint>修改后点「保存」；诗词请在正文里换行分句。</p>"
         "<label>正文</label>"
         "<textarea id=q-text name=text maxlength=");
  b += String(QUOTE_TEXT_MAX);
  b += F(" placeholder=\"名言，或诗词（每句一行）\" required>");
  if (sel >= 0) appendHtmlText(b, items[sel].text);
  b += F("</textarea>"
         "<label>作者 / 出处</label>"
         "<input id=q-author name=author maxlength=");
  b += String(QUOTE_AUTHOR_MAX);
  b += F(" value=\"");
  if (sel >= 0) b += htmlEscapeAttr(items[sel].author.c_str());
  b += F("\" placeholder=\"如：李白《静夜思》\" autocomplete=off>"
         "<div class='btn-row q-ed-actions'>"
         "<button type=submit id=q-save>");
  b += (sel >= 0) ? F("保存") : F("添加");
  b += F("</button>"
         "<button type=submit id=q-del class='danger q-del' formnovalidate");
  if (sel < 0) b += F(" style=display:none");
  b += F(">删除</button>"
         "</div></form></div></div></section>"
         "<script>const MAX_QUOTES=");
  b += String(QUOTE_ITEM_MAX);
  b += F(";const INIT_QUOTE_IDX=");
  b += String(sel);
  b += F(";const QUOTES=[");
  for (int i = 0; i < n; i++) {
    if (i) b += ',';
    b += F("{t:\"");
    appendJsStringContent(b, items[i].text);
    b += F("\",a:\"");
    appendJsStringContent(b, items[i].author);
    b += F("\"}");
  }
  b += F("];</script>");
  delete[] items;
  sendHtml(NavTab::Quote, b, webQuoteSendScript);
}

// 加载名言列表到堆缓冲；失败返回 nullptr（调用方 delete[]）
static QuoteItem *quoteLoadHeap(int &out_n, int &out_idx) {
  QuoteItem *items = new (std::nothrow) QuoteItem[QUOTE_ITEM_MAX];
  if (!items) return nullptr;
  out_n = 0;
  out_idx = 0;
  if (!quoteLoad(items, QUOTE_ITEM_MAX, out_n, out_idx)) {
    out_n = 0;
    out_idx = 0;
  }
  return items;
}

static void quoteRedirectSel(const char *code, int sel) {
  String loc = F("/quote?ok=");
  loc += code;
  if (sel >= 0) {
    loc += F("&i=");
    loc += String(sel);
  }
  g_server.sendHeader("Location", loc);
  g_server.send(303);
}

static void handleQuotePost() {
  const String action = g_server.arg("action");
  if (action == "reset") {
    if (!quoteResetToDefaults()) {
      String b;
      b += F("<div class='flash err'>恢复内置列表失败</div>");
      sendHtml(NavTab::Quote, b);
      return;
    }
    redirectOk("/quote", "quote_reset");
    return;
  }

  // 单条保存 / 新增 / 删除
  if (action == "save_one" || action == "add" || action == "delete") {
    int n = 0, stored = 0;
    QuoteItem *items = quoteLoadHeap(n, stored);
    if (!items) {
      String b;
      b += F("<div class='flash err'>名言列表内存不足</div>");
      sendHtml(NavTab::Quote, b);
      return;
    }

    if (action == "delete") {
      const int di = g_server.arg("idx").toInt();
      if (di < 0 || di >= n) {
        delete[] items;
        String b;
        b += F("<div class='flash err'>删除失败：下标无效</div>");
        sendHtml(NavTab::Quote, b);
        return;
      }
      for (int i = di; i < n - 1; i++) items[i] = items[i + 1];
      n--;
      if (stored > di) stored--;
      else if (stored >= n) stored = n > 0 ? n - 1 : 0;
      const bool ok = quoteSave(items, n, stored);
      delete[] items;
      if (!ok) {
        String b;
        b += F("<div class='flash err'>删除失败</div>");
        sendHtml(NavTab::Quote, b);
        return;
      }
      const int sel = (n > 0) ? (di < n ? di : n - 1) : -1;
      quoteRedirectSel("quote_del", sel);
      return;
    }

    QuoteItem it;
    it.text = g_server.arg("text");
    it.author = g_server.hasArg("author") ? g_server.arg("author") : String();
    it.text.trim();
    it.author.trim();
    if (it.text.isEmpty()) {
      delete[] items;
      String b;
      b += F("<div class='flash err'>正文不能为空</div>");
      sendHtml(NavTab::Quote, b);
      return;
    }
    if (it.text.length() > QUOTE_TEXT_MAX) it.text.remove(QUOTE_TEXT_MAX);
    if (it.author.length() > QUOTE_AUTHOR_MAX) it.author.remove(QUOTE_AUTHOR_MAX);

    int sel = 0;
    if (action == "add") {
      if (n >= QUOTE_ITEM_MAX) {
        delete[] items;
        String b;
        b += F("<div class='flash err'>已达上限，无法再添加</div>");
        sendHtml(NavTab::Quote, b);
        return;
      }
      items[n] = it;
      sel = n;
      n++;
    } else {
      const int ui = g_server.arg("idx").toInt();
      if (ui < 0 || ui >= n) {
        delete[] items;
        String b;
        b += F("<div class='flash err'>保存失败：下标无效</div>");
        sendHtml(NavTab::Quote, b);
        return;
      }
      items[ui] = it;
      sel = ui;
    }

    const bool ok = quoteSave(items, n, stored);
    delete[] items;
    if (!ok) {
      String b;
      b += F("<div class='flash err'>名言保存失败</div>");
      sendHtml(NavTab::Quote, b);
      return;
    }
    quoteRedirectSel(action == "add" ? "quote_add" : "quote_one", sel);
    return;
  }

  const AppConfig &cfg = getAppConfig();
  if (action == "refresh") {
    const bool refresh_en = g_server.hasArg("refresh_en");
    const uint32_t d = (uint32_t)g_server.arg("refresh_d").toInt();
    const uint32_t h = (uint32_t)g_server.arg("refresh_h").toInt();
    const uint32_t m = (uint32_t)g_server.arg("refresh_m").toInt();
    saveQuoteSettings(cfg.quote_random, refresh_en, weatherRefreshSecFromDhm(d, h, m));
    redirectOk("/quote", "quote_refresh");
    return;
  }

  // play / settings（兼容旧表单）
  const bool random_order = g_server.arg("order") == "rand";
  saveQuoteSettings(random_order, cfg.quote_refresh_en, cfg.quote_refresh_sec);
  redirectOk("/quote", "quote_play");
}

static void appendRefreshDhmFields(String &b, const char *prefix, uint32_t sec) {
  uint32_t d = 0, h = 0, m = 0;
  weatherRefreshToDhm(sec, d, h, m);
  b += F("<div class=btn-row style=align-items:flex-end;gap:10px>"
         "<label style=margin:0>天<br><input name=");
  b += prefix;
  b += F("_d type=number min=0 max=365 step=1 value=");
  b += String(d);
  b += F(" style=width:5.5em></label>"
         "<label style=margin:0>时<br><input name=");
  b += prefix;
  b += F("_h type=number min=0 max=23 step=1 value=");
  b += String(h);
  b += F(" style=width:5.5em></label>"
         "<label style=margin:0>分<br><input name=");
  b += prefix;
  b += F("_m type=number min=0 max=59 step=1 value=");
  b += String(m);
  b += F(" style=width:5.5em></label></div>");
}

static void handleScheduleGet() {
  const AppConfig &cfg = getAppConfig();
  String b;
  appendFlashFromQuery(b);

  // ① 静默时段
  b += F("<section><h2><span class=set-step>1</span>静默时段</h2>"
         "<p class=hint>静默内<strong>不作任何定时动作</strong>（刷新 / 翻页 / App 时间表）。"
         "一天可多段；结束早于开始表示跨午夜（如 22:00–06:00）。</p>"
         "<form method=POST action=/schedule><input type=hidden name=action value=quiet>"
         "<label class='check-row");
  if (cfg.quiet_en) b += F(" on");
  b += F("'><input type=checkbox name=quiet_en value=1");
  if (cfg.quiet_en) b += F(" checked");
  b += F("><span>启用静默</span></label>");
  for (int i = 0; i < QUIET_SLOT_MAX; i++) {
    char sh[8] = "", eh[8] = "";
    if (i < cfg.quiet_count) {
      scheduleFormatHm(cfg.quiet_slots[i].start_min, sh, sizeof(sh));
      scheduleFormatHm(cfg.quiet_slots[i].end_min, eh, sizeof(eh));
    }
    b += F("<div class='sched-row quiet'>"
           "<label>开始<br><input name=q_start_");
    b += String(i);
    b += F(" placeholder=22:00 value=\"");
    b += sh;
    b += F("\"></label><label>结束<br><input name=q_end_");
    b += String(i);
    b += F(" placeholder=06:00 value=\"");
    b += eh;
    b += F("\"></label></div>");
  }
  b += F("<div class=btn-row><button type=submit>保存静默时段</button></div></form></section>");

  // ② 每日 App 时间表
  b += F("<section><h2><span class=set-step>2</span>每日 App 时间表</h2>"
         "<p class=hint>到点自动切到指定 App（可选手界面）。静默时段内不切换。</p>"
         "<form method=POST action=/schedule><input type=hidden name=action value=appsched>"
         "<label class='check-row");
  if (cfg.app_sched_en) b += F(" on");
  b += F("'><input type=checkbox name=app_sched_en value=1");
  if (cfg.app_sched_en) b += F(" checked");
  b += F("><span>启用时间表</span></label>");
  for (int i = 0; i < APP_SCHED_MAX; i++) {
    char at[8] = "";
    uint8_t app = 0, view = 0;
    if (i < cfg.app_sched_count) {
      scheduleFormatHm(cfg.app_sched[i].at_min, at, sizeof(at));
      app = cfg.app_sched[i].app_id;
      view = cfg.app_sched[i].view;
    }
    b += F("<div class=sched-row><label>时间<br><input name=s_at_");
    b += String(i);
    b += F(" placeholder=08:00 value=\"");
    b += at;
    b += F("\"></label><label>App<br><select name=s_app_");
    b += String(i);
    b += F("><option value=''>—</option>");
    for (int a = 0; a < static_cast<int>(AppId::Count); a++) {
      b += F("<option value=");
      b += String(a);
      if (i < cfg.app_sched_count && app == (uint8_t)a) b += F(" selected");
      b += F(">");
      b += appIdLabelZh(static_cast<AppId>(a));
      b += F("</option>");
    }
    b += F("</select></label><label>界面<br><select name=s_view_");
    b += String(i);
    b += F("><option value=0");
    if (view == 0) b += F(" selected");
    b += F(">默认 / 天气实况</option><option value=1");
    if (view == 1) b += F(" selected");
    b += F(">天气·7日</option></select></label></div>");
  }
  b += F("<div class=btn-row><button type=submit>保存 App 时间表</button></div></form></section>");

  // ③ 一日时间轴（预览）
  b += F("<section><h2><span class=set-step>3</span>一日时间轴</h2>"
         "<p class=hint>上方静默与时间表的可视化预览。</p>"
         "<div class=sched-day>");
  for (uint8_t i = 0; i < cfg.quiet_count; i++) {
    const QuietSlot &q = cfg.quiet_slots[i];
    if (q.start_min < q.end_min) {
      const float l = q.start_min * 100.0f / 1440.0f;
      const float w = (q.end_min - q.start_min) * 100.0f / 1440.0f;
      b += F("<span class=q style='left:");
      b += String(l, 2);
      b += F("%;width:");
      b += String(w, 2);
      b += F("%'></span>");
    } else {
      // 跨午夜拆两段
      const float l1 = q.start_min * 100.0f / 1440.0f;
      b += F("<span class=q style='left:");
      b += String(l1, 2);
      b += F("%;width:");
      b += String(100.0f - l1, 2);
      b += F("%'></span><span class=q style='left:0;width:");
      b += String(q.end_min * 100.0f / 1440.0f, 2);
      b += F("%'></span>");
    }
  }
  for (uint8_t i = 0; i < cfg.app_sched_count; i++) {
    const float l = cfg.app_sched[i].at_min * 100.0f / 1440.0f;
    b += F("<span class=m title='");
    b += appIdLabelZh(static_cast<AppId>(cfg.app_sched[i].app_id));
    b += F("' style='left:");
    b += String(l, 2);
    b += F("%'></span>");
  }
  b += F("<div class=lab><span>00:00</span><span>06:00</span><span>12:00</span>"
         "<span>18:00</span><span>24:00</span></div></div>"
         "<p class='hint sm'>红块=静默；黑竖线=切 App。</p></section>");

  // ④ 各 App 定时刷新总览
  b += F("<section><h2><span class=set-step>4</span>定时刷新总览</h2>"
         "<p class=hint>统一管理各 App 定时动作；也可在对应 App 页单独改。</p>"
         "<form method=POST action=/schedule><input type=hidden name=action value=refresh>");

  auto refreshBlock = [&](const char *title, const char *en_name, bool en, const char *prefix,
                          uint32_t sec) {
    b += F("<h2 style=margin-top:18px>");
    b += title;
    b += F("</h2><label class='check-row");
    if (en) b += F(" on");
    b += F("'><input type=checkbox name=");
    b += en_name;
    b += F(" value=1");
    if (en) b += F(" checked");
    b += F("><span>启用</span></label>");
    appendRefreshDhmFields(b, prefix, sec);
  };
  refreshBlock("日天气", "wx_now_en", cfg.weather_now_refresh_en, "wx_now",
               cfg.weather_now_refresh_sec);
  refreshBlock("周天气", "wx_week_en", cfg.weather_week_refresh_en, "wx_week",
               cfg.weather_week_refresh_sec);
  refreshBlock("相册", "alb_en", cfg.album_refresh_en, "alb", cfg.album_refresh_sec);
  refreshBlock("象棋", "ch_en", cfg.chess_refresh_en, "ch", cfg.chess_refresh_sec);
  refreshBlock("名言", "qt_en", cfg.quote_refresh_en, "qt", cfg.quote_refresh_sec);
  b += F("<div class=btn-row><button type=submit>保存定时刷新</button></div></form></section>");

  sendHtml(NavTab::Schedule, b);
}

static void handleSchedulePost() {
  const String action = g_server.arg("action");
  if (action == "quiet") {
    QuietSlot slots[QUIET_SLOT_MAX];
    uint8_t n = 0;
    for (int i = 0; i < QUIET_SLOT_MAX; i++) {
      uint16_t st = 0, en = 0;
      const String ks = String("q_start_") + i;
      const String ke = String("q_end_") + i;
      if (!scheduleParseHm(g_server.arg(ks), st)) continue;
      if (!scheduleParseHm(g_server.arg(ke), en)) continue;
      slots[n].start_min = st;
      slots[n].end_min = en;
      n++;
    }
    saveQuietSchedule(g_server.hasArg("quiet_en"), slots, n);
    redirectOk("/schedule", "quiet");
    return;
  }
  if (action == "appsched") {
    AppSchedSlot slots[APP_SCHED_MAX];
    uint8_t n = 0;
    for (int i = 0; i < APP_SCHED_MAX; i++) {
      uint16_t at = 0;
      const String ka = String("s_at_") + i;
      const String kp = String("s_app_") + i;
      const String kv = String("s_view_") + i;
      if (!scheduleParseHm(g_server.arg(ka), at)) continue;
      const String app_s = g_server.arg(kp);
      if (!app_s.length()) continue;
      const int app = app_s.toInt();
      if (app < 0 || app >= static_cast<int>(AppId::Count)) continue;
      slots[n].at_min = at;
      slots[n].app_id = (uint8_t)app;
      slots[n].view = (uint8_t)g_server.arg(kv).toInt();
      n++;
    }
    saveAppSchedule(g_server.hasArg("app_sched_en"), slots, n);
    redirectOk("/schedule", "appsched");
    return;
  }

  // refresh_all
  {
    const uint32_t now_sec = weatherRefreshSecFromDhm((uint32_t)g_server.arg("wx_now_d").toInt(),
                                                     (uint32_t)g_server.arg("wx_now_h").toInt(),
                                                     (uint32_t)g_server.arg("wx_now_m").toInt());
    saveWeatherNowRefresh(g_server.hasArg("wx_now_en"), now_sec);
    const uint32_t week_sec = weatherRefreshSecFromDhm((uint32_t)g_server.arg("wx_week_d").toInt(),
                                                      (uint32_t)g_server.arg("wx_week_h").toInt(),
                                                      (uint32_t)g_server.arg("wx_week_m").toInt());
    saveWeatherWeekRefresh(g_server.hasArg("wx_week_en"), week_sec);
  }
  {
    const uint32_t alb = weatherRefreshSecFromDhm((uint32_t)g_server.arg("alb_d").toInt(),
                                                 (uint32_t)g_server.arg("alb_h").toInt(),
                                                 (uint32_t)g_server.arg("alb_m").toInt());
    saveAlbumRefreshSec(alb);
    saveAlbumRefreshEn(g_server.hasArg("alb_en"));
  }
  {
    const AppConfig &cfg = getAppConfig();
    const uint32_t ch = weatherRefreshSecFromDhm((uint32_t)g_server.arg("ch_d").toInt(),
                                                (uint32_t)g_server.arg("ch_h").toInt(),
                                                (uint32_t)g_server.arg("ch_m").toInt());
    saveChessSettings(cfg.chess_random, cfg.chess_levels, g_server.hasArg("ch_en"), ch);
  }
  {
    const AppConfig &cfg = getAppConfig();
    const uint32_t qt = weatherRefreshSecFromDhm((uint32_t)g_server.arg("qt_d").toInt(),
                                                (uint32_t)g_server.arg("qt_h").toInt(),
                                                (uint32_t)g_server.arg("qt_m").toInt());
    saveQuoteSettings(cfg.quote_random, g_server.hasArg("qt_en"), qt);
  }
  redirectOk("/schedule", "refresh_all");
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
         "长按下键约 2s 关机（深睡，再按下键唤醒）。</p></section>");

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
  row("led-anim-ack", "长按确认两闪", "长按阈值到达时闪两下，可松手；随后执行对应动作。", "UP≈1.5s 开 Web · DOWN≈2s 关机");
  row("led-anim-slow", "慢闪", "约 400ms 亮灭交替。", "正在连 WiFi / 启动配置网页");
  row("led-anim-fast", "快闪", "约 120ms 亮灭交替。", "HTTPS 拉待办 / 天气等请求中");
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
  g_server.on("/chess", HTTP_GET, handleChessGet);
  g_server.on("/chess", HTTP_POST, handleChessPost);
  g_server.on("/quote", HTTP_GET, handleQuoteGet);
  g_server.on("/quote", HTTP_POST, handleQuotePost);
  g_server.on("/schedule", HTTP_GET, handleScheduleGet);
  g_server.on("/schedule", HTTP_POST, handleSchedulePost);
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
  g_server.on("/api/device", HTTP_GET, handleApiDeviceGet);
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
