#include "app_web.h"
#include "app_config.h"
#include "app_wifi.h"
#include "app_diary.h"
#include "app_todo.h"
#include "app_version.h"

#include <WebServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>

static WebServer g_server(80);
static bool g_running = false;
static bool g_ap = false;
static char g_url[40] = "";

static const char *kCss = R"CSS(
body{font-family:-apple-system,BlinkMacSystemFont,sans-serif;margin:0;background:#f4f1ea;color:#1a1a1a}
header{background:#1a1a1a;color:#fff;padding:12px 16px}
header a{color:#f5d76e;margin-right:12px;text-decoration:none}
main{max-width:720px;margin:16px auto;padding:0 16px}
card,section{display:block;background:#fff;border-radius:8px;padding:16px;margin-bottom:16px;box-shadow:0 1px 3px rgba(0,0,0,.08)}
h1{font-size:18px;margin:0}
h2{font-size:16px;margin:0 0 12px}
label{display:block;font-size:13px;margin:10px 0 4px}
input,textarea{width:100%;box-sizing:border-box;padding:8px;border:1px solid #ccc;border-radius:6px;font-size:14px}
button,.btn{background:#1a1a1a;color:#fff;border:0;border-radius:6px;padding:10px 14px;font-size:14px;cursor:pointer;margin:6px 6px 0 0}
button.secondary{background:#666}
button.danger{background:#a33}
.hint{color:#666;font-size:13px}
.ok{color:#0a7}
.err{color:#a33}
table{width:100%;border-collapse:collapse}
td,th{padding:6px;border-bottom:1px solid #eee;text-align:left;font-size:13px}
pre{white-space:pre-wrap;background:#f7f7f7;padding:10px;border-radius:6px;font-size:13px}
)CSS";

static void sendHtml(const String &title, const String &body) {
  String page;
  page.reserve(body.length() + 800);
  page += F("<!DOCTYPE html><html><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>");
  page += F("<title>");
  page += title;
  page += F("</title><style>");
  page += kCss;
  page += F("</style></head><body><header><h1>NOTE4C</h1>");
  page += F("<a href=/>状态</a><a href=/wifi>WiFi</a><a href=/auth>鉴权</a><a href=/todo>待办</a>");
  page += F("</header><main>");
  page += body;
  page += F("</main></body></html>");
  g_server.send(200, "text/html; charset=utf-8", page);
}

static void handleRoot() {
  const AppConfig &cfg = getAppConfig();
  String b;
  b += F("<section><h2>状态</h2><p class=hint>版本 ");
  b += APP_VERSION;
  b += F("</p><p>模式：");
  b += webModeName();
  b += F("</p><p>地址：");
  b += g_url;
  b += F("</p>");
  if (wifiIsStaConnected()) {
    b += F("<p class=ok>STA 已连接：");
    b += wifiCurrentSsid();
    b += F(" / ");
    b += wifiStaIp();
    b += F(" RSSI ");
    b += String(wifiStaRssi());
    b += F("</p>");
  } else if (g_ap) {
    b += F("<p>热点：");
    b += WIFI_AP_SSID;
    b += F(" / ");
    b += WIFI_AP_PASS;
    b += F(" → ");
    b += wifiApIp();
    b += F("</p>");
  }
  b += F("<p>Portal：");
  b += cfg.portal_base_url;
  b += F("</p><p>邮箱：");
  b += cfg.portal_email[0] ? cfg.portal_email : "(未登录)";
  b += F("</p><p>Token：");
  b += cfg.portal_token[0] ? "已保存（快过期时自动续签 X-Access-Token）" : "无";
  b += F("</p><p>Diary ID：");
  b += cfg.diary_id[0] ? cfg.diary_id : "(未设置)";
  b += F("</p></section>");
  sendHtml("NOTE4C", b);
}

static void handleWifiGet() {
  const AppConfig &cfg = getAppConfig();
  String b;
  b += F("<section><h2>WiFi（最多 5 条）</h2>");
  b += F("<p class=hint>保存后设备会按 Active → 其它条目自动尝试连接。也可开热点配网。</p>");
  b += F("<form method=POST action=/wifi>");
  for (int i = 0; i < WIFI_PROFILE_MAX; i++) {
    const char *ssid = i < cfg.wifi_count ? cfg.wifis[i].ssid : "";
    const char *pass = i < cfg.wifi_count ? cfg.wifis[i].password : "";
    const bool active = ssid[0] && strcmp(ssid, cfg.wifi_active) == 0;
    b += F("<fieldset style='border:1px solid #ddd;border-radius:8px;padding:10px;margin-bottom:10px'>");
    b += F("<label><input type=radio name=active value=");
    b += String(i);
    if (active) b += F(" checked");
    b += F("> Active #");
    b += String(i + 1);
    b += F("</label>");
    b += F("<label>SSID</label><input name=ssid");
    b += String(i);
    b += F(" value='");
    b += ssid;
    b += F("'>");
    b += F("<label>密码</label><input name=pass");
    b += String(i);
    b += F(" value='");
    b += pass;
    b += F("'></fieldset>");
  }
  b += F("<button type=submit>保存 WiFi</button></form></section>");
  sendHtml("WiFi", b);
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

static void handleAuthGet() {
  const AppConfig &cfg = getAppConfig();
  String b;
  b += F("<section><h2>Portal 鉴权 / Diary</h2>");
  b += F("<p class=hint>登录调用 portal-go <code>POST /user/login</code>，保存 JWT。"
         "业务请求若响应头带 <code>X-Access-Token</code>（剩余 &lt; 7 天）会自动写回。</p>");
  b += F("<form method=POST action=/auth>");
  b += F("<label>API Base URL</label><input name=base_url value='");
  b += cfg.portal_base_url;
  b += F("'>");
  b += F("<label>邮箱</label><input name=email type=email value='");
  b += cfg.portal_email;
  b += F("'>");
  b += F("<label>密码（仅本次登录，不落盘）</label><input name=password type=password>");
  b += F("<label>Diary ID</label><input name=diary_id value='");
  b += cfg.diary_id;
  b += F("'>");
  b += F("<button type=submit name=action value=save>保存 URL / ID</button>");
  b += F("<button type=submit name=action value=login>登录并保存 Token</button>");
  b += F("<button type=submit name=action value=logout class=danger>清除 Token</button>");
  b += F("</form></section>");
  sendHtml("鉴权", b);
}

static void handleAuthPost() {
  const String action = g_server.arg("action");
  const String base = g_server.arg("base_url");
  const String email = g_server.arg("email");
  const String password = g_server.arg("password");
  const String diary_id = g_server.arg("diary_id");

  if (base.length()) savePortalBaseUrl(base.c_str());
  if (diary_id.length()) saveDiaryId(diary_id.c_str());

  String flash;
  if (action == "login") {
    String err;
    if (diaryLogin(email.c_str(), password.c_str(), err)) {
      flash = "<p class=ok>登录成功，Token 已保存</p>";
    } else {
      flash = String("<p class=err>登录失败：") + err + "</p>";
    }
  } else if (action == "logout") {
    savePortalAuth(email.c_str(), "");
    flash = "<p class=ok>已清除 Token</p>";
  } else {
    if (email.length()) savePortalAuth(email.c_str(), getAppConfig().portal_token);
    flash = "<p class=ok>已保存</p>";
  }

  String b = flash;
  // re-render form
  const AppConfig &cfg = getAppConfig();
  b += F("<section><h2>Portal 鉴权 / Diary</h2><form method=POST action=/auth>");
  b += F("<label>API Base URL</label><input name=base_url value='");
  b += cfg.portal_base_url;
  b += F("'>");
  b += F("<label>邮箱</label><input name=email type=email value='");
  b += cfg.portal_email;
  b += F("'>");
  b += F("<label>密码</label><input name=password type=password>");
  b += F("<label>Diary ID</label><input name=diary_id value='");
  b += cfg.diary_id;
  b += F("'>");
  b += F("<button type=submit name=action value=save>保存 URL / ID</button>");
  b += F("<button type=submit name=action value=login>登录并保存 Token</button>");
  b += F("<button type=submit name=action value=logout class=danger>清除 Token</button>");
  b += F("</form></section>");
  sendHtml("鉴权", b);
}

static void handleTodoGet() {
  String b;
  b += F("<section><h2>待办预览</h2>");
  b += F("<p class=hint>从 portal <code>GET /diary/detail?diaryId=</code> 拉取。需已连网并登录。</p>");
  b += F("<form method=POST action=/todo><button type=submit>立即拉取</button></form>");
  b += F("</section>");
  sendHtml("待办", b);
}

static void handleTodoPost() {
  String b;
  if (!wifiIsStaConnected() && !g_ap) {
    // AP mode can't reach internet usually
  }
  if (!wifiIsStaConnected()) {
    b += F("<section><p class=err>需要 STA 已连接外网才能拉日记</p></section>");
    sendHtml("待办", b);
    return;
  }
  DiaryDetail d;
  if (!diaryFetchById(nullptr, d)) {
    b += F("<section><p class=err>");
    b += d.message;
    b += F("</p></section>");
  } else {
    TodoItem items[TODO_ITEM_MAX];
    const int n = todoParseContent(d.content, items, TODO_ITEM_MAX);
    int open_n = 0;
    for (int i = 0; i < n; i++) {
      if (!items[i].done) open_n++;
    }
    b += F("<section><h2>");
    b += d.title.length() ? d.title : String("Todo");
    b += F("</h2><p class=hint>#");
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
      b += F("<ul style='list-style:none;padding:0;margin:0'>");
      for (int i = 0; i < n; i++) {
        b += F("<li style='padding:8px 0;border-bottom:1px solid #eee");
        if (items[i].done) b += F(";opacity:.55;text-decoration:line-through");
        b += F("'>");
        b += items[i].done ? F("☑ ") : F("☐ ");
        String c = items[i].content;
        c.replace("&", "&amp;");
        c.replace("<", "&lt;");
        b += c;
        if (items[i].note.length()) {
          b += F(" <span class=hint>#");
          String note = items[i].note;
          note.replace("&", "&amp;");
          note.replace("<", "&lt;");
          b += note;
          b += F("</span>");
        }
        b += F("</li>");
      }
      b += F("</ul>");
    }
    b += F("</section>");
  }
  b += F("<section><form method=POST action=/todo><button type=submit>再拉一次</button></form></section>");
  sendHtml("待办", b);
}

static void handleApiConfigGet() {
  g_server.send(200, "application/json", appConfigToJson(true));
}

static void registerRoutes() {
  g_server.on("/", HTTP_GET, handleRoot);
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
