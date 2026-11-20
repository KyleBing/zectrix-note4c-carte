#include "app_weather.h"
#include "app_config.h"
#include "app_font.h"
#include "app_wifi.h"
#include "app_header.h"
#include "app_theme.h"
#include "app_footer.h"
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

static void captureRenewedToken(HTTPClient &http) {
  const String renewed = http.header(kRenewHeader);
  if (renewed.length() > 20) {
    Serial.println("[Weather] renew token via X-Access-Token");
    savePortalToken(renewed.c_str());
  }
}

static bool ensureWifi(String &err) {
  if (wifiIsStaConnected()) return true;
  if (!hasSavedWifiConfig()) {
    err = "No WiFi. hold UP = config web";
    return false;
  }
  if (!wifiConnectSaved(18000)) {
    err = "WiFi connect failed";
    return false;
  }
  return true;
}

// GET portal /weather/... with Bearer; out_data = response.data object as JSON string subtree via doc
static bool portalWeatherGet(const char *path_and_query, JsonDocument &doc, String &err) {
  err = "";
  const AppConfig &cfg = getAppConfig();
  if (!cfg.portal_base_url[0]) {
    err = "portal base_url empty";
    return false;
  }
  if (!cfg.portal_token[0]) {
    err = "not logged in. Open web /auth";
    return false;
  }

  const String url = joinUrl(cfg.portal_base_url, path_and_query);
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15000);
  HTTPClient http;
  if (!http.begin(client, url)) {
    err = "http begin failed";
    return false;
  }
  http.addHeader("Authorization", String("Bearer ") + cfg.portal_token);
  http.addHeader("Accept-Encoding", "identity");
  const char *headerKeys[] = {kRenewHeader};
  http.collectHeaders(headerKeys, 1);

  const int code = http.GET();
  const String body = http.getString();
  captureRenewedToken(http);
  http.end();

  if (code <= 0) {
    err = String("HTTP error ") + code;
    return false;
  }
  if (code == 401) {
    err = "unauthorized — re-login in web /auth";
    return false;
  }

  if (deserializeJson(doc, body)) {
    err = "bad JSON";
    return false;
  }
  if (!(doc["success"] | false)) {
    err = doc["message"] | "portal weather failed";
    return false;
  }
  return true;
}

bool weatherGeoLookup(const char *query, WeatherGeoResult &out) {
  out = WeatherGeoResult{};
  if (!query || !query[0]) {
    out.message = "query empty";
    return false;
  }
  String err;
  if (!ensureWifi(err)) {
    out.message = err;
    return false;
  }

  String path = "/weather/geo?lang=zh&number=10&location=";
  for (const unsigned char *p = (const unsigned char *)query; *p; ++p) {
    const unsigned char c = *p;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
        c == '_' || c == '.' || c == '~') {
      path += (char)c;
    } else if (c == ' ') {
      path += "%20";
    } else {
      char hex[4];
      snprintf(hex, sizeof(hex), "%%%02X", c);
      path += hex;
    }
  }

  JsonDocument doc;
  if (!portalWeatherGet(path.c_str(), doc, err)) {
    out.message = err;
    return false;
  }

  JsonObject data = doc["data"].as<JsonObject>();
  const char *code = data["code"] | "";
  if (strcmp(code, "200") != 0) {
    out.message = String("geo code ") + code;
    return false;
  }
  JsonArray arr = data["location"].as<JsonArray>();
  if (arr.isNull() || arr.size() == 0) {
    out.message = "no locations";
    return false;
  }
  for (JsonObject loc : arr) {
    if (out.count >= WEATHER_GEO_MAX) break;
    WeatherGeoItem &it = out.items[out.count++];
    it.name = loc["name"] | "";
    it.id = loc["id"] | "";
    it.adm1 = loc["adm1"] | "";
    it.adm2 = loc["adm2"] | "";
    it.country = loc["country"] | "";
  }
  out.ok = out.count > 0;
  if (!out.ok) out.message = "empty list";
  return out.ok;
}

