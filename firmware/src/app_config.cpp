#include "app_config.h"

#include <Preferences.h>
#include <ArduinoJson.h>
#include <cstring>

static AppConfig g_cfg{};
static Preferences g_prefs;

static void clearConfig() {
  memset(&g_cfg, 0, sizeof(g_cfg));
  strncpy(g_cfg.portal_base_url, "https://kylebing.cn/portal", sizeof(g_cfg.portal_base_url) - 1);
  g_cfg.album_show_header = true;
}

static void syncActiveWifiMirror() {
  g_cfg.wifi_ssid[0] = '\0';
  g_cfg.wifi_password[0] = '\0';
  if (g_cfg.wifi_count <= 0) {
    g_cfg.wifi_active[0] = '\0';
    return;
  }
  int idx = -1;
  for (int i = 0; i < g_cfg.wifi_count; i++) {
    if (g_cfg.wifi_active[0] && strcmp(g_cfg.wifis[i].ssid, g_cfg.wifi_active) == 0) {
      idx = i;
      break;
    }
  }
  if (idx < 0) {
    idx = 0;
    strncpy(g_cfg.wifi_active, g_cfg.wifis[0].ssid, sizeof(g_cfg.wifi_active) - 1);
  }
  strncpy(g_cfg.wifi_ssid, g_cfg.wifis[idx].ssid, sizeof(g_cfg.wifi_ssid) - 1);
  strncpy(g_cfg.wifi_password, g_cfg.wifis[idx].password, sizeof(g_cfg.wifi_password) - 1);
}

bool initAppStorage() {
  clearConfig();
  return g_prefs.begin("note4c", false);
}

bool loadAppConfig() {
  clearConfig();
  String raw = g_prefs.getString("cfg", "");
  if (raw.isEmpty()) {
    g_cfg.loaded = true;
    return false;
  }
  const bool ok = appConfigFromJson(raw.c_str());
  g_cfg.loaded = true;
  return ok;
}

bool saveAppConfig() {
  syncActiveWifiMirror();
  const String json = appConfigToJson(false);
  const bool ok = g_prefs.putString("cfg", json) > 0;
  g_cfg.loaded = true;
  return ok;
}

void saveUiSession(uint8_t app_id, uint8_t weather_mode, int album_index) {
  g_prefs.putUChar("ui_app", app_id);
  g_prefs.putUChar("ui_wx", weather_mode);
  g_prefs.putInt("ui_alb", album_index);
}

bool loadUiSession(uint8_t &app_id, uint8_t &weather_mode, int &album_index) {
  if (!g_prefs.isKey("ui_app")) return false;
  app_id = g_prefs.getUChar("ui_app", 0);
  weather_mode = g_prefs.getUChar("ui_wx", 0);
  album_index = g_prefs.getInt("ui_alb", 0);
  return true;
}

bool upsertAppConfigWifi(const char *ssid, const char *password, bool set_active) {
  if (!ssid || !ssid[0]) return false;
  int idx = -1;
  for (int i = 0; i < g_cfg.wifi_count; i++) {
    if (strcmp(g_cfg.wifis[i].ssid, ssid) == 0) {
      idx = i;
      break;
    }
  }
  if (idx < 0) {
    if (g_cfg.wifi_count >= WIFI_PROFILE_MAX) return false;
    idx = g_cfg.wifi_count++;
  }
  memset(&g_cfg.wifis[idx], 0, sizeof(g_cfg.wifis[idx]));
  strncpy(g_cfg.wifis[idx].ssid, ssid, sizeof(g_cfg.wifis[idx].ssid) - 1);
  if (password) {
    strncpy(g_cfg.wifis[idx].password, password, sizeof(g_cfg.wifis[idx].password) - 1);
  }
  if (set_active || g_cfg.wifi_active[0] == '\0') {
    strncpy(g_cfg.wifi_active, ssid, sizeof(g_cfg.wifi_active) - 1);
  }
  return saveAppConfig();
}

bool removeAppConfigWifi(const char *ssid) {
  if (!ssid || !ssid[0]) return false;
  int idx = -1;
  for (int i = 0; i < g_cfg.wifi_count; i++) {
    if (strcmp(g_cfg.wifis[i].ssid, ssid) == 0) {
      idx = i;
      break;
    }
  }
  if (idx < 0) return false;
  for (int i = idx; i < g_cfg.wifi_count - 1; i++) {
    g_cfg.wifis[i] = g_cfg.wifis[i + 1];
  }
  g_cfg.wifi_count--;
  memset(&g_cfg.wifis[g_cfg.wifi_count], 0, sizeof(g_cfg.wifis[0]));
  if (strcmp(g_cfg.wifi_active, ssid) == 0) {
    g_cfg.wifi_active[0] = '\0';
    if (g_cfg.wifi_count > 0) {
      strncpy(g_cfg.wifi_active, g_cfg.wifis[0].ssid, sizeof(g_cfg.wifi_active) - 1);
    }
  }
  return saveAppConfig();
}

