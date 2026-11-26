#include "app_config.h"
#include "app_apps.h"
#include "app_chess.h"

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
  g_cfg.album_refresh_en = false;
  g_cfg.album_refresh_sec = 3600;
  g_cfg.album_random = false;
  g_cfg.week_start_monday = true;
  g_cfg.tz_offset_min = 480;  // UTC+8
  g_cfg.ntp_interval_days = 30;
  g_cfg.todo_use_local = false;
  g_cfg.qweather_source = QW_SRC_PORTAL;
  g_cfg.weather_now_refresh_en = false;
  g_cfg.weather_now_refresh_sec = 3600;
  g_cfg.weather_week_refresh_en = false;
  g_cfg.weather_week_refresh_sec = 21600;  // 周预报默认 6 小时
  g_cfg.chess_random = false;
  g_cfg.chess_levels = CHESS_LV_ALL;
  g_cfg.chess_refresh_en = false;
  g_cfg.chess_refresh_sec = 3600;
  g_cfg.quote_random = false;
  g_cfg.quote_refresh_en = false;
  g_cfg.quote_refresh_sec = 3600;
  g_cfg.quiet_en = false;
  g_cfg.quiet_count = 0;
  g_cfg.app_sched_en = false;
  g_cfg.app_sched_count = 0;
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

bool loadScheduledSleep() { return g_prefs.getBool("sched_slp", false); }

void saveScheduledSleep(bool on) { g_prefs.putBool("sched_slp", on); }

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