bool weatherFetchNow(WeatherNowData &out) {
  out = WeatherNowData{};
  if (!qweatherConfigured()) {
    out.message = "Weather not ready. Login + set location in web /weather";
    return false;
  }
  String err;
  if (!ensureWifi(err)) {
    out.message = err;
    return false;
  }

  const AppConfig &cfg = getAppConfig();
  char path[80];
  snprintf(path, sizeof(path), "/weather/now?location=%s", cfg.qweather_location_id);

  JsonDocument doc;
  if (!portalWeatherGet(path, doc, err)) {
    out.message = err;
    return false;
  }
  JsonObject data = doc["data"].as<JsonObject>();
  const char *code = data["code"] | "";
  if (strcmp(code, "200") != 0) {
    out.message = String("now code ") + code;
    return false;
  }
  JsonObject now = data["now"].as<JsonObject>();
  if (now.isNull()) {
    out.message = "no now";
    return false;
  }
  out.obs_time = now["obsTime"] | "";
  out.temp = now["temp"] | "";
  out.feels_like = now["feelsLike"] | "";
  out.text = now["text"] | "";
  out.icon = now["icon"] | "";
  out.wind_dir = now["windDir"] | "";
  out.wind_scale = now["windScale"] | "";
  out.humidity = now["humidity"] | "";
  out.precip = now["precip"] | "";
  out.pressure = now["pressure"] | "";
  out.vis = now["vis"] | "";
  out.cloud = now["cloud"] | "";
  out.ok = true;
  return true;
}

bool weatherFetchHourly(const char *hours_path, WeatherHourlyData &out) {
  out = WeatherHourlyData{};
  if (!qweatherConfigured()) {
    out.message = "Weather not ready. Login + set location in web /weather";
    return false;
  }
  if (!hours_path || !hours_path[0]) {
    out.message = "hours empty";
    return false;
  }
  String err;
  if (!ensureWifi(err)) {
    out.message = err;
    return false;
  }

  const AppConfig &cfg = getAppConfig();
  char path[96];
  snprintf(path, sizeof(path), "/weather/hourly?location=%s&hours=%s", cfg.qweather_location_id,
           hours_path);

  JsonDocument doc;
  if (!portalWeatherGet(path, doc, err)) {
    out.message = err;
    return false;
  }
  JsonObject data = doc["data"].as<JsonObject>();
  const char *code = data["code"] | "";
  if (strcmp(code, "200") != 0) {
    out.message = String("hourly code ") + code;
    return false;
  }
  JsonArray hourly = data["hourly"].as<JsonArray>();
  if (hourly.isNull()) {
    out.message = "no hourly";
    return false;
  }
  for (JsonObject h : hourly) {
    if (out.count >= WEATHER_HOURLY_MAX) break;
    WeatherHourlyItem &it = out.hours[out.count++];
    it.fx_time = h["fxTime"] | "";
    it.temp = h["temp"] | "";
    it.text = h["text"] | "";
    it.wind_dir = h["windDir"] | "";
    it.wind_scale = h["windScale"] | "";
    it.humidity = h["humidity"] | "";
    it.precip = h["precip"] | "";
    it.pop = h["pop"] | "";
  }
  out.ok = out.count > 0;
  if (!out.ok) out.message = "empty hourly";
  return out.ok;
}

bool weatherFetchDaily(const char *days_path, WeatherDailyData &out) {
  out = WeatherDailyData{};
  if (!qweatherConfigured()) {
    out.message = "Weather not ready. Login + set location in web /weather";
    return false;
  }
  if (!days_path || !days_path[0]) {
    out.message = "days empty";
    return false;
  }
  String err;
  if (!ensureWifi(err)) {
    out.message = err;
    return false;
  }

  const AppConfig &cfg = getAppConfig();
  char path[96];
  snprintf(path, sizeof(path), "/weather/daily?location=%s&days=%s", cfg.qweather_location_id, days_path);

  JsonDocument doc;
  if (!portalWeatherGet(path, doc, err)) {
    out.message = err;
    return false;
  }
  JsonObject data = doc["data"].as<JsonObject>();
  const char *code = data["code"] | "";
  if (strcmp(code, "200") != 0) {
    out.message = String("daily code ") + code;
    return false;
  }
  JsonArray daily = data["daily"].as<JsonArray>();
  if (daily.isNull()) {
    out.message = "no daily";
    return false;
  }
  for (JsonObject d : daily) {
    if (out.count >= WEATHER_DAILY_MAX) break;
    WeatherDailyItem &it = out.days[out.count++];
    it.fx_date = d["fxDate"] | "";
    it.temp_max = d["tempMax"] | "";
    it.temp_min = d["tempMin"] | "";
    it.text_day = d["textDay"] | "";
    it.text_night = d["textNight"] | "";
    it.wind_dir_day = d["windDirDay"] | "";
    it.wind_scale_day = d["windScaleDay"] | "";
    it.humidity = d["humidity"] | "";
    it.precip = d["precip"] | "";
    it.uv_index = d["uvIndex"] | "";
  }
  out.ok = out.count > 0;
  if (!out.ok) out.message = "empty daily";
  return out.ok;
}