bool setAppConfigWifiActive(const char *ssid) {
  if (!ssid || !ssid[0]) return false;
  for (int i = 0; i < g_cfg.wifi_count; i++) {
    if (strcmp(g_cfg.wifis[i].ssid, ssid) == 0) {
      strncpy(g_cfg.wifi_active, ssid, sizeof(g_cfg.wifi_active) - 1);
      return saveAppConfig();
    }
  }
  return false;
}

bool savePortalAuth(const char *email, const char *token) {
  if (email) {
    strncpy(g_cfg.portal_email, email, sizeof(g_cfg.portal_email) - 1);
  }
  if (token) {
    strncpy(g_cfg.portal_token, token, sizeof(g_cfg.portal_token) - 1);
  }
  return saveAppConfig();
}

bool savePortalToken(const char *token) {
  if (!token) return false;
  strncpy(g_cfg.portal_token, token, sizeof(g_cfg.portal_token) - 1);
  return saveAppConfig();
}

bool savePortalBaseUrl(const char *url) {
  if (!url || !url[0]) return false;
  strncpy(g_cfg.portal_base_url, url, sizeof(g_cfg.portal_base_url) - 1);
  // strip trailing slash
  size_t n = strlen(g_cfg.portal_base_url);
  while (n > 0 && g_cfg.portal_base_url[n - 1] == '/') {
    g_cfg.portal_base_url[--n] = '\0';
  }
  return saveAppConfig();
}

bool saveDiaryId(const char *id) {
  if (!id) return false;
  strncpy(g_cfg.diary_id, id, sizeof(g_cfg.diary_id) - 1);
  return saveAppConfig();
}

static void sanitizeQWeatherHost(char *host, size_t cap) {
  if (!host || cap == 0) return;
  // strip scheme / path — store hostname only (RideTrack sanitizedHost)
  String h = host;
  h.trim();
  if (h.startsWith("https://")) h.remove(0, 8);
  else if (h.startsWith("http://")) h.remove(0, 7);
  const int slash = h.indexOf('/');
  if (slash >= 0) h.remove(slash);
  memset(host, 0, cap);
  strncpy(host, h.c_str(), cap - 1);
}

bool saveQWeatherConfig(bool enabled, const char *host, const char *key, const char *loc_id,
                        const char *loc_name) {
  g_cfg.qweather_enabled = enabled;
  if (host) {
    strncpy(g_cfg.qweather_host, host, sizeof(g_cfg.qweather_host) - 1);
    sanitizeQWeatherHost(g_cfg.qweather_host, sizeof(g_cfg.qweather_host));
  }
  if (key) {
    String k = key;
    k.trim();
    memset(g_cfg.qweather_key, 0, sizeof(g_cfg.qweather_key));
    strncpy(g_cfg.qweather_key, k.c_str(), sizeof(g_cfg.qweather_key) - 1);
  }
  if (loc_id) {
    strncpy(g_cfg.qweather_location_id, loc_id, sizeof(g_cfg.qweather_location_id) - 1);
  }
  if (loc_name) {
    strncpy(g_cfg.qweather_location_name, loc_name, sizeof(g_cfg.qweather_location_name) - 1);
  }
  return saveAppConfig();
}

bool saveIdleOffSec(uint32_t sec) {
  g_cfg.idle_off_sec = sec;
  return saveAppConfig();
}

bool saveAlbumShowHeader(bool show) {
  g_cfg.album_show_header = show;
  return saveAppConfig();
}

bool qweatherConfigured() {
  // 天气经 Portal 代理：需要登录 Token + 已选 LocationID（和风密钥在服务器）
  return g_cfg.qweather_enabled && g_cfg.qweather_location_id[0] && g_cfg.portal_token[0] &&
         g_cfg.portal_base_url[0];
}

String appConfigToJson(bool mask_secrets) {
  JsonDocument doc;
  JsonArray wifis = doc["wifis"].to<JsonArray>();
  for (int i = 0; i < g_cfg.wifi_count; i++) {
    JsonObject w = wifis.add<JsonObject>();
    w["ssid"] = g_cfg.wifis[i].ssid;
    if (mask_secrets && g_cfg.wifis[i].password[0]) {
      w["password"] = "********";
    } else {
      w["password"] = g_cfg.wifis[i].password;
    }
  }
  doc["wifi_active"] = g_cfg.wifi_active;
  JsonObject portal = doc["portal"].to<JsonObject>();
  portal["base_url"] = g_cfg.portal_base_url;
  portal["email"] = g_cfg.portal_email;
  if (mask_secrets && g_cfg.portal_token[0]) {
    portal["token"] = String(g_cfg.portal_token).substring(0, 12) + "...";
    portal["token_set"] = true;
  } else {
    portal["token"] = g_cfg.portal_token;
    portal["token_set"] = g_cfg.portal_token[0] != '\0';
  }
  portal["diary_id"] = g_cfg.diary_id;

  JsonObject qw = doc["qweather"].to<JsonObject>();
  qw["enabled"] = g_cfg.qweather_enabled;
  qw["host"] = g_cfg.qweather_host;
  if (mask_secrets && g_cfg.qweather_key[0]) {
    qw["key"] = String(g_cfg.qweather_key).substring(0, 4) + "********";
    qw["key_set"] = true;
  } else {
    qw["key"] = g_cfg.qweather_key;
    qw["key_set"] = g_cfg.qweather_key[0] != '\0';
  }
  qw["location_id"] = g_cfg.qweather_location_id;
  qw["location_name"] = g_cfg.qweather_location_name;
  doc["idle_off_sec"] = g_cfg.idle_off_sec;
  doc["album_show_header"] = g_cfg.album_show_header;

  String out;
  serializeJsonPretty(doc, out);
  return out;
}

