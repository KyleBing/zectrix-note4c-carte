#include "app_weather.h"
#include "app_config.h"
#include "app_font.h"
#include "app_wifi.h"
#include "app_header.h"
#include "app_theme.h"
#include "app_footer.h"
#include "app_version.h"
#include "app_errorlog.h"
#include "app_power.h"
#include "app_led.h"

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
  // chrome_left = app indicator; never draw status over it (was covering dots with "OK").
  const int chrome_left = headerDrawAppBar(epd, AppId::Weather, nullptr);
  (void)mode;
  const AppConfig &cfg = getAppConfig();
  char head[96];
  if (cfg.qweather_location_name[0]) {
    snprintf(head, sizeof(head), "天气 · %s", cfg.qweather_location_name);
  } else {
    snprintf(head, sizeof(head), "天气");
  }
  const int title_max = chrome_left - Theme::MarginX - 8;
  if (title_max > 40) {
    fontDrawTextWrapped(epd, Theme::MarginX, 3, title_max, Theme::HeaderH, head, Theme::HeaderFg,
                        Theme::HeaderBg);
  }
  // Only show failure status; success is obvious from content. Keep left of indicator.
  if (status && status[0] && strcmp(status, "OK") != 0) {
    const int sw = (int)strlen(status) * 6;
    const int sx = chrome_left - 6 - sw;
    if (sx > Theme::MarginX + 48) {
      epd.drawText(sx, 7, status, Theme::HeaderAccent, Theme::HeaderBg);
    }
  }
}

// BOOT cycles these; yellow pill marks the active window so a mode change is obvious.
static int drawWeatherModeTabs(EpdSsd2683 &epd, WeatherMode mode, int y) {
  static const char *kLabel[] = {"NOW", "12H", "7D", "14D"};
  int x = Theme::MarginX;
  for (int i = 0; i < static_cast<int>(WeatherMode::Count); i++) {
    const char *lab = kLabel[i];
    const int tw = (int)strlen(lab) * 6;
    const int pad = 4;
    const int bw = tw + pad * 2;
    const int bh = 12;
    const bool sel = (i == static_cast<int>(mode));
    if (sel) {
      epd.fillRect(x, y, bw, bh, Theme::Accent);
      epd.drawText(x + pad, y + 2, lab, Theme::Fg, Theme::Accent);
    } else {
      epd.drawRect(x, y, bw, bh, Theme::Fg);
      epd.drawText(x + pad, y + 2, lab, Theme::Fg);
    }
    x += bw + 6;
  }
  return y + 16;
}

static void drawWeatherFooter(EpdSsd2683 &epd) {
  footerDraw(epd, "mode");
}

static int drawErrorBody(EpdSsd2683 &epd, int y, const char *msg) {
  y = fontDrawTextWrapped(epd, Theme::MarginX, y, 384, Theme::FooterY - 4, "错误详情",
                          Theme::Alert);
  y += 4;
  return fontDrawTextWrapped(epd, Theme::MarginX, y, 384, Theme::FooterY - 4,
                             msg && msg[0] ? msg : "(unknown error)", Theme::Fg);
}

static void drawErrorPage(EpdSsd2683 &epd, WeatherMode mode, const char *msg, const char *status) {
  epd.clear(Theme::Bg);
  drawWeatherHeader(epd, mode, status);
  int y = drawWeatherModeTabs(epd, mode, 26);
  drawErrorBody(epd, y, msg);
  drawWeatherFooter(epd);
  epd.display();
}

void weatherDrawNow(EpdSsd2683 &epd, const WeatherNowData &d, const char *status) {
  epd.clear(Theme::Bg);
  drawWeatherHeader(epd, WeatherMode::Now, status);
  int y = drawWeatherModeTabs(epd, WeatherMode::Now, 26);
  if (!d.ok) {
    drawErrorBody(epd, y, d.message.length() ? d.message.c_str() : "(error)");
  } else {
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
  int y = drawWeatherModeTabs(epd, WeatherMode::Hour12, 26);
  if (!d.ok || d.count <= 0) {
    drawErrorBody(epd, y, d.message.length() ? d.message.c_str() : "(no hourly)");
  } else {
    // One row per hour: "11-19 15:00  12C  多云  东北风3"
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
  int y = drawWeatherModeTabs(epd, mode, 26);
  if (!d.ok || d.count <= 0) {
    drawErrorBody(epd, y, d.message.length() ? d.message.c_str() : "(error)");
  } else {
    // One row per day. 14D uses smaller glyphs so 14 rows fit.
    const bool compact = (mode == WeatherMode::Week14) || (d.count > 8);
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
  WdtLongOpGuard wdt;  // fetch + 记日志 + 排版，直到 epd.display 自己再套一层
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
    case WeatherMode::Now: {
      const bool ok = weatherFetchNow(g_now_buf);
      if (!ok) errorLog("weather", g_now_buf.message.length() ? g_now_buf.message : "now fetch failed");
      weatherDrawNow(epd, g_now_buf, ok ? "OK" : "err");
      return true;
    }
    case WeatherMode::Hour12: {
      const bool ok = weatherFetchHourly("24h", g_hourly_buf);  // API min 24h; UI shows first 12
      if (!ok) errorLog("weather", g_hourly_buf.message.length() ? g_hourly_buf.message : "hourly fetch failed");
      weatherDrawHourly(epd, g_hourly_buf, ok ? "OK" : "err");
      return true;
    }
    case WeatherMode::Week7: {
      const bool ok = weatherFetchDaily("7d", g_daily_buf);
      if (!ok) errorLog("weather", g_daily_buf.message.length() ? g_daily_buf.message : "daily 7d failed");
      weatherDrawForecast(epd, g_daily_buf, mode, ok ? "OK" : "err");
      return true;
    }
    case WeatherMode::Week14: {
      const bool ok = weatherFetchDaily("15d", g_daily_buf);
      if (ok && g_daily_buf.count > 14) g_daily_buf.count = 14;  // show 14 days
      if (!ok) errorLog("weather", g_daily_buf.message.length() ? g_daily_buf.message : "daily 14d failed");
      weatherDrawForecast(epd, g_daily_buf, mode, ok ? "OK" : "err");
      return true;
    }
    default:
      errorLog("weather", "unknown weather mode");
      return false;
  }
}