// "2024-11-19" or with time → weekday 0=Sun..6=Sat (Sakamoto)
static int weekdayFromYmd(int y, int m, int d) {
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

static const char *weekdayCn(int wd) {
  static const char *k[] = {"日", "一", "二", "三", "四", "五", "六"};
  if (wd < 0 || wd > 6) return "?";
  return k[wd];
}

// Parse leading YYYY-MM-DD from qweather strings.
static bool parseYmd(const String &s, int &y, int &m, int &d) {
  if (s.length() < 10) return false;
  if (s[4] != '-' || s[7] != '-') return false;
  y = s.substring(0, 4).toInt();
  m = s.substring(5, 7).toInt();
  d = s.substring(8, 10).toInt();
  return y >= 2000 && m >= 1 && m <= 12 && d >= 1 && d <= 31;
}

// Daily: "11-19 三"
static void formatDailyLabel(const String &fx_date, char *out, size_t n) {
  int y = 0, m = 0, d = 0;
  if (parseYmd(fx_date, y, m, d)) {
    snprintf(out, n, "%02d-%02d %s", m, d, weekdayCn(weekdayFromYmd(y, m, d)));
  } else if (fx_date.length() >= 10) {
    snprintf(out, n, "%c%c-%c%c", fx_date[5], fx_date[6], fx_date[8], fx_date[9]);
  } else {
    snprintf(out, n, "%s", fx_date.c_str());
  }
}

// Hourly fxTime "2024-11-19T15:00+08:00" → "11-19 15:00"
static void formatHourlyLabel(const String &fx_time, char *out, size_t n) {
  int y = 0, m = 0, d = 0;
  const int tpos = fx_time.indexOf('T');
  if (parseYmd(fx_time, y, m, d) && tpos > 0 && fx_time.length() >= (size_t)(tpos + 6)) {
    snprintf(out, n, "%02d-%02d %c%c:%c%c", m, d, fx_time[tpos + 1], fx_time[tpos + 2],
             fx_time[tpos + 4], fx_time[tpos + 5]);
  } else if (tpos > 0 && fx_time.length() >= (size_t)(tpos + 6)) {
    snprintf(out, n, "%c%c:%c%c", fx_time[tpos + 1], fx_time[tpos + 2], fx_time[tpos + 4],
             fx_time[tpos + 5]);
  } else {
    snprintf(out, n, "%s", fx_time.c_str());
  }
}

// obsTime same ISO family → "11-19 15:04"
static void formatObsLabel(const String &obs, char *out, size_t n) {
  formatHourlyLabel(obs, out, n);
}

static void drawWeatherHeader(EpdSsd2683 &epd, WeatherMode mode, const char *status) {
  // title/location may include CJK — must use HZK font, not 6x8 drawText (else ???)
  const int bat_left = headerDrawAppBar(epd, AppId::Weather, nullptr);
  const AppConfig &cfg = getAppConfig();
  char head[96];
  if (cfg.qweather_location_name[0]) {
    snprintf(head, sizeof(head), "天气 %s · %s", weatherModeName(mode), cfg.qweather_location_name);
  } else {
    snprintf(head, sizeof(head), "天气 %s", weatherModeName(mode));
  }
  const int title_max = bat_left - Theme::MarginX - 40;
  if (title_max > 40) {
    fontDrawTextWrapped(epd, Theme::MarginX, 3, title_max, Theme::HeaderH, head, Theme::HeaderFg,
                        Theme::HeaderBg);
  }
  if (status && status[0]) {
    const int sw = (int)strlen(status) * 6;
    const int sx = bat_left - 8 - sw;
    if (sx > Theme::MarginX + 24) {
      epd.drawText(sx, 7, status, Theme::HeaderAccent, Theme::HeaderBg);
    }
  }
}

static void drawWeatherFooter(EpdSsd2683 &epd) {
  footerDraw(epd, "mode");
}

static void drawErrorPage(EpdSsd2683 &epd, WeatherMode mode, const char *msg, const char *status) {
  epd.clear(Theme::Bg);
  drawWeatherHeader(epd, mode, status);
  fontDrawTextWrapped(epd, Theme::MarginX, 36, 384, Theme::FooterY - 4,
                      msg ? msg : "(error)", Theme::Fg);
  drawWeatherFooter(epd);
  epd.display();
}

void weatherDrawNow(EpdSsd2683 &epd, const WeatherNowData &d, const char *status) {
  epd.clear(Theme::Bg);
  drawWeatherHeader(epd, WeatherMode::Now, status);
  if (!d.ok) {
    fontDrawTextWrapped(epd, Theme::MarginX, 36, 384, Theme::FooterY - 4,
                        d.message.length() ? d.message.c_str() : "(error)", Theme::Fg);
  } else {
    int y = 32;
    char line[112];
    char when[24];
    formatObsLabel(d.obs_time, when, sizeof(when));
    snprintf(line, sizeof(line), "%s  %sC  体感%sC", d.text.c_str(), d.temp.c_str(),
             d.feels_like.c_str());
    y = fontDrawTextWrapped(epd, Theme::MarginX, y, 384, Theme::FooterY - 4, line, Theme::Alert);
    y += 6;
    snprintf(line, sizeof(line), "风 %s %s级", d.wind_dir.c_str(), d.wind_scale.c_str());
    y = fontDrawTextWrapped(epd, Theme::MarginX, y, 384, Theme::FooterY - 4, line, Theme::Fg);
    snprintf(line, sizeof(line), "湿度 %s%%  降水 %smm", d.humidity.c_str(), d.precip.c_str());
    y = fontDrawTextWrapped(epd, Theme::MarginX, y, 384, Theme::FooterY - 4, line, Theme::Fg);
    snprintf(line, sizeof(line), "气压 %shPa  能见度 %skm", d.pressure.c_str(), d.vis.c_str());
    y = fontDrawTextWrapped(epd, Theme::MarginX, y, 384, Theme::FooterY - 4, line, Theme::Fg);
    if (when[0]) {
      y += 6;
      snprintf(line, sizeof(line), "观测 %s", when);
      fontDrawTextWrapped(epd, Theme::MarginX, y, 384, Theme::FooterY - 4, line, Theme::Accent);
    }
  }
  drawWeatherFooter(epd);
  epd.display();
}

void weatherDrawHourly(EpdSsd2683 &epd, const WeatherHourlyData &d, const char *status) {
  epd.clear(Theme::Bg);
  drawWeatherHeader(epd, WeatherMode::Hour12, status);
  if (!d.ok || d.count <= 0) {
    fontDrawTextWrapped(epd, Theme::MarginX, 36, 384, Theme::FooterY - 4,
                        d.message.length() ? d.message.c_str() : "(no hourly)", Theme::Fg);
  } else {
    // One row per hour: "11-19 15:00  12C  多云  东北风3"
    int y = 26;
    char label[20];
    char line[128];
    for (int i = 0; i < d.count; i++) {
      if (y + fontLineHeight() > Theme::FooterY - 4) {
        fontDrawTextWrapped(epd, Theme::MarginX, y, 384, Theme::FooterY - 4, "…", Theme::Alert);
        break;
      }
      const WeatherHourlyItem &it = d.hours[i];
      formatHourlyLabel(it.fx_time, label, sizeof(label));
      if (it.wind_dir.length() || it.wind_scale.length()) {
        snprintf(line, sizeof(line), "%s  %sC  %s  %s%s", label, it.temp.c_str(), it.text.c_str(),
                 it.wind_dir.c_str(), it.wind_scale.c_str());
      } else {
        snprintf(line, sizeof(line), "%s  %sC  %s", label, it.temp.c_str(), it.text.c_str());
      }
      y = fontDrawTextWrapped(epd, Theme::MarginX, y, 384, Theme::FooterY - 4, line, Theme::Fg);
      y += 1;
    }
  }
  drawWeatherFooter(epd);
  epd.display();
}

void weatherDrawForecast(EpdSsd2683 &epd, const WeatherDailyData &d, WeatherMode mode,
                         const char *status) {
  epd.clear(Theme::Bg);
  drawWeatherHeader(epd, mode, status);
  if (!d.ok || d.count <= 0) {
    fontDrawTextWrapped(epd, Theme::MarginX, 36, 384, Theme::FooterY - 4,
                        d.message.length() ? d.message.c_str() : "(error)", Theme::Fg);
  } else {
    // One row per day. 14D uses smaller glyphs so 14 rows fit.
    const bool compact = (mode == WeatherMode::Week14) || (d.count > 8);
    int y = 26;
    char label[24];
    char line[128];
    for (int i = 0; i < d.count; i++) {
      const int row_h = compact ? fontLineHeightSmall() : fontLineHeight();
      if (y + row_h > Theme::FooterY - 4) {
        if (compact) {
          fontDrawTextWrappedSmall(epd, Theme::MarginX, y, 384, Theme::FooterY - 4, "…",
                                   Theme::Alert);
        } else {
          fontDrawTextWrapped(epd, Theme::MarginX, y, 384, Theme::FooterY - 4, "…", Theme::Alert);
        }
        break;
      }
      const WeatherDailyItem &it = d.days[i];
      formatDailyLabel(it.fx_date, label, sizeof(label));
      // "11-19 三  3~12C  晴 / 多云"
      if (it.text_night.length() && it.text_night != it.text_day) {
        snprintf(line, sizeof(line), "%s  %s~%sC  %s/%s", label, it.temp_min.c_str(),
                 it.temp_max.c_str(), it.text_day.c_str(), it.text_night.c_str());
      } else {
        snprintf(line, sizeof(line), "%s  %s~%sC  %s", label, it.temp_min.c_str(),
                 it.temp_max.c_str(), it.text_day.c_str());
      }
      if (compact) {
        y = fontDrawTextWrappedSmall(epd, Theme::MarginX, y, 384, Theme::FooterY - 4, line,
                                     Theme::Fg);
      } else {
        y = fontDrawTextWrapped(epd, Theme::MarginX, y, 384, Theme::FooterY - 4, line, Theme::Fg);
      }
      y += 1;
    }
  }
  drawWeatherFooter(epd);
  epd.display();
}

bool weatherShowMode(EpdSsd2683 &epd, WeatherMode mode) {
  if (!epd.ok()) return false;
  if (!qweatherConfigured()) {
    drawErrorPage(epd, mode,
                  "Need portal login + location.\nWeb: /auth then /weather\n(server holds QWeather keys)",
                  "cfg");
    return true;  // UI 已按 mode 刷新；BOOT 切换应看到标题变化，勿 ErrorBurst
  }

  switch (mode) {
    case WeatherMode::Now: {
      WeatherNowData d;
      const bool ok = weatherFetchNow(d);
      weatherDrawNow(epd, d, ok ? "OK" : "err");
      return true;
    }
    case WeatherMode::Hour12: {
      WeatherHourlyData d;
      const bool ok = weatherFetchHourly("24h", d);  // API min 24h; UI shows first 12
      weatherDrawHourly(epd, d, ok ? "OK" : "err");
      return true;
    }
    case WeatherMode::Week7: {
      WeatherDailyData d;
      const bool ok = weatherFetchDaily("7d", d);
      weatherDrawForecast(epd, d, mode, ok ? "OK" : "err");
      return true;
    }
    case WeatherMode::Week14: {
      WeatherDailyData d;
      const bool ok = weatherFetchDaily("15d", d);
      if (ok && d.count > 14) d.count = 14;  // show 14 days
      weatherDrawForecast(epd, d, mode, ok ? "OK" : "err");
      return true;
    }
    default:
      return false;
  }
}