bool appConfigFromJson(const char *json) {
  if (!json) return false;
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, json);
  if (err) return false;

  g_cfg.wifi_count = 0;
  memset(g_cfg.wifis, 0, sizeof(g_cfg.wifis));
  if (doc["wifis"].is<JsonArray>()) {
    for (JsonObject w : doc["wifis"].as<JsonArray>()) {
      if (g_cfg.wifi_count >= WIFI_PROFILE_MAX) break;
      const char *ssid = w["ssid"] | "";
      if (!ssid[0]) continue;
      WifiProfile &p = g_cfg.wifis[g_cfg.wifi_count++];
      strncpy(p.ssid, ssid, sizeof(p.ssid) - 1);
      const char *pass = w["password"] | "";
      if (strcmp(pass, "********") != 0) {
        strncpy(p.password, pass, sizeof(p.password) - 1);
      }
    }
  } else if (doc["wifi"].is<JsonObject>()) {
    // legacy single wifi
    const char *ssid = doc["wifi"]["ssid"] | "";
    if (ssid[0]) {
      strncpy(g_cfg.wifis[0].ssid, ssid, sizeof(g_cfg.wifis[0].ssid) - 1);
      strncpy(g_cfg.wifis[0].password, doc["wifi"]["password"] | "", sizeof(g_cfg.wifis[0].password) - 1);
      g_cfg.wifi_count = 1;
    }
  }

  strncpy(g_cfg.wifi_active, doc["wifi_active"] | "", sizeof(g_cfg.wifi_active) - 1);

  JsonObject portal = doc["portal"].as<JsonObject>();
  if (!portal.isNull()) {
    const char *url = portal["base_url"] | "https://kylebing.cn/portal";
    strncpy(g_cfg.portal_base_url, url, sizeof(g_cfg.portal_base_url) - 1);
    strncpy(g_cfg.portal_email, portal["email"] | "", sizeof(g_cfg.portal_email) - 1);
    const char *tok = portal["token"] | "";
    if (tok[0] && !strstr(tok, "...")) {
      strncpy(g_cfg.portal_token, tok, sizeof(g_cfg.portal_token) - 1);
    }
    strncpy(g_cfg.diary_id, portal["diary_id"] | "", sizeof(g_cfg.diary_id) - 1);
  }

  JsonObject qw = doc["qweather"].as<JsonObject>();
  if (!qw.isNull()) {
    g_cfg.qweather_enabled = qw["enabled"] | false;
    strncpy(g_cfg.qweather_host, qw["host"] | "", sizeof(g_cfg.qweather_host) - 1);
    sanitizeQWeatherHost(g_cfg.qweather_host, sizeof(g_cfg.qweather_host));
    const char *key = qw["key"] | "";
    if (key[0] && !strstr(key, "********")) {
      strncpy(g_cfg.qweather_key, key, sizeof(g_cfg.qweather_key) - 1);
    }
    strncpy(g_cfg.qweather_location_id, qw["location_id"] | "", sizeof(g_cfg.qweather_location_id) - 1);
    strncpy(g_cfg.qweather_location_name, qw["location_name"] | "",
            sizeof(g_cfg.qweather_location_name) - 1);
  }

  g_cfg.idle_off_sec = doc["idle_off_sec"] | 0;
  if (doc["album_show_header"].is<bool>()) {
    g_cfg.album_show_header = doc["album_show_header"].as<bool>();
  } else {
    g_cfg.album_show_header = true;
  }

  size_t n = strlen(g_cfg.portal_base_url);
  while (n > 0 && g_cfg.portal_base_url[n - 1] == '/') {
    g_cfg.portal_base_url[--n] = '\0';
  }
  syncActiveWifiMirror();
  return true;
}

const AppConfig &getAppConfig() { return g_cfg; }

bool hasSavedWifiConfig() {
  return g_cfg.wifi_count > 0 && g_cfg.wifi_ssid[0] != '\0';
}
