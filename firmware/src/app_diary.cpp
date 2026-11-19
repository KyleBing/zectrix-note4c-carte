#include "app_diary.h"
#include "app_config.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

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
