#pragma once

#include <Arduino.h>

static constexpr int WIFI_PROFILE_MAX = 5;
static constexpr int PORTAL_TOKEN_MAX = 1024;
static constexpr int PORTAL_URL_MAX = 128;
static constexpr int PORTAL_EMAIL_MAX = 80;
static constexpr int DIARY_ID_MAX = 24;
static constexpr int QW_HOST_MAX = 80;
static constexpr int QW_KEY_MAX = 64;
static constexpr int QW_LOC_ID_MAX = 16;
static constexpr int QW_LOC_NAME_MAX = 48;

struct WifiProfile {
  char ssid[33];
  char password[65];
};

struct AppConfig {
  WifiProfile wifis[WIFI_PROFILE_MAX];
  int wifi_count;
  char wifi_active[33];
  // Mirrored active credentials for connect helpers
  char wifi_ssid[33];
  char wifi_password[65];

  char portal_base_url[PORTAL_URL_MAX];  // e.g. https://kylebing.cn/portal
  char portal_email[PORTAL_EMAIL_MAX];
  char portal_token[PORTAL_TOKEN_MAX];
  char diary_id[DIARY_ID_MAX];

  uint32_t idle_off_sec;  // 无操作自动关机秒数；0=关闭。Web 配置期间不关
  bool album_show_header; // 设备相册展示时是否叠标题栏；默认 true
  bool week_start_monday; // 日历一周起始：true=周一，false=周日；默认 true
  int16_t tz_offset_min;  // 时区相对 UTC 分钟数；默认 480（UTC+8）
  uint16_t ntp_interval_days; // NTP 周期天数；默认 30（复位后仍会校一次）
  uint8_t apps_enabled;   // bit i = AppId i 是否启用；默认全开

  // 天气：设备只存位置；和风 JWT/Key 在 Portal-go 服务端
  // host/key 字段保留兼容旧配置，设备端请求不再使用
  bool qweather_enabled;
  char qweather_host[QW_HOST_MAX];
  char qweather_key[QW_KEY_MAX];
  char qweather_location_id[QW_LOC_ID_MAX];
  char qweather_location_name[QW_LOC_NAME_MAX];

  bool loaded;
};

bool initAppStorage();
bool loadAppConfig();
bool saveAppConfig();

bool upsertAppConfigWifi(const char *ssid, const char *password, bool set_active = true);
bool removeAppConfigWifi(const char *ssid);
bool setAppConfigWifiActive(const char *ssid);

bool savePortalAuth(const char *email, const char *token);
bool savePortalToken(const char *token);
bool savePortalBaseUrl(const char *url);
bool saveDiaryId(const char *id);

bool saveQWeatherConfig(bool enabled, const char *host, const char *key, const char *loc_id,
                        const char *loc_name);
bool qweatherConfigured();
bool saveIdleOffSec(uint32_t sec);
bool saveAlbumShowHeader(bool show);
bool saveWeekStartMonday(bool monday);
bool saveTimezoneOffsetMin(int16_t minutes);
bool saveNtpIntervalDays(uint16_t days);
bool saveAppsEnabled(uint8_t mask);
bool setAppEnabled(uint8_t app_id, bool enabled);  // 至少保留一个

String appConfigToJson(bool mask_secrets = false);
bool appConfigFromJson(const char *json);

const AppConfig &getAppConfig();
bool hasSavedWifiConfig();

// Last App / weather mode / album index — survive reboot (BOOT/WDT 等)
void saveUiSession(uint8_t app_id, uint8_t weather_mode, int album_index);
bool loadUiSession(uint8_t &app_id, uint8_t &weather_mode, int &album_index);
