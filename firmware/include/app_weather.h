#pragma once

#include <Arduino.h>
#include "epd_ssd2683.h"

static constexpr int WEATHER_DAILY_MAX = 15;
static constexpr int WEATHER_HOURLY_MAX = 6;
static constexpr int WEATHER_GEO_MAX = 12;

enum class WeatherMode : uint8_t {
  Now = 0,    // 当天实况 + 未来 6 小时
  Hour12,     // 旧会话值：当作 Now，BOOT 不再露出
  Week7,      // 未来 7 天
  Week14,     // 旧会话值：当作 Week7
  Count,
};

struct WeatherNowData {
  bool ok = false;
  String message;
  String obs_time;
  String temp;
  String feels_like;
  String text;
  String icon;
  String wind_dir;
  String wind_scale;
  String humidity;
  String precip;
  String pressure;
  String vis;
  String cloud;
  String aqi;
  String aqi_category;
  String pm2p5;
};

struct WeatherHourlyItem {
  String fx_time;
  String temp;
  String text;
  String icon;
  String wind_dir;
  String wind_scale;
  String humidity;
  String precip;
  String pop;
};

struct WeatherHourlyData {
  bool ok = false;
  String message;
  int count = 0;
  WeatherHourlyItem hours[WEATHER_HOURLY_MAX];
};

struct WeatherDailyItem {
  String fx_date;
  String temp_max;
  String temp_min;
  String text_day;
  String icon_day;
  String text_night;
  String wind_dir_day;
  String wind_scale_day;
  String humidity;
  String precip;
  String uv_index;
};

struct WeatherDailyData {
  bool ok = false;
  String message;
  int count = 0;
  WeatherDailyItem days[WEATHER_DAILY_MAX];
};

struct WeatherGeoItem {
  String name;
  String id;
  String adm1;
  String adm2;
  String country;
};

struct WeatherGeoResult {
  bool ok = false;
  String message;
  int count = 0;
  WeatherGeoItem items[WEATHER_GEO_MAX];
};

inline WeatherMode weatherModeNormalize(WeatherMode m) {
  if (m == WeatherMode::Hour12) return WeatherMode::Now;
  if (m == WeatherMode::Week14) return WeatherMode::Week7;
  if (static_cast<uint8_t>(m) >= static_cast<uint8_t>(WeatherMode::Count)) return WeatherMode::Now;
  return m;
}

inline const char *weatherModeName(WeatherMode m) {
  switch (weatherModeNormalize(m)) {
    case WeatherMode::Now:
    case WeatherMode::Hour12:
      return "NOW";
    case WeatherMode::Week7:
    case WeatherMode::Week14:
      return "7D";
    default:
      return "?";
  }
}

inline WeatherMode weatherModeNext(WeatherMode m) {
  return weatherModeNormalize(m) == WeatherMode::Now ? WeatherMode::Week7 : WeatherMode::Now;
}

// GeoAPI city lookup (device proxies; needs 同 WiFi 站点).
bool weatherGeoLookup(const char *query, WeatherGeoResult &out);

// Fetch APIs (lazy per mode). location = LocationID from config.
bool weatherFetchNow(WeatherNowData &out);
bool weatherFetchHourly(const char *hours_path, WeatherHourlyData &out);  // "24h"
bool weatherFetchDaily(const char *days_path, WeatherDailyData &out);    // "7d" / "15d"

void weatherDrawNow(EpdSsd2683 &epd, const WeatherNowData &now, const WeatherHourlyData &hourly,
                    const char *status);
void weatherDrawForecast(EpdSsd2683 &epd, const WeatherDailyData &d, WeatherMode mode,
                         const char *status);

// Ensure wifi, fetch for mode, draw. Returns false on soft failure.
bool weatherShowMode(EpdSsd2683 &epd, WeatherMode mode);
