#include "app_config.h"
#include "app_apps.h"

#include <Preferences.h>
#include <ArduinoJson.h>
#include <ctime>
#include <cstring>

static AppConfig g_cfg{};
static Preferences g_prefs;

static void clearConfig() {
  memset(&g_cfg, 0, sizeof(g_cfg));
  strncpy(g_cfg.portal_base_url, "https://kylebing.cn/portal", sizeof(g_cfg.portal_base_url) - 1);
  g_cfg.idle_off_on_power = false;
  g_cfg.album_show_header = true;
  g_cfg.week_start_monday = true;
  g_cfg.tz_offset_min = 480;  // UTC+8
  g_cfg.ntp_interval_days = 30;
  g_cfg.weather_refresh_sec = 0;
  g_cfg.apps_enabled = appsEnabledDefaultMask();
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
  if (!g_prefs.begin("carte", false)) return false;
  // 从旧命名空间 note4c 迁移配置（固件更名 Carte）
  if (!g_prefs.isKey("cfg")) {
    Preferences legacy;
    if (legacy.begin("note4c", true)) {
      const String raw = legacy.getString("cfg", "");
      if (raw.length()) g_prefs.putString("cfg", raw);
      if (legacy.isKey("ui_app")) {
        g_prefs.putUChar("ui_app", legacy.getUChar("ui_app", 0));
        g_prefs.putUChar("ui_wx", legacy.getUChar("ui_wx", 0));
        g_prefs.putInt("ui_alb", legacy.getInt("ui_alb", 0));
      }
      legacy.end();
      Serial.println("[CFG] migrated prefs note4c -> carte");
    }
  }
  return true;
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

int loadSplashBuild() {
  if (!g_prefs.isKey("splash_b")) return -1;
  return g_prefs.getInt("splash_b", -1);
}

void saveSplashBuild(int build) {
  g_prefs.putInt("splash_b", build);
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

bool saveIdleOffOnPower(bool on) {
  g_cfg.idle_off_on_power = on;
  return saveAppConfig();
}

bool saveAlbumShowHeader(bool show) {
  g_cfg.album_show_header = show;
  return saveAppConfig();
}

bool saveWeekStartMonday(bool monday) {
  g_cfg.week_start_monday = monday;
  return saveAppConfig();
}

bool saveTimezoneOffsetMin(int16_t minutes) {
  if (minutes < -12 * 60) minutes = -12 * 60;
  if (minutes > 14 * 60) minutes = 14 * 60;
  g_cfg.tz_offset_min = minutes;
  return saveAppConfig();
}

bool saveNtpIntervalDays(uint16_t days) {
  if (days == 0) days = 30;
  if (days > 365) days = 365;
  g_cfg.ntp_interval_days = days;
  return saveAppConfig();
}

bool saveAppsEnabled(uint8_t mask) {
  const uint8_t all = appsEnabledDefaultMask();
  mask &= all;
  if (mask == 0) mask = all;  // 至少保留全部（防空）
  g_cfg.apps_enabled = mask;
  return saveAppConfig();
}

bool setAppEnabled(uint8_t app_id, bool enabled) {
  if (app_id >= static_cast<uint8_t>(AppId::Count)) return false;
  uint8_t mask = g_cfg.apps_enabled;
  if (enabled) {
    mask |= static_cast<uint8_t>(1u << app_id);
  } else {
    const uint8_t next = static_cast<uint8_t>(mask & ~(1u << app_id));
    if (next == 0) return false;  // 至少保留一个
    mask = next;
  }
  g_cfg.apps_enabled = mask;
  return saveAppConfig();
}

bool qweatherConfigured() {
  // 天气经 Portal 代理：需要登录 Token + 已选 LocationID（和风密钥在服务器）
  return g_cfg.qweather_enabled && g_cfg.qweather_location_id[0] && g_cfg.portal_token[0] &&
         g_cfg.portal_base_url[0];
}

static constexpr uint32_t kWeatherRefreshMinSec = 60;
static constexpr uint32_t kWeatherRefreshMaxSec = 365UL * 86400UL;

uint32_t weatherRefreshSecFromDhm(uint32_t days, uint32_t hours, uint32_t minutes) {
  if (hours > 23) hours = 23;
  if (minutes > 59) minutes = 59;
  if (days > 365) days = 365;
  const uint64_t sec =
      (uint64_t)days * 86400ULL + (uint64_t)hours * 3600ULL + (uint64_t)minutes * 60ULL;
  if (sec == 0) return 0;
  if (sec < kWeatherRefreshMinSec) return kWeatherRefreshMinSec;
  if (sec > kWeatherRefreshMaxSec) return kWeatherRefreshMaxSec;
  return (uint32_t)sec;
}

void weatherRefreshToDhm(uint32_t sec, uint32_t &days, uint32_t &hours, uint32_t &minutes) {
  days = sec / 86400UL;
  sec %= 86400UL;
  hours = sec / 3600UL;
  sec %= 3600UL;
  minutes = sec / 60UL;
}

bool saveWeatherRefreshSec(uint32_t sec) {
  if (sec != 0) {
    if (sec < kWeatherRefreshMinSec) sec = kWeatherRefreshMinSec;
    if (sec > kWeatherRefreshMaxSec) sec = kWeatherRefreshMaxSec;
  }
  g_cfg.weather_refresh_sec = sec;
  return saveAppConfig();
}

uint32_t loadWeatherLastUnix() {
  if (!g_prefs.isKey("wx_last")) return 0;
  return g_prefs.getUInt("wx_last", 0);
}

void saveWeatherLastUnix(uint32_t unix_sec) { g_prefs.putUInt("wx_last", unix_sec); }

bool weatherAutoRefreshDue() {
  const uint32_t iv = g_cfg.weather_refresh_sec;
  if (iv < kWeatherRefreshMinSec) return false;
  if (!qweatherConfigured()) return false;
  if (!appIdEnabled(AppId::Weather)) return false;
  const time_t now = time(nullptr);
  if (now < 1700000000) return false;  // 无有效时间则不瞎刷
  const uint32_t last = loadWeatherLastUnix();
  if (last == 0) return false;  // 尚未成功刷过：等用户先手动打开一次天气
  return (uint32_t)now >= last + iv;
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
  qw["refresh_sec"] = g_cfg.weather_refresh_sec;
  doc["idle_off_sec"] = g_cfg.idle_off_sec;
  doc["idle_off_on_power"] = g_cfg.idle_off_on_power;
  doc["album_show_header"] = g_cfg.album_show_header;
  doc["week_start_monday"] = g_cfg.week_start_monday;
  doc["tz_offset_min"] = g_cfg.tz_offset_min;
  doc["ntp_interval_days"] = g_cfg.ntp_interval_days;
  doc["apps_enabled"] = g_cfg.apps_enabled;

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
    if (qw["refresh_sec"].is<uint32_t>() || qw["refresh_sec"].is<int>()) {
      uint32_t sec = (uint32_t)(qw["refresh_sec"] | 0);
      if (sec != 0) {
        if (sec < 60) sec = 60;
        if (sec > 365UL * 86400UL) sec = 365UL * 86400UL;
      }
      g_cfg.weather_refresh_sec = sec;
    } else {
      g_cfg.weather_refresh_sec = 0;
    }
  } else {
    g_cfg.weather_refresh_sec = 0;
  }

  g_cfg.idle_off_sec = doc["idle_off_sec"] | 0;
  if (doc["idle_off_on_power"].is<bool>()) {
    g_cfg.idle_off_on_power = doc["idle_off_on_power"].as<bool>();
  } else {
    g_cfg.idle_off_on_power = false;
  }
  if (doc["album_show_header"].is<bool>()) {
    g_cfg.album_show_header = doc["album_show_header"].as<bool>();
  } else {
    g_cfg.album_show_header = true;
  }
  if (doc["week_start_monday"].is<bool>()) {
    g_cfg.week_start_monday = doc["week_start_monday"].as<bool>();
  } else {
    g_cfg.week_start_monday = true;
  }
  if (doc["tz_offset_min"].is<int>()) {
    g_cfg.tz_offset_min = (int16_t)doc["tz_offset_min"].as<int>();
  } else {
    g_cfg.tz_offset_min = 480;
  }
  if (doc["ntp_interval_days"].is<int>()) {
    g_cfg.ntp_interval_days = (uint16_t)doc["ntp_interval_days"].as<int>();
    if (g_cfg.ntp_interval_days == 0) g_cfg.ntp_interval_days = 30;
  } else {
    g_cfg.ntp_interval_days = 30;
  }
  if (doc["apps_enabled"].is<int>()) {
    g_cfg.apps_enabled = (uint8_t)doc["apps_enabled"].as<int>() & appsEnabledDefaultMask();
    if (g_cfg.apps_enabled == 0) g_cfg.apps_enabled = appsEnabledDefaultMask();
  } else {
    g_cfg.apps_enabled = appsEnabledDefaultMask();
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
