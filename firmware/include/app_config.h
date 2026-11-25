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
// 天气数据源：Portal 代理（JWT）/ 和风普通 API Key（非 JWT）
static constexpr uint8_t QW_SRC_PORTAL = 0;
static constexpr uint8_t QW_SRC_APIKEY = 1;
static constexpr int QUIET_SLOT_MAX = 6;
static constexpr int APP_SCHED_MAX = 12;

// 静默时段：当天分钟 [start, end)；若 start>end 则跨午夜
struct QuietSlot {
  uint16_t start_min;
  uint16_t end_min;
};

// 每日 App 切换点：到点切到 app_id；view 为界面（天气 0=实况 1=预报）
struct AppSchedSlot {
  uint16_t at_min;
  uint8_t app_id;
  uint8_t view;
};

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
  bool idle_off_on_power; // 连接电源（充电/充满）时是否仍执行空闲关机；默认 false
  bool album_show_header; // 设备相册展示时是否叠标题栏；默认 true
  bool album_refresh_en;  // 相册定时翻页 / 关机定时醒；默认 false
  uint32_t album_refresh_sec; // 相册翻页间隔秒；开启时至少 60；默认 3600
  bool album_random;      // 相册翻页乱序；默认 false
  bool week_start_monday; // 日历一周起始：true=周一，false=周日；默认 true
  int16_t tz_offset_min;  // 时区相对 UTC 分钟数；默认 480（UTC+8）
  uint16_t ntp_interval_days; // NTP 周期天数；默认 30（复位后仍会校一次）
  uint8_t apps_enabled;   // bit i = AppId i 是否启用；默认全开

  // 待办：true=用本机正文（LittleFS），false=拉 Portal 日记
  bool todo_use_local;

  // 天气：两种数据源可切换
  // QW_SRC_PORTAL：经 Portal 代理，用登录 JWT；和风密钥在服务器
  // QW_SRC_APIKEY：设备和风普通 API（Host + Key 查询参数，非 JWT）
  bool qweather_enabled;
  uint8_t qweather_source;  // QW_SRC_PORTAL / QW_SRC_APIKEY
  char qweather_host[QW_HOST_MAX];
  char qweather_key[QW_KEY_MAX];
  char qweather_location_id[QW_LOC_ID_MAX];
  char qweather_location_name[QW_LOC_NAME_MAX];
  bool weather_refresh_en;       // 天气定时刷新 / 关机定时醒；默认 false
  uint32_t weather_refresh_sec;  // 间隔秒；开启时至少 60；默认 3600

  // 国际象棋练习
  bool chess_random;             // false=顺序，true=乱序；默认 false
  uint8_t chess_levels;          // CHESS_LV_* 多选；0 视为全开；默认全开
  bool chess_refresh_en;         // 定时翻局 / 关机定时醒；默认 false
  uint32_t chess_refresh_sec;    // 间隔秒；开启时至少 60；默认 3600

  // 名言警句
  bool quote_random;             // 翻条顺序；默认 false
  bool quote_refresh_en;         // 定时翻条；默认 false
  uint32_t quote_refresh_sec;    // 间隔秒；默认 3600

  // 每日静默时段：期间不做定时刷新 / App 时间表切换 / RTC 醒刷
  bool quiet_en;
  uint8_t quiet_count;
  QuietSlot quiet_slots[QUIET_SLOT_MAX];

  // 每日 App 切换时间表
  bool app_sched_en;
  uint8_t app_sched_count;
  AppSchedSlot app_sched[APP_SCHED_MAX];

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

bool saveQWeatherConfig(bool enabled, uint8_t source, const char *host, const char *key,
                        const char *loc_id, const char *loc_name);
bool qweatherConfigured();
bool qweatherSourceIsApiKey();
bool saveWeatherRefreshEn(bool en);
// 间隔钳到 [60, 365 天]；开关关闭时仍可改间隔
bool saveWeatherRefreshSec(uint32_t sec);
uint32_t weatherRefreshSecFromDhm(uint32_t days, uint32_t hours, uint32_t minutes);
void weatherRefreshToDhm(uint32_t sec, uint32_t &days, uint32_t &hours, uint32_t &minutes);
// 上次成功刷新的 unix 秒（NVS）；0 = 未知
uint32_t loadWeatherLastUnix();
void saveWeatherLastUnix(uint32_t unix_sec);
bool weatherAutoRefreshDue();  // 开关开 + 间隔到期（需有效系统时间）

bool saveAlbumRefreshEn(bool en);
bool saveAlbumRefreshSec(uint32_t sec);
uint32_t loadAlbumLastUnix();
void saveAlbumLastUnix(uint32_t unix_sec);
bool albumAutoRefreshDue();

bool saveChessSettings(bool random_order, uint8_t levels, bool refresh_en, uint32_t refresh_sec);
uint32_t loadChessLastUnix();
void saveChessLastUnix(uint32_t unix_sec);
bool chessAutoRefreshDue();

bool saveQuoteSettings(bool random_order, bool refresh_en, uint32_t refresh_sec);
uint32_t loadQuoteLastUnix();
void saveQuoteLastUnix(uint32_t unix_sec);

bool saveAlbumRandom(bool random_order);
bool saveTodoUseLocal(bool use_local);
bool saveQuietSchedule(bool en, const QuietSlot *slots, uint8_t count);
bool saveAppSchedule(bool en, const AppSchedSlot *slots, uint8_t count);

bool saveIdleOffSec(uint32_t sec);
bool saveIdleOffOnPower(bool on);
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

// 刷机首屏：已展示过的 APP_BUILD；未见过则返回 -1
int loadSplashBuild();
void saveSplashBuild(int build);

// 定时深睡标记：睡前写入；仅「下键交互开机」清除。用于唤醒原因丢失时仍走刷完再睡。
bool loadScheduledSleep();
void saveScheduledSleep(bool on);
