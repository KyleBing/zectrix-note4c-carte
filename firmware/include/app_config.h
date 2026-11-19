#pragma once

#include <Arduino.h>

static constexpr int WIFI_PROFILE_MAX = 5;
static constexpr int PORTAL_TOKEN_MAX = 1024;
static constexpr int PORTAL_URL_MAX = 128;
static constexpr int PORTAL_EMAIL_MAX = 80;
static constexpr int DIARY_ID_MAX = 24;

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

String appConfigToJson(bool mask_secrets = false);
bool appConfigFromJson(const char *json);

const AppConfig &getAppConfig();
bool hasSavedWifiConfig();
