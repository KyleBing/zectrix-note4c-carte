#include "app_web.h"
#include "app_config.h"
#include "app_wifi.h"
#include "app_diary.h"
#include "app_todo.h"
#include "app_battery.h"
#include "app_version.h"

#include <WebServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>

static WebServer g_server(80);
static bool g_running = false;
static bool g_ap = false;
static char g_url[40] = "";

enum class NavTab : uint8_t { Status, Wifi, Auth, Todo };

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
header .bat{display:inline-flex;align-items:center;gap:7px;margin-left:auto;flex-shrink:0;font-size:12px;color:rgba(255,255,255,.85);user-select:none}
header .bat.chg{color:#7dffa0}
header .bat.full{color:#ffcc00}
header .bat-bolt{font-size:11px;line-height:1;opacity:.95}
header .bat-icon{position:relative;width:26px;height:12px;border:1.5px solid currentColor;border-radius:2px;box-sizing:border-box}
header .bat-icon::after{content:"";position:absolute;right:-4px;top:2px;width:2px;height:6px;background:currentColor;border-radius:0 1px 1px 0}
header .bat-icon i{display:block;height:100%;background:currentColor;border-radius:1px}
header .bat-pct{font-variant-numeric:tabular-nums;font-weight:650;min-width:2.6em}
main{max-width:720px;margin:16px auto;padding:0 16px 28px}
section{display:block;background:#fff;border-radius:12px;padding:18px 16px;margin-bottom:14px;box-shadow:0 1px 2px rgba(0,0,0,.05)}
h2{font-size:15px;margin:0 0 14px;font-weight:650}
.hint{color:#6b6b6b;font-size:13px;margin:0 0 14px}
.ok{color:#0a7a4b}.err{color:#b42318}
.flash{padding:10px 12px;border-radius:8px;margin:0 0 14px;font-size:13px}
.flash.ok{background:#e8f7ef;color:#0a7a4b}
.flash.err{background:#fdecec;color:#b42318}
label{display:block;font-size:12px;margin:12px 0 5px;color:#555;font-weight:500}
input,textarea{width:100%;padding:10px 12px;border:1px solid #ddd;border-radius:8px;font-size:14px;background:#fafafa}
input:focus,textarea:focus{outline:0;border-color:#1a1a1a;background:#fff}
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

static void appendBatteryWidget(String &page) {
  const BatteryStatus bat = batteryRead();
  page += F("<div class='bat");
  if (bat.charge == BatteryCharge::Charging) page += F(" chg");
  else if (bat.charge == BatteryCharge::Full) page += F(" full");
  page += F("' title='");
  if (bat.ok) {
    page += String(bat.mv);
    page += F(" mV · ");
    page += batteryChargeLabel(bat.charge);
  } else {
    page += F("电池未读到");
  }
  page += F("'>");
  if (bat.charge == BatteryCharge::Charging) page += F("<span class=bat-bolt>⚡</span>");
  page += F("<span class=bat-icon><i style=width:");
  page += String(bat.ok ? bat.percent : 0);
  page += F("%></i></span><span class=bat-pct>");
  if (bat.ok) {
    page += String(bat.percent);
    page += F("%");
  } else {
    page += F("--");
  }
  page += F("</span></div>");
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
  page += F("</style></head><body><header>"
            "<div class=brand>"
            "<img class=site-logo src=/favicon.svg alt='' width=28 height=28>"
            "<h1>NOTE4C</h1></div><nav>");
  appendNavLink(page, NavTab::Status, active, "/", "状态");
  appendNavLink(page, NavTab::Wifi, active, "/wifi", "WiFi");
  appendNavLink(page, NavTab::Auth, active, "/auth", "鉴权");
  appendNavLink(page, NavTab::Todo, active, "/todo", "待办");
  page += F("</nav>");
  appendBatteryWidget(page);
  page += F("</header><main>");
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
  const BatteryStatus bat = batteryRead();
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
  if (bat.ok) {
    String bat_line = String(bat.percent) + "% · " + String(bat.mv) + " mV · " +
                      batteryChargeLabel(bat.charge);
    kvRow(b, "电池", bat_line);
  } else {
    kvRow(b, "电池", String("未读到（检查 ADC）"));
  }
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
  b += F("</dl></section>");
  sendHtml("状态", NavTab::Status, b);
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
  // preserve portal
  const AppConfig &cfg = getAppConfig();
  JsonObject portal = doc["portal"].to<JsonObject>();
  portal["base_url"] = cfg.portal_base_url;
  portal["email"] = cfg.portal_email;
  portal["token"] = cfg.portal_token;
  portal["diary_id"] = cfg.diary_id;

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
  g_server.on("/favicon.svg", HTTP_GET, handleFavicon);
  g_server.on("/wifi", HTTP_GET, handleWifiGet);
  g_server.on("/wifi", HTTP_POST, handleWifiPost);
  g_server.on("/auth", HTTP_GET, handleAuthGet);
  g_server.on("/auth", HTTP_POST, handleAuthPost);
  g_server.on("/todo", HTTP_GET, handleTodoGet);
  g_server.on("/todo", HTTP_POST, handleTodoPost);
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
