#include "app_diary.h"
#include "app_config.h"
#include "app_font.h"
#include "app_wifi.h"
#include "app_battery.h"
#include "app_theme.h"
#include "app_version.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <cstdio>
#include <cstring>

static const char *kRenewHeader = "X-Access-Token";

static String joinUrl(const char *base, const char *path) {
  String b = base ? base : "";
  while (b.endsWith("/")) b.remove(b.length() - 1);
  if (path && path[0] == '/') return b + path;
  return b + "/" + String(path ? path : "");
}

static bool beginHttp(HTTPClient &http, WiFiClientSecure &client, const String &url) {
  client.setInsecure();  // portal uses public CA; insecure OK for personal device MVP
  client.setTimeout(15000);
  return http.begin(client, url);
}

static void captureRenewedToken(HTTPClient &http) {
  // HTTPClient exposes headers via header()
  const String renewed = http.header(kRenewHeader);
  if (renewed.length() > 20) {
    Serial.println("[Diary] renew token via X-Access-Token");
    savePortalToken(renewed.c_str());
  }
}

bool diaryHasToken() { return getAppConfig().portal_token[0] != '\0'; }

bool diaryLogin(const char *email, const char *password, String &err) {
  err = "";
  if (!email || !email[0] || !password) {
    err = "email/password required";
    return false;
  }
  const AppConfig &cfg = getAppConfig();
  const String url = joinUrl(cfg.portal_base_url, "/user/login");

  JsonDocument body;
  body["email"] = email;
  body["password"] = password;
  String payload;
  serializeJson(body, payload);

  WiFiClientSecure client;
  HTTPClient http;
  if (!beginHttp(http, client, url)) {
    err = "http begin failed";
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  const char *headerKeys[] = {kRenewHeader};
  http.collectHeaders(headerKeys, 1);

  const int code = http.POST(payload);
  const String resp = http.getString();
  captureRenewedToken(http);
  http.end();

  if (code <= 0) {
    err = String("HTTP error ") + code;
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, resp)) {
    err = "bad JSON";
    return false;
  }
  if (!(doc["success"] | false)) {
    err = doc["message"] | "login failed";
    return false;
  }
  const char *token = doc["data"]["token"] | "";
  if (!token[0]) {
    err = "no token in response";
    return false;
  }
  savePortalAuth(email, token);
  Serial.println("[Diary] login ok, token saved");
  return true;
}

bool diaryFetchById(const char *diary_id, DiaryDetail &out) {
  out = DiaryDetail{};
  const AppConfig &cfg = getAppConfig();
  const char *id = (diary_id && diary_id[0]) ? diary_id : cfg.diary_id;
  if (!id || !id[0]) {
    out.message = "diary_id empty";
    return false;
  }
  if (!cfg.portal_token[0]) {
    out.message = "not logged in";
    return false;
  }

  String url = joinUrl(cfg.portal_base_url, "/diary/detail?diaryId=");
  url += id;

  WiFiClientSecure client;
  HTTPClient http;
  if (!beginHttp(http, client, url)) {
    out.message = "http begin failed";
    return false;
  }
  http.addHeader("Authorization", String("Bearer ") + cfg.portal_token);
  const char *headerKeys[] = {kRenewHeader};
  http.collectHeaders(headerKeys, 1);

  const int code = http.GET();
  const String resp = http.getString();
  captureRenewedToken(http);
  http.end();

  if (code <= 0) {
    out.message = String("HTTP error ") + code;
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, resp)) {
    out.message = "bad JSON";
    return false;
  }
  if (!(doc["success"] | false)) {
    out.message = doc["message"] | "fetch failed";
    return false;
  }

  JsonObject data = doc["data"].as<JsonObject>();
  out.ok = true;
  out.id = String(data["id"] | id);
  out.title = String(data["title"] | "");
  out.content = String(data["content"] | "");
  out.category = String(data["category"] | "");
  out.date = String(data["date"] | "");
  out.message = "ok";
  return true;
}

void diaryDrawPage(EpdSsd2683 &epd, const DiaryDetail &diary, const char *status_line) {
  epd.clear(Theme::Bg);
  // Diary is no longer an App slot; keep draw helper for possible Web/debug use.
  epd.fillRect(0, 0, 400, Theme::HeaderH, Theme::HeaderBg);
  epd.drawText(Theme::MarginX, 7, "DIARY", Theme::HeaderFg, Theme::HeaderBg);
  const int bat_left = batteryDrawHeader(epd);
  if (status_line && status_line[0] && bat_left - 40 > 160) {
    epd.drawText(160, 7, status_line, Theme::HeaderAccent, Theme::HeaderBg);
  }

  int y = 28;
  char line[64];
  if (diary.date.length() || diary.category.length()) {
    snprintf(line, sizeof(line), "%s  %s", diary.date.c_str(), diary.category.c_str());
    epd.drawText(Theme::MarginX, y, line, Theme::Accent);
    y += 14;
  }
  if (diary.title.length()) {
    y = fontDrawTextWrapped(epd, Theme::MarginX, y, 384, Theme::FooterY - 4, diary.title.c_str(),
                            Theme::Alert);
    y += 4;
  }
  epd.drawHLine(Theme::MarginX, y, 384, Theme::Fg);
  y += 4;

  if (!diary.ok) {
    fontDrawTextWrapped(epd, Theme::MarginX, y, 384, Theme::FooterY - 4,
                        diary.message.length() ? diary.message.c_str() : "(error)", Theme::Fg);
  } else if (diary.content.length()) {
    fontDrawTextWrapped(epd, Theme::MarginX, y, 384, Theme::FooterY - 4, diary.content.c_str(),
                        Theme::Fg);
  } else {
    fontDrawTextWrapped(epd, Theme::MarginX, y, 384, Theme::FooterY - 4, "(empty)", Theme::Fg);
  }

  epd.drawHLine(Theme::MarginX, Theme::FooterY, 384, Theme::Fg);
  epd.drawText(Theme::MarginX, 282, "BOOT:refresh  DN:next  UP:prev  hold:web/off", Theme::Fg);
  char ver[24];
  snprintf(ver, sizeof(ver), "%s(%d)", APP_VERSION, APP_BUILD);
  const int ver_w = (int)strlen(ver) * 6;
  epd.drawText(400 - Theme::MarginX - ver_w, 282, ver, Theme::Fg);
  epd.display();
}

bool diaryRefreshAndDraw(EpdSsd2683 &epd) {
  if (!epd.ok()) return false;
  DiaryDetail d;
  if (!wifiIsStaConnected()) {
    if (!hasSavedWifiConfig()) {
      d.message = "No WiFi. hold UP = config web";
      diaryDrawPage(epd, d, "no-wifi");
      return false;
    }
    if (!wifiConnectSaved(18000)) {
      d.message = "WiFi connect failed";
      diaryDrawPage(epd, d, "wifi-fail");
      return false;
    }
  }
  if (!diaryHasToken()) {
    d.message = "Not logged in. Open web /auth";
    diaryDrawPage(epd, d, "no-auth");
    return false;
  }
  const AppConfig &cfg = getAppConfig();
  if (!cfg.diary_id[0]) {
    d.message = "diary_id empty. Set in web /auth";
    diaryDrawPage(epd, d, "no-id");
    return false;
  }
  if (!diaryFetchById(cfg.diary_id, d)) {
    diaryDrawPage(epd, d, "err");
    return false;
  }
  diaryDrawPage(epd, d, "ok");
  return true;
}
