#include "app_web.h"
#include "app_config.h"
#include "app_wifi.h"
#include "app_diary.h"
#include "app_todo.h"
#include "app_weather.h"
#include "app_web_album.h"
#include "app_album.h"
#include "app_version.h"
#include "app_power.h"

#include <WebServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>

static WebServer g_server(80);
static bool g_running = false;
static bool g_ap = false;
static char g_url[40] = "";

enum class NavTab : uint8_t { Status, Wifi, Auth, Todo, Weather, Album };

static const char *kCss = R"CSS(
*{box-sizing:border-box}
body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;margin:0;background:#f0eee8;color:#1a1a1a;line-height:1.45}
header{background:#111;color:#fff;padding:10px 14px;display:flex;align-items:center;gap:10px 14px;flex-wrap:wrap}
header .brand{display:flex;align-items:center;gap:8px;flex-shrink:0}
header .site-logo{width:28px;height:28px;object-fit:contain;flex-shrink:0;display:block}
header h1{font-size:16px;margin:0;font-weight:650;letter-spacing:.02em;white-space:nowrap}
header nav{display:flex;flex-wrap:wrap;align-items:center;gap:6px;margin:0}
header nav a{display:inline-block;padding:5px 12px;border-radius:999px;font-size:13px;line-height:1.2;color:rgba(255,255,255,.55);text-decoration:none;border:1px solid transparent;background:transparent;transition:background .15s,color .15s,border-color .15s}
header nav a:hover{color:#fff;background:rgba(255,255,255,.08)}
header nav a.active{background:#ffcc00;color:#111;border-color:#ffcc00;font-weight:650}
main{max-width:720px;margin:16px auto;padding:0 16px 28px}
section{display:block;background:#fff;border-radius:12px;padding:18px 16px;margin-bottom:14px;box-shadow:0 1px 2px rgba(0,0,0,.05)}
h2{font-size:15px;margin:0 0 14px;font-weight:650}
.hint{color:#6b6b6b;font-size:13px;margin:0 0 14px}
.ok{color:#0a7a4b}.err{color:#b42318}
.flash{padding:10px 12px;border-radius:8px;margin:0 0 14px;font-size:13px}
.flash.ok{background:#e8f7ef;color:#0a7a4b}
.flash.err{background:#fdecec;color:#b42318}
label{display:block;font-size:12px;margin:12px 0 5px;color:#555;font-weight:500}
input,textarea,select{width:100%;padding:10px 12px;border:1px solid #ddd;border-radius:8px;font-size:14px;background:#fafafa}
input:focus,textarea:focus,select:focus{outline:0;border-color:#1a1a1a;background:#fff}
.btn-row{display:flex;flex-wrap:wrap;gap:8px;margin-top:16px}
button,.btn{background:#1a1a1a;color:#fff;border:0;border-radius:8px;padding:11px 16px;font-size:14px;cursor:pointer}
button.secondary{background:#5c5c5c}
button.danger{background:#b42318}
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
body.page-album{overflow-x:hidden;overflow-y:auto;min-height:100vh;background:#e8e6e0}
body.page-album main{max-width:none;width:100%;margin:0;padding:0;display:block}
.ed-shell{display:flex;flex-direction:column;min-width:0;background:#fff}
.ed-work{display:flex;flex-direction:column;height:auto;overflow:visible;border-bottom:1px solid #d0cec8}
.ed-toolbar{display:flex;flex-wrap:nowrap;gap:8px;align-items:center;background:#fff;border-bottom:1px solid #d0cec8;padding:6px 10px;flex:0 0 auto;overflow-x:auto}
.ed-sep{width:1px;height:28px;background:#d0cec8;flex-shrink:0}
.ed-tool-group{display:flex;flex-wrap:nowrap;gap:4px;align-items:center;flex-shrink:0}
.ed-paint{gap:6px}
.ed-erase-sz{display:flex;align-items:center;gap:6px;padding:2px 8px;background:#f3f2ee;min-width:140px}
.ed-erase-sz span{font-size:12px;color:#555;white-space:nowrap}
.ed-erase-sz input[type=range]{flex:1;padding:0;border:0;accent-color:#1a1a1a;min-width:72px}
.ed-erase-sz em{font-style:normal;font-size:12px;color:#888;width:20px;text-align:right}
.ed-toolbar button{padding:7px 10px;font-size:12px;border-radius:0;background:#eceae4;color:#1a1a1a;white-space:nowrap}
.ed-toolbar button.on,.ed-map-bar>button.on{background:#1a1a1a;color:#fff;box-shadow:none}
.ed-body{flex:0 0 auto;display:grid;grid-template-columns:1fr;min-width:0;overflow:visible;align-items:stretch}
@media(min-width:960px){.ed-body{grid-template-columns:minmax(0,1fr) 232px}}
.ed-pair{display:grid;grid-template-columns:1fr 1fr;grid-template-rows:auto 48px;min-height:0;height:auto;overflow:hidden;background:#fff}
.ed-stage{position:relative;min-width:0;line-height:0;overflow:hidden;display:flex;align-items:flex-start;justify-content:center;background:#f0eee8;padding:8px 22px 6px;grid-row:1}
.ed-stage:nth-child(2){border-left:1px solid #d0cec8}
.ed-fit{position:relative;aspect-ratio:4/3;width:90%;max-width:90%;height:auto;line-height:0;flex-shrink:0;overflow:hidden}
.ed-can,.ed-cur{position:absolute;inset:0;width:100%;height:100%;image-rendering:pixelated;border:1px solid #ccc;background:#fff;touch-action:none;display:block}
.ed-can{cursor:grab}
.ed-cur{pointer-events:none;border:0;background:transparent}
#pvStage .ed-fit{overflow:auto}
.ed-zoom{position:absolute;inset:0;width:100%;height:100%}
.ed-toast{position:fixed;top:56px;left:50%;transform:translate(-50%,-12px);background:#0a7a4b;color:#fff;padding:12px 24px;font-size:16px;font-weight:700;z-index:80;opacity:0;pointer-events:none;box-shadow:0 8px 28px rgba(0,0,0,.22);transition:opacity .18s,transform .18s;letter-spacing:.02em}
.ed-toast.show{opacity:1;transform:translate(-50%,0)}
.ed-toast.err{background:#b42318}
#loupe{position:absolute;width:96px;height:96px;border:2px solid #111;image-rendering:pixelated;background:#fff;pointer-events:none;display:none;box-shadow:0 4px 16px rgba(0,0,0,.25);z-index:2}
.ed-foot{display:flex;flex-wrap:nowrap;align-items:center;gap:6px;height:48px;padding:0 8px;overflow:visible;border-top:1px solid #d0cec8;grid-row:2;background:#fff}
.ed-foot:last-child{border-left:1px solid #d0cec8}
.ed-foot h3{margin:0;font-size:12px;font-weight:650;flex-shrink:0}
.ed-foot button{border-radius:0;padding:6px 10px;font-size:12px}
.ed-foot .meta{margin:0;font-size:11px;color:#888;min-width:0;flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.ed-side{background:#fff;padding:8px 10px;overflow:auto;min-height:0;border-left:1px solid #d0cec8;display:flex;flex-direction:column;gap:8px;align-self:stretch}
.ed-side-h{display:flex;align-items:center;justify-content:space-between;gap:8px}
.ed-side-h h3{margin:0;font-size:13px;font-weight:650}
.ed-side button{padding:4px 8px;font-size:12px;border-radius:0}
.ed-block{padding:0}
.ed-map-bar{display:flex;align-items:center;gap:8px;flex-wrap:wrap}
.ed-help{margin:auto 0 0;font-size:11px;line-height:1.35}
.hist-wrap{position:relative;user-select:none}
#hist{width:100%;height:88px;display:block;background:#3d3d3d;cursor:ew-resize}
.hist-read{display:flex;justify-content:space-between;font-size:11px;color:#888;margin-top:2px;font-variant-numeric:tabular-nums}
.ed-row{display:flex;align-items:center;gap:6px;margin:4px 0}
.ed-row span{flex:0 0 36px;font-size:12px;color:#555}
.ed-row input[type=range]{flex:1;padding:0;border:0;background:transparent;accent-color:#1a1a1a}
.ed-row em{flex:0 0 36px;font-style:normal;font-size:11px;color:#888;text-align:right}
.pal-sw{display:inline-flex;align-items:center;gap:12px;margin:0;padding:4px 2px;border:0;flex-shrink:0}
.pal-sw button,.ed-toolbar .pal-sw button{width:16px;height:16px;padding:0;margin:0;border-radius:50%;border:1px solid rgba(0,0,0,.4);font-size:0;line-height:0;cursor:pointer;box-shadow:none;outline:none;flex-shrink:0}
.pal-sw button.on,.ed-toolbar .pal-sw button.on{outline:none;box-shadow:0 0 0 2px #fff,0 0 0 4px #111;background-image:none}
.pal-sw button.pal-k,.ed-toolbar .pal-sw button.pal-k{background:#111;border-color:#111}
.pal-sw button.pal-w,.ed-toolbar .pal-sw button.pal-w{background:#fff;border-color:#888}
.pal-sw button.pal-y,.ed-toolbar .pal-sw button.pal-y{background:#F6C749;border-color:#c9a02a}
.pal-sw button.pal-r,.ed-toolbar .pal-sw button.pal-r{background:#701B0E;border-color:#4a1209}
.rules{font-size:11px;margin:4px 0 0;max-height:72px;overflow:hidden}
.rules .item{display:flex;align-items:center;gap:6px;padding:3px 0}
.sw{width:14px;height:14px;border:1px solid #ccc;flex-shrink:0}
.ed-album{padding:14px 12px 24px;background:#f0eee8;flex:0 0 auto}
.ed-album-h{display:flex;align-items:baseline;gap:10px;margin-bottom:10px;flex-wrap:wrap}
.ed-album-h h2{margin:0;font-size:14px}
.ed-saved{display:flex;flex-direction:column;gap:8px;overflow:visible;padding-bottom:4px}
.thumb{display:flex;flex-wrap:wrap;align-items:center;gap:10px;border:1px solid #ccc;padding:8px;background:#fff;width:auto;max-width:100%;box-sizing:border-box}
.thumb.on{border-color:#111;outline:1px solid #111}
.thumb.off{opacity:.45}
.thumb canvas{width:200px;max-width:100%;height:auto;aspect-ratio:4/3;image-rendering:pixelated;display:block;background:#fff;cursor:pointer;flex:0 0 auto}
.thumb .row{display:flex;align-items:center;gap:4px;margin:0;flex:1;min-width:180px;flex-wrap:wrap}
.thumb .row code{font-size:10px}
.show-lab{display:inline-flex;align-items:center;gap:3px;font-size:11px;color:#555;cursor:pointer;user-select:none}
.show-lab input{width:auto;margin:0;accent-color:#1a1a1a;padding:0}
.file-pick{display:flex;flex-direction:row;align-items:center;gap:8px;margin:0;flex-shrink:0}
.file-cta{position:relative;display:flex;align-items:center;justify-content:center;height:36px;padding:0 16px;background:#1a1a1a;color:#fff;font-size:14px;font-weight:700;cursor:pointer;border:0;overflow:hidden;letter-spacing:.02em}
.file-cta:hover{background:#333}
.file-cta input{position:absolute;inset:0;opacity:0;cursor:pointer;width:100%;height:100%;padding:0;border:0;margin:0}
.file-name{display:block;font-size:11px;line-height:1.3;color:#555;max-width:140px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.file-name.empty{color:#aaa}
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

static void sendHtml(const String &title, NavTab active, const String &body) {
  String page;
  page.reserve(body.length() + 2000);
  page += F("<!DOCTYPE html><html><head><meta charset=utf-8>"
            "<meta name=viewport content='width=device-width,initial-scale=1'>"
            "<link rel=icon href=/favicon.svg type=image/svg+xml><title>");
  page += title;
  page += F("</title><style>");
  page += kCss;
  page += F("</style></head><body");
  if (active == NavTab::Album) page += F(" class=page-album");
  page += F("><header>"
            "<div class=brand>"
            "<img class=site-logo src=/favicon.svg alt='' width=28 height=28>"
            "<h1>NOTE4C</h1></div><nav>");
  appendNavLink(page, NavTab::Status, active, "/", "状态");
  appendNavLink(page, NavTab::Wifi, active, "/wifi", "WiFi");
  appendNavLink(page, NavTab::Auth, active, "/auth", "鉴权");
  appendNavLink(page, NavTab::Todo, active, "/todo", "待办");
  appendNavLink(page, NavTab::Weather, active, "/weather", "天气");
  appendNavLink(page, NavTab::Album, active, "/album", "相册");
  page += F("</nav></header><main>");
  page += body;
  page += F("</main></body></html>");
  g_server.send(200, "text/html; charset=utf-8", page);
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
  b += F("<section><div class=status-hero><div>");
  if (wifiIsStaConnected()) {
    b += F("<span class='pill on'>STA 已连接</span>");
  } else if (g_ap) {
    b += F("<span class='pill ap'>热点配网</span>");
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
  }
  b += F("</dl></section>");

  b += F("<section><h2>电源</h2>"
         "<p class=hint>无操作自动关机是本固件自己的计时，不是芯片默认。按下任意键会重新计时。"
         "开启 Web 配置期间不会自动关机，方便慢慢配网。</p>"
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
  b += F("</select><div class=btn-row><button type=submit>保存电源设置</button></div></form></section>");
  sendHtml("状态", NavTab::Status, b);
}

static void handlePowerPost() {
  const uint32_t sec = (uint32_t)g_server.arg("idle_off_sec").toInt();
  saveIdleOffSec(sec);
  powerIdleSetTimeoutSec(sec);
  g_server.sendHeader("Location", "/");
  g_server.send(303);
}

static void handleWifiGet() {
  const AppConfig &cfg = getAppConfig();
  String b;
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
  sendHtml("WiFi", NavTab::Wifi, b);
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
  g_server.sendHeader("Location", "/wifi");
  g_server.send(303);
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
  sendHtml("鉴权", NavTab::Auth, b);
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
  sendHtml("鉴权", NavTab::Auth, b);
}

static void handleTodoGet() {
  String b;
  b += F("<section><h2>待办预览</h2>");
  b += F("<p class=hint>从 portal <code>GET /diary/detail</code> 拉取，需 STA 已连外网并登录。</p>");
  b += F("<form method=POST action=/todo><div class=btn-row>"
         "<button type=submit>立即拉取</button></div></form></section>");
  sendHtml("待办", NavTab::Todo, b);
}

static void handleTodoPost() {
  String b;
  if (!wifiIsStaConnected()) {
    b += F("<section><div class='flash err'>需要 STA 已连接外网才能拉日记</div>");
    b += F("<form method=POST action=/todo><div class=btn-row>"
           "<button type=submit>重试</button></div></form></section>");
    sendHtml("待办", NavTab::Todo, b);
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
  sendHtml("待办", NavTab::Todo, b);
}

static void appendWeatherForm(String &b, const AppConfig &cfg, const WeatherGeoResult *geo,
                              const char *flash_html) {
  if (flash_html && flash_html[0]) b += flash_html;
  b += F("<section><h2>天气位置</h2>");
  b += F("<p class=hint>设备经 <strong>Portal</strong> 拉天气（<code>/weather/*</code>），"
         "和风 JWT/Key 在服务器配置。请先在「鉴权」登录，再用下方搜索选位置。</p>");
  b += F("<form method=POST action=/weather>");
  b += F("<label><input type=checkbox name=enabled value=1");
  if (cfg.qweather_enabled) b += F(" checked");
  b += F("> 启用天气</label>");
  b += F("<label>当前 LocationID</label><input name=location_id value=\"");
  b += htmlEscapeAttr(cfg.qweather_location_id);
  b += F("\" readonly>");
  b += F("<label>位置名称</label><input name=location_name value=\"");
  b += htmlEscapeAttr(cfg.qweather_location_name);
  b += F("\" readonly>");
  b += F("<div class=btn-row><button type=submit name=action value=save>保存开关</button></div>");
  b += F("</form></section>");

  b += F("<section><h2>搜索位置</h2>");
  b += F("<p class=hint>经 Portal <code>/weather/geo</code> 调和风城市搜索；需 STA + 已登录。</p>");
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
  sendHtml("天气", NavTab::Weather, b);
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
  } else if (action == "search") {
    const String q = g_server.arg("geo_query");
    if (!weatherGeoLookup(q.c_str(), geo)) {
      // geo.message already set
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
  sendHtml("天气", NavTab::Weather, b);
}

static void handleAlbumGet() {
  albumEnsureDir();
  String b;
  webAlbumAppendPage(b);
  sendHtml("相册", NavTab::Album, b);
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

static void registerRoutes() {
  g_server.on("/", HTTP_GET, handleRoot);
  g_server.on("/power", HTTP_POST, handlePowerPost);
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
  webAlbumRegisterRoutes(g_server);
  g_server.on("/api/config", HTTP_GET, handleApiConfigGet);
}

bool webStart(bool force_ap) {
  webStop();
  g_ap = false;
  bool ok = false;

  if (!force_ap && hasSavedWifiConfig()) {
    Serial.println("[Web] try STA...");
    ok = wifiConnectSaved(15000);
  }

  if (!ok) {
    Serial.printf("[Web] start AP %s / %s\n", WIFI_AP_SSID, WIFI_AP_PASS);
    ok = wifiStartAp(WIFI_AP_SSID, WIFI_AP_PASS);
    g_ap = ok;
  }

  if (!ok) {
    Serial.println("[Web] STA and AP both failed");
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
const char *webModeName() { return g_ap ? "AP" : "STA"; }
