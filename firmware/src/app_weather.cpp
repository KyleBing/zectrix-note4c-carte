#include "app_weather.h"
#include "app_config.h"
#include "app_font.h"
#include "app_wifi.h"
#include "app_theme.h"
#include "app_errorlog.h"
#include "app_power.h"
#include "app_led.h"
#include "app_gfx_icon.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <cstdio>
#include <cstring>
#include <memory>

static const char *kRenewHeader = "X-Access-Token";
static constexpr size_t kWeatherJsonCap = 48 * 1024;

// 预报结构体含大量 String，放静态区，避免和 WiFiClientSecure 叠在 loop 栈上撑爆复位。
static WeatherNowData g_now_buf;
static WeatherHourlyData g_hourly_buf;
static WeatherDailyData g_daily_buf;

static void weatherPump() {
  powerPoll(false);
  ledUpdate();
}

// HTTPS 读 body 时喂灯/看门狗，并截断过大 JSON，避免任务看门狗把板子复位（灯闪几下、屏不更新）。
class WeatherBodyStream : public Stream {
 public:
  WeatherBodyStream(Stream &inner, size_t cap) : inner_(inner), cap_(cap) {}
  int available() override { return inner_.available(); }
  int peek() override { return inner_.peek(); }
  void flush() override { inner_.flush(); }
  size_t write(uint8_t) override { return 0; }
  int read() override {
    if (n_ >= cap_) {
      truncated_ = true;
      return -1;
    }
    const int c = inner_.read();
    if (c >= 0) n_++;
    const uint32_t now = millis();
    if (now - last_pump_ >= 200) {
      last_pump_ = now;
      weatherPump();
    }
    return c;
  }
  bool truncated() const { return truncated_; }
  size_t bytes() const { return n_; }

 private:
  Stream &inner_;
  size_t cap_;
  size_t n_ = 0;
  uint32_t last_pump_ = 0;
  bool truncated_ = false;
};

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

// QWeather / Portal 字段多为字符串，偶发数字；统一成可显示的 String。
static String jsonField(JsonVariantConst v) {
  if (v.isNull()) return "";
  if (v.is<const char *>()) {
    const char *s = v.as<const char *>();
    return s ? String(s) : String();
  }
  if (v.is<bool>()) return v.as<bool>() ? String("1") : String("0");
  if (v.is<long>() || v.is<int>() || v.is<unsigned long>() || v.is<unsigned int>()) {
    return String(v.as<long>());
  }
  if (v.is<double>() || v.is<float>()) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%g", v.as<double>());
    return String(buf);
  }
  return v.as<String>();
}

// Portal 透传和风 code，可能是 "200" 或 200。
static bool qweatherCodeOk(JsonVariantConst code, String &got) {
  got = jsonField(code);
  return got == "200";
}

static JsonObject weatherDataObj(JsonDocument &doc) {
  JsonObject data = doc["data"].as<JsonObject>();
  if (!data.isNull()) return data;
  return doc.as<JsonObject>();
}

