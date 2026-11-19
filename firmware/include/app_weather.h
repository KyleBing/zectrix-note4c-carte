#pragma once

#include <Arduino.h>
#include "epd_ssd2683.h"

static constexpr int WEATHER_DAILY_MAX = 15;
static constexpr int WEATHER_HOURLY_MAX = 12;
static constexpr int WEATHER_GEO_MAX = 12;

enum class WeatherMode : uint8_t {
  Now = 0,    // 当天此时（实况）
  Hour12,     // 未来 12 小时
  Week7,      // 未来 7 天
  Week14,     // 未来 14 天（和风 15d，屏上取前 14）
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
};

struct WeatherHourlyItem {
  String fx_time;
  String temp;
  String text;
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

inline const char *weatherModeName(WeatherMode m) {
  switch (m) {
    case WeatherMode::Now:
      return "NOW";
    case WeatherMode::Hour12:
      return "12H";
    case WeatherMode::Week7:
      return "7D";
    case WeatherMode::Week14:
      return "14D";
    default:
      return "?";
  }
}

inline WeatherMode weatherModeNext(WeatherMode m) {
  const int n = static_cast<int>(WeatherMode::Count);
  return static_cast<WeatherMode>((static_cast<int>(m) + 1) % n);
}

// GeoAPI city lookup (device proxies; needs STA).
bool weatherGeoLookup(const char *query, WeatherGeoResult &out);

// Fetch APIs (lazy per mode). location = LocationID from config.
bool weatherFetchNow(WeatherNowData &out);
bool weatherFetchHourly(const char *hours_path, WeatherHourlyData &out);  // "24h"
bool weatherFetchDaily(const char *days_path, WeatherDailyData &out);    // "7d" / "15d"

void weatherDrawNow(EpdSsd2683 &epd, const WeatherNowData &d, const char *status);
void weatherDrawHourly(EpdSsd2683 &epd, const WeatherHourlyData &d, const char *status);
void weatherDrawForecast(EpdSsd2683 &epd, const WeatherDailyData &d, WeatherMode mode,
                         const char *status);

// Ensure wifi, fetch for mode, draw. Returns false on soft failure.
bool weatherShowMode(EpdSsd2683 &epd, WeatherMode mode);