bool saveQWeatherConfig(bool enabled, uint8_t source, const char *host, const char *key,
                        const char *loc_id, const char *loc_name) {
  g_cfg.qweather_enabled = enabled;
  g_cfg.qweather_source = (source == QW_SRC_APIKEY) ? QW_SRC_APIKEY : QW_SRC_PORTAL;
  if (host) {
    strncpy(g_cfg.qweather_host, host, sizeof(g_cfg.qweather_host) - 1);
    sanitizeQWeatherHost(g_cfg.qweather_host, sizeof(g_cfg.qweather_host));
  }
  if (key) {
    String k = key;
    k.trim();
    // 空串或掩码占位：保留已有 Key，避免表单回写把密钥清掉
    if (k.length() && k.indexOf("********") < 0) {
      memset(g_cfg.qweather_key, 0, sizeof(g_cfg.qweather_key));
      strncpy(g_cfg.qweather_key, k.c_str(), sizeof(g_cfg.qweather_key) - 1);
    }
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

bool qweatherSourceIsApiKey() { return g_cfg.qweather_source == QW_SRC_APIKEY; }

bool qweatherConfigured() {
  if (!g_cfg.qweather_enabled || !g_cfg.qweather_location_id[0]) return false;
  if (qweatherSourceIsApiKey()) {
    // 和风普通 API：Host + Key + LocationID
    return g_cfg.qweather_host[0] && g_cfg.qweather_key[0];
  }
  // Portal 代理：登录 Token + Base URL（和风密钥在服务器）
  return g_cfg.portal_token[0] && g_cfg.portal_base_url[0];
}

static constexpr uint32_t kRefreshMinSec = 60;
static constexpr uint32_t kRefreshMaxSec = 365UL * 86400UL;
static constexpr uint32_t kRefreshDefaultSec = 3600;
static constexpr uint32_t kWeatherWeekDefaultSec = 21600;  // 6h

static uint32_t clampRefreshSec(uint32_t sec) {
  if (sec < kRefreshMinSec) sec = kRefreshMinSec;
  if (sec > kRefreshMaxSec) sec = kRefreshMaxSec;
  return sec;
}

uint32_t weatherRefreshSecFromDhm(uint32_t days, uint32_t hours, uint32_t minutes) {
  if (hours > 23) hours = 23;
  if (minutes > 59) minutes = 59;
  if (days > 365) days = 365;
  const uint64_t sec =
      (uint64_t)days * 86400ULL + (uint64_t)hours * 3600ULL + (uint64_t)minutes * 60ULL;
  if (sec == 0) return kRefreshDefaultSec;
  return clampRefreshSec((uint32_t)sec);
}

void weatherRefreshToDhm(uint32_t sec, uint32_t &days, uint32_t &hours, uint32_t &minutes) {
  if (sec == 0) sec = kRefreshDefaultSec;
  days = sec / 86400UL;
  sec %= 86400UL;
  hours = sec / 3600UL;
  sec %= 3600UL;
  minutes = sec / 60UL;
}

bool weatherModeIsWeek(uint8_t weather_mode) {
  // WeatherMode::Week7=2, Week14=3
  return weather_mode == 2 || weather_mode == 3;
}

bool saveWeatherNowRefresh(bool en, uint32_t sec) {
  g_cfg.weather_now_refresh_en = en;
  g_cfg.weather_now_refresh_sec = clampRefreshSec(sec == 0 ? kRefreshDefaultSec : sec);
  if (en && g_cfg.weather_now_refresh_sec < kRefreshMinSec) {
    g_cfg.weather_now_refresh_sec = kRefreshDefaultSec;
  }
  return saveAppConfig();
}

bool saveWeatherWeekRefresh(bool en, uint32_t sec) {
  g_cfg.weather_week_refresh_en = en;
  g_cfg.weather_week_refresh_sec = clampRefreshSec(sec == 0 ? kWeatherWeekDefaultSec : sec);
  if (en && g_cfg.weather_week_refresh_sec < kRefreshMinSec) {
    g_cfg.weather_week_refresh_sec = kWeatherWeekDefaultSec;
  }
  return saveAppConfig();
}

uint32_t loadWeatherLastUnixForMode(uint8_t weather_mode) {
  const char *key = weatherModeIsWeek(weather_mode) ? "wx_week_last" : "wx_now_last";
  // 旧单一 wx_last：无分档记录时回退，避免升级后立刻连刷
  if (!g_prefs.isKey(key)) {
    if (g_prefs.isKey("wx_last")) return g_prefs.getUInt("wx_last", 0);
    return 0;
  }
  return g_prefs.getUInt(key, 0);
}

void saveWeatherLastUnixForMode(uint8_t weather_mode, uint32_t unix_sec) {
  const char *key = weatherModeIsWeek(weather_mode) ? "wx_week_last" : "wx_now_last";
  g_prefs.putUInt(key, unix_sec);
}

uint32_t weatherRefreshSecForMode(uint8_t weather_mode) {
  if (weatherModeIsWeek(weather_mode)) {
    if (!g_cfg.weather_week_refresh_en) return 0;
    return g_cfg.weather_week_refresh_sec;
  }
  if (!g_cfg.weather_now_refresh_en) return 0;
  return g_cfg.weather_now_refresh_sec;
}

bool weatherAutoRefreshDue(uint8_t weather_mode) {
  const uint32_t iv = weatherRefreshSecForMode(weather_mode);
  if (iv < kRefreshMinSec) return false;
  if (!qweatherConfigured()) return false;
  if (!appIdEnabled(AppId::Weather)) return false;
  const time_t now = time(nullptr);
  if (now < 1700000000) return false;
  const uint32_t last = loadWeatherLastUnixForMode(weather_mode);
  if (last == 0) return false;
  return (uint32_t)now >= last + iv;
}

bool saveAlbumRefreshEn(bool en) {
  g_cfg.album_refresh_en = en;
  if (en && g_cfg.album_refresh_sec < kRefreshMinSec) {
    g_cfg.album_refresh_sec = kRefreshDefaultSec;
  }
  return saveAppConfig();
}

bool saveAlbumRefreshSec(uint32_t sec) {
  g_cfg.album_refresh_sec = clampRefreshSec(sec == 0 ? kRefreshDefaultSec : sec);
  return saveAppConfig();
}

uint32_t loadAlbumLastUnix() {
  if (!g_prefs.isKey("alb_last")) return 0;
  return g_prefs.getUInt("alb_last", 0);
}

void saveAlbumLastUnix(uint32_t unix_sec) { g_prefs.putUInt("alb_last", unix_sec); }

bool albumAutoRefreshDue() {
  if (!g_cfg.album_refresh_en) return false;
  const uint32_t iv = g_cfg.album_refresh_sec;
  if (iv < kRefreshMinSec) return false;
  if (!appIdEnabled(AppId::Album)) return false;
  const time_t now = time(nullptr);
  if (now < 1700000000) return false;
  const uint32_t last = loadAlbumLastUnix();
  if (last == 0) return false;
  return (uint32_t)now >= last + iv;
}

bool saveChessSettings(bool random_order, uint8_t levels, bool refresh_en, uint32_t refresh_sec) {
  g_cfg.chess_random = random_order;
  g_cfg.chess_levels = levels ? levels : CHESS_LV_ALL;
  g_cfg.chess_refresh_en = refresh_en;
  g_cfg.chess_refresh_sec = clampRefreshSec(refresh_sec == 0 ? kRefreshDefaultSec : refresh_sec);
  if (refresh_en && g_cfg.chess_refresh_sec < kRefreshMinSec) {
    g_cfg.chess_refresh_sec = kRefreshDefaultSec;
  }
  return saveAppConfig();
}

uint32_t loadChessLastUnix() {
  if (!g_prefs.isKey("ch_last")) return 0;
  return g_prefs.getUInt("ch_last", 0);
}

void saveChessLastUnix(uint32_t unix_sec) { g_prefs.putUInt("ch_last", unix_sec); }

bool chessAutoRefreshDue() {
  if (!g_cfg.chess_refresh_en) return false;
  const uint32_t iv = g_cfg.chess_refresh_sec;
  if (iv < kRefreshMinSec) return false;
  if (!appIdEnabled(AppId::Chess)) return false;
  const time_t now = time(nullptr);
  if (now < 1700000000) return false;
  const uint32_t last = loadChessLastUnix();
  if (last == 0) return false;
  return (uint32_t)now >= last + iv;
}

bool saveQuoteSettings(bool random_order, bool refresh_en, uint32_t refresh_sec) {
  g_cfg.quote_random = random_order;
  g_cfg.quote_refresh_en = refresh_en;
  g_cfg.quote_refresh_sec = clampRefreshSec(refresh_sec == 0 ? kRefreshDefaultSec : refresh_sec);
  if (refresh_en && g_cfg.quote_refresh_sec < kRefreshMinSec) {
    g_cfg.quote_refresh_sec = kRefreshDefaultSec;
  }
  return saveAppConfig();
}

uint32_t loadQuoteLastUnix() {
  if (!g_prefs.isKey("qt_last")) return 0;
  return g_prefs.getUInt("qt_last", 0);
}

void saveQuoteLastUnix(uint32_t unix_sec) { g_prefs.putUInt("qt_last", unix_sec); }

bool saveAlbumRandom(bool random_order) {
  g_cfg.album_random = random_order;
  return saveAppConfig();
}

bool saveTodoUseLocal(bool use_local) {
  g_cfg.todo_use_local = use_local;
  return saveAppConfig();
}

bool saveQuietSchedule(bool en, const QuietSlot *slots, uint8_t count) {
  g_cfg.quiet_en = en;
  g_cfg.quiet_count = 0;
  memset(g_cfg.quiet_slots, 0, sizeof(g_cfg.quiet_slots));
  if (slots && count) {
    if (count > QUIET_SLOT_MAX) count = QUIET_SLOT_MAX;
    for (uint8_t i = 0; i < count; i++) {
      if (slots[i].start_min > 1439 || slots[i].end_min > 1439) continue;
      if (slots[i].start_min == slots[i].end_min) continue;
      g_cfg.quiet_slots[g_cfg.quiet_count++] = slots[i];
    }
  }
  return saveAppConfig();
}

bool saveAppSchedule(bool en, const AppSchedSlot *slots, uint8_t count) {
  g_cfg.app_sched_en = en;
  g_cfg.app_sched_count = 0;
  memset(g_cfg.app_sched, 0, sizeof(g_cfg.app_sched));
  if (slots && count) {
    if (count > APP_SCHED_MAX) count = APP_SCHED_MAX;
    for (uint8_t i = 0; i < count; i++) {
      if (slots[i].at_min > 1439) continue;
      if (slots[i].app_id >= static_cast<uint8_t>(AppId::Count)) continue;
      g_cfg.app_sched[g_cfg.app_sched_count++] = slots[i];
    }
  }
  // 按 at_min 升序，方便时间表展示与解析
  for (uint8_t i = 0; i + 1 < g_cfg.app_sched_count; i++) {
    for (uint8_t j = i + 1; j < g_cfg.app_sched_count; j++) {
      if (g_cfg.app_sched[j].at_min < g_cfg.app_sched[i].at_min) {
        const AppSchedSlot tmp = g_cfg.app_sched[i];
        g_cfg.app_sched[i] = g_cfg.app_sched[j];
        g_cfg.app_sched[j] = tmp;
      }
    }
  }
  return saveAppConfig();
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
  qw["source"] = g_cfg.qweather_source;
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
  qw["now_refresh_en"] = g_cfg.weather_now_refresh_en;
  qw["now_refresh_sec"] = g_cfg.weather_now_refresh_sec;
  qw["week_refresh_en"] = g_cfg.weather_week_refresh_en;
  qw["week_refresh_sec"] = g_cfg.weather_week_refresh_sec;
  doc["idle_off_sec"] = g_cfg.idle_off_sec;
  doc["idle_off_on_power"] = g_cfg.idle_off_on_power;
  doc["album_show_header"] = g_cfg.album_show_header;
  doc["album_refresh_en"] = g_cfg.album_refresh_en;
  doc["album_refresh_sec"] = g_cfg.album_refresh_sec;
  doc["album_random"] = g_cfg.album_random;
  doc["todo_use_local"] = g_cfg.todo_use_local;
  doc["chess_random"] = g_cfg.chess_random;
  doc["chess_levels"] = g_cfg.chess_levels;
  doc["chess_refresh_en"] = g_cfg.chess_refresh_en;
  doc["chess_refresh_sec"] = g_cfg.chess_refresh_sec;
  doc["quote_random"] = g_cfg.quote_random;
  doc["quote_refresh_en"] = g_cfg.quote_refresh_en;
  doc["quote_refresh_sec"] = g_cfg.quote_refresh_sec;
  doc["week_start_monday"] = g_cfg.week_start_monday;
  doc["tz_offset_min"] = g_cfg.tz_offset_min;
  doc["ntp_interval_days"] = g_cfg.ntp_interval_days;
  doc["apps_enabled"] = g_cfg.apps_enabled;
  doc["apps_mask_ver"] = 3;  // Quote 引入；升级时默认打开新 App

  doc["quiet_en"] = g_cfg.quiet_en;
  JsonArray quiet = doc["quiet"].to<JsonArray>();
  for (uint8_t i = 0; i < g_cfg.quiet_count; i++) {
    JsonObject q = quiet.add<JsonObject>();
    q["start"] = g_cfg.quiet_slots[i].start_min;
    q["end"] = g_cfg.quiet_slots[i].end_min;
  }
  doc["app_sched_en"] = g_cfg.app_sched_en;
  JsonArray sched = doc["app_sched"].to<JsonArray>();
  for (uint8_t i = 0; i < g_cfg.app_sched_count; i++) {
    JsonObject s = sched.add<JsonObject>();
    s["at"] = g_cfg.app_sched[i].at_min;
    s["app"] = g_cfg.app_sched[i].app_id;
    s["view"] = g_cfg.app_sched[i].view;
  }

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
    {
      const int src = qw["source"] | (int)QW_SRC_PORTAL;
      g_cfg.qweather_source = (src == (int)QW_SRC_APIKEY) ? QW_SRC_APIKEY : QW_SRC_PORTAL;
    }
    strncpy(g_cfg.qweather_host, qw["host"] | "", sizeof(g_cfg.qweather_host) - 1);
    sanitizeQWeatherHost(g_cfg.qweather_host, sizeof(g_cfg.qweather_host));
    const char *key = qw["key"] | "";
    if (key[0] && !strstr(key, "********")) {
      strncpy(g_cfg.qweather_key, key, sizeof(g_cfg.qweather_key) - 1);
    }
    strncpy(g_cfg.qweather_location_id, qw["location_id"] | "", sizeof(g_cfg.qweather_location_id) - 1);
    strncpy(g_cfg.qweather_location_name, qw["location_name"] | "",
            sizeof(g_cfg.qweather_location_name) - 1);
    // 新：日/周分档；旧 refresh_* 迁到日天气，周天气默认 6h、同开关
    const bool has_now = qw["now_refresh_sec"].is<uint32_t>() || qw["now_refresh_sec"].is<int>() ||
                         qw["now_refresh_en"].is<bool>();
    const bool has_week = qw["week_refresh_sec"].is<uint32_t>() || qw["week_refresh_sec"].is<int>() ||
                          qw["week_refresh_en"].is<bool>();
    if (has_now || has_week) {
      if (qw["now_refresh_sec"].is<uint32_t>() || qw["now_refresh_sec"].is<int>()) {
        uint32_t sec = (uint32_t)(qw["now_refresh_sec"] | 0);
        g_cfg.weather_now_refresh_sec = clampRefreshSec(sec == 0 ? kRefreshDefaultSec : sec);
      } else {
        g_cfg.weather_now_refresh_sec = kRefreshDefaultSec;
      }
      g_cfg.weather_now_refresh_en = qw["now_refresh_en"] | false;
      if (qw["week_refresh_sec"].is<uint32_t>() || qw["week_refresh_sec"].is<int>()) {
        uint32_t sec = (uint32_t)(qw["week_refresh_sec"] | 0);
        g_cfg.weather_week_refresh_sec = clampRefreshSec(sec == 0 ? kWeatherWeekDefaultSec : sec);
      } else {
        g_cfg.weather_week_refresh_sec = kWeatherWeekDefaultSec;
      }
      g_cfg.weather_week_refresh_en = qw["week_refresh_en"] | false;
    } else if (qw["refresh_sec"].is<uint32_t>() || qw["refresh_sec"].is<int>()) {
      uint32_t sec = (uint32_t)(qw["refresh_sec"] | 0);
      bool en = false;
      if (sec == 0) {
        en = false;
        sec = kRefreshDefaultSec;
      } else {
        sec = clampRefreshSec(sec);
        en = qw["refresh_en"].is<bool>() ? qw["refresh_en"].as<bool>() : true;
      }
      g_cfg.weather_now_refresh_en = en;
      g_cfg.weather_now_refresh_sec = sec;
      g_cfg.weather_week_refresh_en = en;
      g_cfg.weather_week_refresh_sec = kWeatherWeekDefaultSec;
    } else {
      g_cfg.weather_now_refresh_sec = kRefreshDefaultSec;
      g_cfg.weather_now_refresh_en = qw["refresh_en"] | false;
      g_cfg.weather_week_refresh_sec = kWeatherWeekDefaultSec;
      g_cfg.weather_week_refresh_en = g_cfg.weather_now_refresh_en;
    }
  } else {
    g_cfg.weather_now_refresh_sec = kRefreshDefaultSec;
    g_cfg.weather_now_refresh_en = false;
    g_cfg.weather_week_refresh_sec = kWeatherWeekDefaultSec;
    g_cfg.weather_week_refresh_en = false;
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
  if (doc["album_refresh_en"].is<bool>()) {
    g_cfg.album_refresh_en = doc["album_refresh_en"].as<bool>();
  } else {
    g_cfg.album_refresh_en = false;
  }
  if (doc["album_refresh_sec"].is<uint32_t>() || doc["album_refresh_sec"].is<int>()) {
    uint32_t sec = (uint32_t)(doc["album_refresh_sec"] | 0);
    g_cfg.album_refresh_sec = clampRefreshSec(sec == 0 ? kRefreshDefaultSec : sec);
  } else {
    g_cfg.album_refresh_sec = kRefreshDefaultSec;
  }
  g_cfg.album_random = doc["album_random"] | false;
  g_cfg.todo_use_local = doc["todo_use_local"] | false;
  if (doc["chess_random"].is<bool>()) {
    g_cfg.chess_random = doc["chess_random"].as<bool>();
  } else {
    g_cfg.chess_random = false;
  }
  if (doc["chess_levels"].is<int>()) {
    g_cfg.chess_levels = (uint8_t)doc["chess_levels"].as<int>() & CHESS_LV_ALL;
    if (g_cfg.chess_levels == 0) g_cfg.chess_levels = CHESS_LV_ALL;
  } else {
    g_cfg.chess_levels = CHESS_LV_ALL;
  }
  if (doc["chess_refresh_en"].is<bool>()) {
    g_cfg.chess_refresh_en = doc["chess_refresh_en"].as<bool>();
  } else {
    g_cfg.chess_refresh_en = false;
  }
  if (doc["chess_refresh_sec"].is<uint32_t>() || doc["chess_refresh_sec"].is<int>()) {
    uint32_t sec = (uint32_t)(doc["chess_refresh_sec"] | 0);
    g_cfg.chess_refresh_sec = clampRefreshSec(sec == 0 ? kRefreshDefaultSec : sec);
  } else {
    g_cfg.chess_refresh_sec = kRefreshDefaultSec;
  }
  g_cfg.quote_random = doc["quote_random"] | false;
  g_cfg.quote_refresh_en = doc["quote_refresh_en"] | false;
  if (doc["quote_refresh_sec"].is<uint32_t>() || doc["quote_refresh_sec"].is<int>()) {
    uint32_t sec = (uint32_t)(doc["quote_refresh_sec"] | 0);
    g_cfg.quote_refresh_sec = clampRefreshSec(sec == 0 ? kRefreshDefaultSec : sec);
  } else {
    g_cfg.quote_refresh_sec = kRefreshDefaultSec;
  }

  g_cfg.quiet_en = doc["quiet_en"] | false;
  g_cfg.quiet_count = 0;
  memset(g_cfg.quiet_slots, 0, sizeof(g_cfg.quiet_slots));
  if (doc["quiet"].is<JsonArray>()) {
    for (JsonObject q : doc["quiet"].as<JsonArray>()) {
      if (g_cfg.quiet_count >= QUIET_SLOT_MAX) break;
      const int st = q["start"] | -1;
      const int en = q["end"] | -1;
      if (st < 0 || en < 0 || st > 1439 || en > 1439 || st == en) continue;
      g_cfg.quiet_slots[g_cfg.quiet_count].start_min = (uint16_t)st;
      g_cfg.quiet_slots[g_cfg.quiet_count].end_min = (uint16_t)en;
      g_cfg.quiet_count++;
    }
  }

  g_cfg.app_sched_en = doc["app_sched_en"] | false;
  g_cfg.app_sched_count = 0;
  memset(g_cfg.app_sched, 0, sizeof(g_cfg.app_sched));
  if (doc["app_sched"].is<JsonArray>()) {
    for (JsonObject s : doc["app_sched"].as<JsonArray>()) {
      if (g_cfg.app_sched_count >= APP_SCHED_MAX) break;
      const int at = s["at"] | -1;
      const int app = s["app"] | -1;
      const int view = s["view"] | 0;
      if (at < 0 || at > 1439) continue;
      if (app < 0 || app >= static_cast<int>(AppId::Count)) continue;
      g_cfg.app_sched[g_cfg.app_sched_count].at_min = (uint16_t)at;
      g_cfg.app_sched[g_cfg.app_sched_count].app_id = (uint8_t)app;
      g_cfg.app_sched[g_cfg.app_sched_count].view = (uint8_t)(view < 0 ? 0 : view);
      g_cfg.app_sched_count++;
    }
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
    // 旧配置无 apps_mask_ver：把新增 App 默认打开一次
    const int ver = doc["apps_mask_ver"].is<int>() ? doc["apps_mask_ver"].as<int>() : 0;
    if (ver < 2) {
      constexpr uint8_t kNewApps =
          (1u << static_cast<int>(AppId::Status)) | (1u << static_cast<int>(AppId::Chess));
      g_cfg.apps_enabled = (uint8_t)(g_cfg.apps_enabled | kNewApps);
    }
    if (ver < 3) {
      g_cfg.apps_enabled =
          (uint8_t)(g_cfg.apps_enabled | (1u << static_cast<int>(AppId::Quote)));
    }
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