static void filterPortalMeta(JsonDocument &filter) {
  filter["success"] = true;
  filter["ok"] = true;
  filter["message"] = true;
  filter["msg"] = true;
  filter["error"] = true;
  filter["code"] = true;
  filter["data"]["code"] = true;
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

// GET portal /weather/... with Bearer. Stream+filter，避免整包 body 再拷进 JsonDocument。
static bool portalWeatherGet(const char *path_and_query, JsonDocument &doc, String &err,
                             const JsonDocument *filter = nullptr) {
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
  Serial.printf("[Weather] GET %s heap=%u\n", path_and_query, (unsigned)ESP.getFreeHeap());

  std::unique_ptr<WiFiClientSecure> client(new WiFiClientSecure());
  if (!client) {
    err = "client alloc failed";
    return false;
  }
  client->setInsecure();
  client->setTimeout(12000);
  client->setHandshakeTimeout(12);
  HTTPClient http;
  if (!http.begin(*client, url)) {
    err = "http begin failed";
    return false;
  }
  http.setTimeout(15000);
  http.setConnectTimeout(8000);
  http.useHTTP10(true);
  http.addHeader("Authorization", String("Bearer ") + cfg.portal_token);
  http.addHeader("Accept-Encoding", "identity");
  const char *headerKeys[] = {kRenewHeader};
  http.collectHeaders(headerKeys, 1);

  DeserializationError jerr = DeserializationError::Ok;
  int code = 0;
  int size = -1;
  bool truncated = false;
  size_t got = 0;
  {
    WdtLongOpGuard wdt_off;  // TLS busy-spin 会饿死本核 idle WDT；仅关 loop WDT 不够
    code = http.GET();
    if (code <= 0) {
      http.end();
      err = String("HTTP ") + code + " " + path_and_query;
      return false;
    }
    if (code == 401) {
      captureRenewedToken(http);
      http.end();
      err = String("401 未授权 ") + path_and_query + " — 请在 Web /auth 重新登录";
      return false;
    }
    if (code < 200 || code >= 300) {
      http.end();
      err = String("HTTP ") + code + " " + path_and_query;
      if (code == 404) err += "（Portal 无此路由，确认已部署天气 hourly）";
      Serial.printf("[Weather] %s\n", err.c_str());
      return false;
    }
    size = http.getSize();
    Serial.printf("[Weather] HTTP %d size=%d\n", code, size);
    if (size > (int)kWeatherJsonCap) {
      http.end();
      err = String("payload too large size=") + size + " " + path_and_query;
      return false;
    }
    WeatherBodyStream body(http.getStream(), kWeatherJsonCap);
    jerr = filter ? deserializeJson(doc, body, DeserializationOption::Filter(*filter))
                  : deserializeJson(doc, body);
    truncated = body.truncated();
    got = body.bytes();
    captureRenewedToken(http);
    http.end();
  }

  if (truncated) {
    err = String("JSON truncated >") + (int)kWeatherJsonCap + " " + path_and_query;
    Serial.printf("[Weather] %s\n", err.c_str());
    return false;
  }
  if (jerr) {
    err = String("JSON ") + jerr.c_str() + " size=" + size + " got=" + (int)got + " " + path_and_query;
    Serial.printf("[Weather] %s\n", err.c_str());
    return false;
  }
  const bool wrapped_ok = (doc["success"] | false) || (doc["ok"] | false);
  String qcode;
  const bool q_ok =
      qweatherCodeOk(doc["data"]["code"], qcode) || qweatherCodeOk(doc["code"], qcode);
  if (!wrapped_ok && !q_ok) {
    String m = jsonField(doc["message"]);
    if (!m.length()) m = jsonField(doc["msg"]);
    if (!m.length()) m = jsonField(doc["error"]);
    if (!m.length()) m = "portal weather failed";
    err = m + " | " + path_and_query;
    if (qcode.length()) err += String(" qcode=") + qcode;
    err += String(" HTTP ") + code;
    Serial.printf("[Weather] portal fail: %s heap=%u\n", err.c_str(), (unsigned)ESP.getFreeHeap());
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

  String path = "/geo/v2/city/lookup?lang=zh&number=10&range=cn&location=";
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
  if (data.isNull()) {
    out.message = "no data";
    return false;
  }
  String code;
  if (!qweatherCodeOk(data["code"], code)) {
    out.message = String("geo code ") + (code.length() ? code : "?");
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
  snprintf(path, sizeof(path), "/v7/weather/now?location=%s&lang=zh", cfg.qweather_location_id);

  JsonDocument filter;
  filterPortalMeta(filter);
  auto fillNow = [](JsonObject n0) {
    n0["obsTime"] = true;
    n0["temp"] = true;
    n0["feelsLike"] = true;
    n0["text"] = true;
    n0["icon"] = true;
    n0["windDir"] = true;
    n0["windScale"] = true;
    n0["humidity"] = true;
    n0["precip"] = true;
    n0["pressure"] = true;
    n0["vis"] = true;
    n0["cloud"] = true;
  };
  fillNow(filter["data"]["now"].to<JsonObject>());
  fillNow(filter["now"].to<JsonObject>());

  JsonDocument doc;
  if (!portalWeatherGet(path, doc, err, &filter)) {
    out.message = err;
    return false;
  }
  JsonObject data = weatherDataObj(doc);
  if (data.isNull()) {
    out.message = "no data";
    return false;
  }
  String code;
  if (!qweatherCodeOk(data["code"], code)) {
    out.message = String("now code ") + (code.length() ? code : "?");
    return false;
  }
  JsonObject now = data["now"].as<JsonObject>();
  if (now.isNull()) {
    out.message = "no now";
    return false;
  }
  out.obs_time = jsonField(now["obsTime"]);
  out.temp = jsonField(now["temp"]);
  out.feels_like = jsonField(now["feelsLike"]);
  out.text = jsonField(now["text"]);
  out.icon = jsonField(now["icon"]);
  out.wind_dir = jsonField(now["windDir"]);
  out.wind_scale = jsonField(now["windScale"]);
  out.humidity = jsonField(now["humidity"]);
  out.precip = jsonField(now["precip"]);
  out.pressure = jsonField(now["pressure"]);
  out.vis = jsonField(now["vis"]);
  out.cloud = jsonField(now["cloud"]);
  out.ok = true;

  snprintf(path, sizeof(path), "/v7/air/now?location=%s&lang=zh", cfg.qweather_location_id);
  JsonDocument air_filter;
  filterPortalMeta(air_filter);
  auto fillAir = [](JsonObject a0) {
    a0["aqi"] = true;
    a0["category"] = true;
    a0["pm2p5"] = true;
  };
  fillAir(air_filter["data"]["now"].to<JsonObject>());
  fillAir(air_filter["now"].to<JsonObject>());
  JsonDocument air_doc;
  String air_err;
  if (portalWeatherGet(path, air_doc, air_err, &air_filter)) {
    JsonObject air_data = weatherDataObj(air_doc);
    String air_code;
    if (!air_data.isNull() && qweatherCodeOk(air_data["code"], air_code)) {
      JsonObject air = air_data["now"].as<JsonObject>();
      if (!air.isNull()) {
        out.aqi = jsonField(air["aqi"]);
        out.aqi_category = jsonField(air["category"]);
        out.pm2p5 = jsonField(air["pm2p5"]);
      }
    }
  }
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
  snprintf(path, sizeof(path), "/v7/weather/%s?location=%s&lang=zh", hours_path,
           cfg.qweather_location_id);

  JsonDocument filter;
  filterPortalMeta(filter);
  auto fillHour = [](JsonObject h0) {
    h0["fxTime"] = true;
    h0["temp"] = true;
    h0["text"] = true;
    h0["windDir"] = true;
    h0["windScale"] = true;
    h0["humidity"] = true;
    h0["precip"] = true;
    h0["pop"] = true;
    h0["icon"] = true;
  };
  fillHour(filter["data"]["hourly"][0].to<JsonObject>());
  fillHour(filter["hourly"][0].to<JsonObject>());

  JsonDocument doc;
  if (!portalWeatherGet(path, doc, err, &filter)) {
    out.message = err;
    return false;
  }
  JsonObject data = weatherDataObj(doc);
  if (data.isNull()) {
    out.message = "no data";
    return false;
  }
  String code;
  if (!qweatherCodeOk(data["code"], code)) {
    out.message = String("hourly code ") + (code.length() ? code : "?");
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
    it.fx_time = jsonField(h["fxTime"]);
    it.temp = jsonField(h["temp"]);
    it.text = jsonField(h["text"]);
    it.wind_dir = jsonField(h["windDir"]);
    it.wind_scale = jsonField(h["windScale"]);
    it.humidity = jsonField(h["humidity"]);
    it.precip = jsonField(h["precip"]);
    it.pop = jsonField(h["pop"]);
    it.icon = jsonField(h["icon"]);
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
  snprintf(path, sizeof(path), "/v7/weather/%s?location=%s&lang=zh", days_path,
           cfg.qweather_location_id);

  JsonDocument filter;
  filterPortalMeta(filter);
  auto fillDay = [](JsonObject d0) {
    d0["fxDate"] = true;
    d0["tempMax"] = true;
    d0["tempMin"] = true;
    d0["textDay"] = true;
    d0["textNight"] = true;
    d0["windDirDay"] = true;
    d0["windScaleDay"] = true;
    d0["humidity"] = true;
    d0["precip"] = true;
    d0["uvIndex"] = true;
    d0["iconDay"] = true;
  };
  fillDay(filter["data"]["daily"][0].to<JsonObject>());
  fillDay(filter["daily"][0].to<JsonObject>());

  JsonDocument doc;
  if (!portalWeatherGet(path, doc, err, &filter)) {
    out.message = err;
    return false;
  }
  JsonObject data = weatherDataObj(doc);
  if (data.isNull()) {
    out.message = "no data";
    return false;
  }
  String code;
  if (!qweatherCodeOk(data["code"], code)) {
    out.message = String("daily code ") + (code.length() ? code : "?");
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
    it.fx_date = jsonField(d["fxDate"]);
    it.temp_max = jsonField(d["tempMax"]);
    it.temp_min = jsonField(d["tempMin"]);
    it.text_day = jsonField(d["textDay"]);
    it.text_night = jsonField(d["textNight"]);
    it.wind_dir_day = jsonField(d["windDirDay"]);
    it.wind_scale_day = jsonField(d["windScaleDay"]);
    it.humidity = jsonField(d["humidity"]);
    it.precip = jsonField(d["precip"]);
    it.uv_index = jsonField(d["uvIndex"]);
    it.icon_day = jsonField(d["iconDay"]);
  }
  out.ok = out.count > 0;
  if (!out.ok) out.message = "empty daily";
  return out.ok;
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

static int imin(int a, int b) { return a < b ? a : b; }
static int imax(int a, int b) { return a > b ? a : b; }

static int parseTempC(const String &s) { return s.toInt(); }

static int hourFromFxTime(const String &fx) {
  const int tpos = fx.indexOf('T');
  if (tpos > 0 && fx.length() >= (size_t)(tpos + 3)) {
    return fx.substring(tpos + 1, tpos + 3).toInt();
  }
  return -1;
}

static int dayFromFxDate(const String &fx) {
  int y = 0, m = 0, d = 0;
  if (parseYmd(fx, y, m, d)) return d;
  if (fx.length() >= 10) return fx.substring(8, 10).toInt();
  return -1;
}

static constexpr int kWxEdge = 10;
static constexpr int kWxPadY = kWxEdge + 15;  // 上下再各 +15

struct WeatherChartCol {
  int t_hi = 0;
  int t_lo = 0;
  WxIcon icon = WxIcon::Cloudy;
  char above[12] = {};   // 小时：柱顶气温
  char cap1[16] = {};    // 大字第 1 行（小时时间 / 日日期）
  char cap2[16] = {};    // 大字第 2 行（日·周几）
};

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

static int largeTextWidth(const char *s) {
  int w = 0;
  const unsigned char *p = reinterpret_cast<const unsigned char *>(s);
  while (*p) {
    if (*p < 0x80) {
      w += 8;
      p++;
    } else if ((*p & 0xE0) == 0xC0 && p[1]) {
      w += 16;
      p += 2;
    } else if ((*p & 0xF0) == 0xE0 && p[1] && p[2]) {
      w += 16;
      p += 3;
    } else if ((*p & 0xF8) == 0xF0 && p[1] && p[2] && p[3]) {
      w += 16;
      p += 4;
    } else {
      p++;
    }
  }
  return w;
}

static int mapTempY(int t, int t_min, int t_max, int y_top, int y_bot) {
  if (t_max <= t_min) t_max = t_min + 1;
  return y_top + (int)((long)(t_max - t) * (y_bot - y_top) / (t_max - t_min));
}

static void drawCenteredLarge(EpdSsd2683 &epd, int cx, int y, int max_y, const char *s,
                              EpdColor fg = Theme::Fg) {
  if (!s || !s[0]) return;
  const int tw = largeTextWidth(s);
  int x = cx - tw / 2;
  if (x < kWxEdge) x = kWxEdge;
  fontDrawTextWrapped(epd, x, y, EpdSsd2683::kWidth - kWxEdge - x, max_y, s, fg);
}

static void drawTempRangeChart(EpdSsd2683 &epd, int y, int bottom, const WeatherChartCol *cols,
                               int n, bool hourly) {
  if (!cols || n <= 0) return;
  const int plot_x = kWxEdge;
  const int plot_r = EpdSsd2683::kWidth - kWxEdge;
  const int plot_w = plot_r - plot_x;
  if (plot_w < 24) return;
  const int slot = plot_w / n;
  int icon_sz = slot - 4;
  if (icon_sz > 40) icon_sz = 40;
  if (icon_sz < 16) icon_sz = 16;

  const int lh = fontLineHeight();
  constexpr int kDateWdGap = 3;  // 7d：日期与周几之间
  const int cap_h = hourly ? lh : (lh * 2 + kDateWdGap);
  const int above_h = lh + 3;
  const int min_lab_h = hourly ? 0 : (lh + 3);
  const int y_top = y + icon_sz + 5 + above_h;
  const int y_bot = bottom - cap_h - 5 - min_lab_h;
  if (y_bot - y_top < 20) return;

  int t_min = cols[0].t_lo;
  int t_max = cols[0].t_hi;
  for (int i = 1; i < n; i++) {
    t_min = imin(t_min, cols[i].t_lo);
    t_max = imax(t_max, cols[i].t_hi);
  }
  t_min -= 1;
  t_max += 1;
  if (t_max <= t_min) t_max = t_min + 1;

  int bar_w = slot * 2 / 3;
  if (bar_w < 6) bar_w = 6;
  if (bar_w > 20) bar_w = 20;

  for (int i = 0; i < n; i++) {
    const int cx = plot_x + slot * i + slot / 2;
    wxIconDraw(epd, cx - icon_sz / 2, y, cols[i].icon, icon_sz);

    int y_hi = mapTempY(cols[i].t_hi, t_min, t_max, y_top, y_bot);
    int y_lo = mapTempY(cols[i].t_lo, t_min, t_max, y_top, y_bot);
    if (y_lo < y_hi) {
      const int tmp = y_hi;
      y_hi = y_lo;
      y_lo = tmp;
    }
    if (hourly) {
      int h = y_bot - y_hi + 1;
      if (h < 4) h = 4;
      y_hi = y_bot - h + 1;
      y_lo = y_bot;
    } else if (y_lo - y_hi + 1 < 6) {
      y_lo = y_hi + 5;
      if (y_lo > y_bot) {
        y_lo = y_bot;
        y_hi = imax(y_top, y_lo - 5);
      }
    }
    const int bx = cx - bar_w / 2;
    epd.fillRect(bx, y_hi, bar_w, y_lo - y_hi + 1, Theme::Alert);

    if (hourly) {
      drawCenteredLarge(epd, cx, y_hi - 3 - lh, y_hi, cols[i].above, Theme::Alert);
    } else {
      char hi_s[8], lo_s[8];
      snprintf(hi_s, sizeof(hi_s), "%d", cols[i].t_hi);
      snprintf(lo_s, sizeof(lo_s), "%d", cols[i].t_lo);
      drawCenteredLarge(epd, cx, y_hi - 3 - lh, y_hi, hi_s, Theme::Alert);
      drawCenteredLarge(epd, cx, y_lo + 4, y_lo + 4 + lh, lo_s, Theme::Alert);
    }

    int cy = y_bot + 5 + min_lab_h;
    if (hourly) {
      drawCenteredLarge(epd, cx, cy, bottom, cols[i].cap1);
    } else {
      drawCenteredLarge(epd, cx, cy, cy + lh, cols[i].cap1);
      drawCenteredLarge(epd, cx, cy + lh + kDateWdGap, bottom, cols[i].cap2);
    }
  }
}

static int wxTop() { return kWxPadY; }
static int wxBottom() { return EpdSsd2683::kHeight - kWxPadY; }

static int drawErrorBody(EpdSsd2683 &epd, int y, const char *msg) {
  y = fontDrawTextWrapped(epd, kWxEdge, y, 380, wxBottom(), "错误详情", Theme::Alert);
  y += 4;
  return fontDrawTextWrapped(epd, kWxEdge, y, 380, wxBottom(),
                             msg && msg[0] ? msg : "(unknown error)", Theme::Fg);
}

static void drawErrorPage(EpdSsd2683 &epd, WeatherMode mode, const char *msg, const char *status) {
  (void)mode;
  (void)status;
  epd.clear(Theme::Bg);
  drawErrorBody(epd, wxTop(), msg);
  epd.display();
}

void weatherDrawNow(EpdSsd2683 &epd, const WeatherNowData &now, const WeatherHourlyData &hourly,
                    const char *status) {
  (void)status;
  epd.clear(Theme::Bg);
  int y = wxTop();
  const int bottom = wxBottom();
  if (!now.ok) {
    drawErrorBody(epd, y, now.message.length() ? now.message.c_str() : "(error)");
  } else {
    char line[112];
    const int big = 90;
    wxIconDraw(epd, kWxEdge, y, wxIconFromCode(now.icon.c_str()), big);
    const int tx = kWxEdge + big + 8;
    const AppConfig &cfg = getAppConfig();
    const char *loc = cfg.qweather_location_name;
    int loc_col = 0;
    if (loc && loc[0]) {
      loc_col = largeTextWidth(loc) * 2;
      if (loc_col > 160) loc_col = 160;
      if (loc_col < 32) loc_col = 32;
    }
    int loc_x = EpdSsd2683::kWidth - kWxEdge - loc_col;
    int tw = EpdSsd2683::kWidth - tx - kWxEdge;
    if (loc_col > 0) {
      tw = loc_x - 8 - tx;
      if (tw < 96) {
        loc_col = imax(32, loc_col - (96 - tw));
        loc_x = EpdSsd2683::kWidth - kWxEdge - loc_col;
        tw = loc_x - 8 - tx;
      }
    }
    int ty = y;
    snprintf(line, sizeof(line), "%s  %sC", now.text.c_str(), now.temp.c_str());
    ty = fontDrawTextWrapped(epd, tx, ty, tw, bottom, line, Theme::Alert) + 3;
    snprintf(line, sizeof(line), "湿度 %s%%  体感 %sC", now.humidity.c_str(), now.feels_like.c_str());
    ty = fontDrawTextWrapped(epd, tx, ty, tw, bottom, line, Theme::Fg) + 3;
    if (now.aqi_category.length() || now.aqi.length()) {
      if (now.pm2p5.length()) {
        snprintf(line, sizeof(line), "空气 %s %s  PM2.5 %s", now.aqi_category.c_str(),
                 now.aqi.c_str(), now.pm2p5.c_str());
      } else {
        snprintf(line, sizeof(line), "空气 %s %s", now.aqi_category.c_str(), now.aqi.c_str());
      }
      ty = fontDrawTextWrapped(epd, tx, ty, tw, bottom, line, Theme::Fg) + 3;
    }
    if (now.wind_dir.length() || now.wind_scale.length()) {
      snprintf(line, sizeof(line), "风 %s %s级", now.wind_dir.c_str(), now.wind_scale.c_str());
      ty = fontDrawTextWrapped(epd, tx, ty, tw, bottom, line, Theme::Fg);
    }
    if (loc_col > 0) {
      fontDrawTextWrappedDouble(epd, loc_x, y, loc_col, y + big, loc, Theme::Fg);
    }
    y = imax(y + big, ty) + 4;
    if (!hourly.ok || hourly.count <= 0) {
      drawErrorBody(epd, y, hourly.message.length() ? hourly.message.c_str() : "(no 6h)");
    } else {
      WeatherChartCol cols[WEATHER_HOURLY_MAX];
      const int n = imin(hourly.count, WEATHER_HOURLY_MAX);
      for (int i = 0; i < n; i++) {
        const int t = parseTempC(hourly.hours[i].temp);
        cols[i].t_hi = t;
        cols[i].t_lo = t;
        snprintf(cols[i].above, sizeof(cols[i].above), "%d", t);
        const int hh = hourFromFxTime(hourly.hours[i].fx_time);
        if (hh >= 0) snprintf(cols[i].cap1, sizeof(cols[i].cap1), "%d时", hh);
        cols[i].icon = wxIconFromCode(hourly.hours[i].icon.c_str());
      }
      drawTempRangeChart(epd, y, bottom, cols, n, true);
    }
  }
  epd.display();
}

void weatherDrawForecast(EpdSsd2683 &epd, const WeatherDailyData &d, WeatherMode mode,
                         const char *status) {
  (void)mode;
  (void)status;
  epd.clear(Theme::Bg);
  const int y = wxTop();
  if (!d.ok || d.count <= 0) {
    drawErrorBody(epd, y, d.message.length() ? d.message.c_str() : "(error)");
  } else {
    WeatherChartCol cols[WEATHER_DAILY_MAX];
    const int n = imin(d.count, WEATHER_DAILY_MAX);
    for (int i = 0; i < n; i++) {
      const int hi = parseTempC(d.days[i].temp_max);
      const int lo = parseTempC(d.days[i].temp_min);
      cols[i].t_hi = imax(hi, lo);
      cols[i].t_lo = imin(hi, lo);
      int ymd_y = 0, ymd_m = 0, ymd_d = 0;
      if (parseYmd(d.days[i].fx_date, ymd_y, ymd_m, ymd_d)) {
        snprintf(cols[i].cap1, sizeof(cols[i].cap1), "%02d/%02d", ymd_m, ymd_d);
        snprintf(cols[i].cap2, sizeof(cols[i].cap2), "周%s",
                 weekdayCn(weekdayFromYmd(ymd_y, ymd_m, ymd_d)));
      } else {
        const int dd = dayFromFxDate(d.days[i].fx_date);
        if (dd >= 0) snprintf(cols[i].cap1, sizeof(cols[i].cap1), "%02d", dd);
      }
      cols[i].icon = wxIconFromCode(d.days[i].icon_day.c_str());
    }
    drawTempRangeChart(epd, y, wxBottom(), cols, n, false);
  }
  epd.display();
}

bool weatherShowMode(EpdSsd2683 &epd, WeatherMode mode) {
  WdtLongOpGuard wdt;  // fetch + 记日志 + 排版，直到 epd.display 自己再套一层
  mode = weatherModeNormalize(mode);
  if (!epd.ok()) {
    errorLog("weather", "EPD not ready");
    return false;
  }
  if (!qweatherConfigured()) {
    const char *msg =
        "天气未就绪：需 Portal 登录 + 已选位置。\nWeb：/auth 登录，再打开 /weather 选城市。"
        "\n（和风密钥在服务器，设备不存 Key）";
    drawErrorPage(epd, mode, msg, "cfg");
    return true;  // UI 已按 mode 刷新；BOOT 切换应看到标题变化，勿 ErrorBurst
  }

  switch (mode) {
    case WeatherMode::Hour12:
    case WeatherMode::Now: {
      const bool ok_now = weatherFetchNow(g_now_buf);
      if (!ok_now) {
        errorLog("weather", g_now_buf.message.length() ? g_now_buf.message : "now fetch failed");
      }
      const bool ok_h = weatherFetchHourly("24h", g_hourly_buf);
      if (!ok_h) {
        errorLog("weather",
                 g_hourly_buf.message.length() ? g_hourly_buf.message : "hourly fetch failed");
      }
      const char *st = (!ok_now || !ok_h) ? "err" : "OK";
      weatherDrawNow(epd, g_now_buf, g_hourly_buf, st);
      return true;
    }
    case WeatherMode::Week14:
    case WeatherMode::Week7: {
      const bool ok = weatherFetchDaily("7d", g_daily_buf);
      if (!ok) errorLog("weather", g_daily_buf.message.length() ? g_daily_buf.message : "daily 7d failed");
      weatherDrawForecast(epd, g_daily_buf, WeatherMode::Week7, ok ? "OK" : "err");
      return true;
    }
    default:
      errorLog("weather", "unknown weather mode");
      return false;
  }
}
