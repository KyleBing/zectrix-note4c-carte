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
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
// platform 7: esp32s3/rom/miniz.h；platform 8+ Arduino-ESP32 3.x: 扁平 miniz.h
#if __has_include("esp32s3/rom/miniz.h")
#include "esp32s3/rom/miniz.h"
#elif __has_include("miniz.h")
#include "miniz.h"
#else
#error "ESP-ROM miniz.h not found (need tinfl for QWeather gzip)"
#endif
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <vector>

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
  // 关 sleep；长刷屏后仍可能假连，后面 HTTPS 失败会再 force reconnect
  if (wifiPrepareSta(18000)) return true;
  if (!hasSavedWifiConfig()) {
    err = "No WiFi. hold UP = config web";
    return false;
  }
  err = "WiFi connect failed";
  return false;
}

// 从 https://host/path 取出 host（供 DNS 预热）
static String weatherUrlHost(const String &url) {
  int start = 0;
  if (url.startsWith("https://")) start = 8;
  else if (url.startsWith("http://")) start = 7;
  const int slash = url.indexOf('/', start);
  if (slash < 0) return url.substring(start);
  return url.substring(start, slash);
}

static bool weatherWarmDns(const String &url, String &err) {
  const String host = weatherUrlHost(url);
  IPAddress ip;
  if (wifiResolveHost(host.c_str(), ip, 3)) return true;
  Serial.printf("[Weather] DNS dead, force WiFi reconnect for %s\n", host.c_str());
  if (!wifiForceReconnect(12000)) {
    err = String("WiFi reconnect failed (DNS ") + host + ")";
    return false;
  }
  if (wifiResolveHost(host.c_str(), ip, 3)) return true;
  err = String("DNS fail ") + host;
  return false;
}

// 旧公共主机：geo 与天气分域；新 API Host 则同一主机。
static String weatherDirectBase(const AppConfig &cfg, bool for_geo) {
  String host = cfg.qweather_host;
  if (for_geo && (host == "devapi.qweather.com" || host == "api.qweather.com")) {
    host = "geoapi.qweather.com";
  }
  return String("https://") + host;
}

// 跳过 gzip 头，返回 deflate 载荷起点；失败返回 nullptr。
static const uint8_t *gzipSkipHeader(const uint8_t *src, size_t src_len, size_t &deflate_len) {
  deflate_len = 0;
  if (!src || src_len < 18 || src[0] != 0x1f || src[1] != 0x8b || src[2] != 8) return nullptr;
  const uint8_t flags = src[3];
  size_t pos = 10;
  if (flags & 4) {  // FEXTRA
    if (pos + 2 > src_len) return nullptr;
    const size_t xlen = (size_t)src[pos] | ((size_t)src[pos + 1] << 8);
    pos += 2 + xlen;
  }
  if (flags & 8) {  // FNAME
    while (pos < src_len && src[pos]) pos++;
    if (pos >= src_len) return nullptr;
    pos++;
  }
  if (flags & 16) {  // FCOMMENT
    while (pos < src_len && src[pos]) pos++;
    if (pos >= src_len) return nullptr;
    pos++;
  }
  if (flags & 2) {  // FHCRC
    if (pos + 2 > src_len) return nullptr;
    pos += 2;
  }
  if (src_len < pos + 8) return nullptr;  // 尾部 CRC32 + ISIZE
  deflate_len = src_len - pos - 8;
  return src + pos;
}

// 和风常返回 gzip。tinfl_decompress_mem_to_mem 会把 ~30KB decompressor 放栈上，
// Web 回调已在 loopTask 深栈里，必炸 canary —— decompressor / 输出缓冲一律上堆。
static bool gunzipPayload(const uint8_t *src, size_t src_len, String &out, String &err) {
  size_t deflate_len = 0;
  const uint8_t *deflate = gzipSkipHeader(src, src_len, deflate_len);
  if (!deflate || deflate_len == 0) {
    err = "bad gzip header";
    return false;
  }
  // gzip 尾 ISIZE = 未压缩长度（模 2^32）
  const uint32_t isize = (uint32_t)src[src_len - 4] | ((uint32_t)src[src_len - 3] << 8) |
                         ((uint32_t)src[src_len - 2] << 16) | ((uint32_t)src[src_len - 1] << 24);
  size_t out_cap = isize ? (size_t)isize : (deflate_len * 4);
  if (out_cap < 256) out_cap = 256;
  if (out_cap > kWeatherJsonCap) out_cap = kWeatherJsonCap;

  std::unique_ptr<uint8_t[]> plain(new (std::nothrow) uint8_t[out_cap]);
  std::unique_ptr<tinfl_decompressor> decomp(new (std::nothrow) tinfl_decompressor());
  if (!plain || !decomp) {
    err = "gunzip OOM";
    return false;
  }
  tinfl_init(decomp.get());
  size_t in_sz = deflate_len;
  size_t out_sz = out_cap;  // 入：容量；成功返回后：已写出字节数（同 tinfl_decompress_mem_to_mem）
  const tinfl_status st = tinfl_decompress(
      decomp.get(), deflate, &in_sz, plain.get(), plain.get(), &out_sz,
      TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
  if (st != TINFL_STATUS_DONE || out_sz == 0 || out_sz > out_cap) {
    err = String("tinfl st=") + (int)st + " wrote=" + (int)out_sz;
    return false;
  }
  out = "";
  out.reserve(out_sz);
  for (size_t i = 0; i < out_sz; i++) out += (char)plain[i];
  return true;
}

// GET 天气：Portal（Bearer JWT）或和风普通 API Key（query key=，非 JWT）。
// Stream+filter，避免整包 body 再拷进 JsonDocument；gzip 时再整包解压。
static bool weatherHttpGet(const char *path_and_query, JsonDocument &doc, String &err,
                           const JsonDocument *filter = nullptr, bool for_geo = false) {
  err = "";
  const AppConfig &cfg = getAppConfig();
  const bool api_key = qweatherSourceIsApiKey();

  String path = path_and_query ? path_and_query : "";
  String url;
  if (api_key) {
    if (!cfg.qweather_host[0]) {
      err = "qweather host empty. Open web /weather";
      return false;
    }
    if (!cfg.qweather_key[0]) {
      err = "qweather key empty. Open web /weather";
      return false;
    }
    // 普通 API：查询参数 key=...（非 JWT Authorization）
    path += (path.indexOf('?') >= 0) ? "&key=" : "?key=";
    path += cfg.qweather_key;
    url = joinUrl(weatherDirectBase(cfg, for_geo).c_str(), path.c_str());
  } else {
    if (!cfg.portal_base_url[0]) {
      err = "portal base_url empty";
      return false;
    }
    if (!cfg.portal_token[0]) {
      err = "not logged in. Open web /auth";
      return false;
    }
    url = joinUrl(cfg.portal_base_url, path.c_str());
  }

  Serial.printf("[Weather] GET %s src=%s heap=%u\n", path_and_query,
                api_key ? "apikey" : "portal", (unsigned)ESP.getFreeHeap());

  // HTTPS 拉数快闪；函数返回前必须灭灯，否则会泄漏进随后 ~16s 全刷（busy 里仍 ledUpdate）
  struct ScopeLedOff {
    ~ScopeLedOff() { ledOff(); }
  } scope_led_off;
  ledSet(LedPattern::BlinkFast);

  // 预热 DNS：长刷屏 / 中断后常见 hostByName 假死 → start_ssl_client:-1
  if (!weatherWarmDns(url, err)) return false;

  DeserializationError jerr = DeserializationError::Ok;
  int code = 0;
  int size = -1;
  bool truncated = false;
  size_t got = 0;

  for (int attempt = 0; attempt < 2; attempt++) {
    if (attempt > 0) {
      Serial.printf("[Weather] retry GET after recover rssi=%d ip=%s\n", wifiStaRssi(),
                    wifiStaIp().c_str());
      if (!wifiForceReconnect(12000) || !weatherWarmDns(url, err)) return false;
      ledSet(LedPattern::BlinkFast);
    }

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
      if (attempt == 0) continue;
      return false;
    }
    http.setTimeout(15000);
    http.setConnectTimeout(8000);
    http.useHTTP10(true);
    if (api_key) {
      // 和风也支持 Header；与 query key 二选一即可，这里只走 query，避免混用
      http.addHeader("Accept-Encoding", "gzip");
    } else {
      http.addHeader("Authorization", String("Bearer ") + cfg.portal_token);
      http.addHeader("Accept-Encoding", "identity");
    }
    const char *headerKeys[] = {kRenewHeader, "Content-Encoding"};
    http.collectHeaders(headerKeys, 2);

    {
      WdtLongOpGuard wdt_off;  // TLS busy-spin 会饿死本核 idle WDT；仅关 loop WDT 不够
      code = http.GET();
      if (code <= 0) {
        Serial.printf("[Weather] HTTP %d wifi=%d rssi=%d ip=%s heap=%u\n", code, (int)WiFi.status(),
                      wifiStaRssi(), wifiStaIp().c_str(), (unsigned)ESP.getFreeHeap());
        http.end();
        if (attempt == 0) continue;  // 重连再试一次
        err = String("HTTP ") + code + " " + path_and_query;
        return false;
      }
      if (code == 401) {
        if (!api_key) captureRenewedToken(http);
        http.end();
        err = api_key ? String("401 未授权 ") + path_and_query + " — 检查和风 Key / Host"
                      : String("401 未授权 ") + path_and_query + " — 请在 Web /auth 重新登录";
        return false;
      }
      if (code < 200 || code >= 300) {
        http.end();
        err = String("HTTP ") + code + " " + path_and_query;
        if (!api_key && code == 404) err += "（Portal 无此路由，确认已部署天气 hourly）";
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

      const String enc = http.header("Content-Encoding");
      // 和风直连常 gzip（Header 偶发缺失）：API 模式整包读入后按魔数/头判断
      const bool prefer_buffer = api_key || enc.indexOf("gzip") >= 0;
      if (prefer_buffer) {
        const size_t cap = (size > 0) ? (size_t)size : kWeatherJsonCap;
        std::vector<uint8_t> raw;
        raw.reserve(cap);
        WiFiClient &stream = http.getStream();
        uint32_t last = millis();
        while (http.connected() && (size < 0 || (int)raw.size() < size)) {
          while (stream.available()) {
            const int c = stream.read();
            if (c < 0) break;
            raw.push_back((uint8_t)c);
            if (raw.size() > kWeatherJsonCap) {
              http.end();
              err = String("body >") + (int)kWeatherJsonCap;
              return false;
            }
          }
          if (size >= 0 && (int)raw.size() >= size) break;
          if (!stream.available()) {
            if (millis() - last > 12000) break;
            delay(1);
            weatherPump();
          } else {
            last = millis();
          }
        }
        got = raw.size();
        if (!api_key) captureRenewedToken(http);
        http.end();

        const bool looks_gzip =
            enc.indexOf("gzip") >= 0 ||
            (raw.size() >= 2 && raw[0] == 0x1f && raw[1] == 0x8b);
        String plain;
        if (looks_gzip) {
          String gz_err;
          if (!gunzipPayload(raw.data(), raw.size(), plain, gz_err)) {
            err = String("gzip ") + gz_err + " " + path_and_query;
            Serial.printf("[Weather] %s\n", err.c_str());
            return false;
          }
        } else {
          plain.reserve(raw.size());
          for (uint8_t c : raw) plain += (char)c;
        }
        got = plain.length();
        jerr = filter ? deserializeJson(doc, plain, DeserializationOption::Filter(*filter))
                      : deserializeJson(doc, plain);
      } else {
        WeatherBodyStream body(http.getStream(), kWeatherJsonCap);
        jerr = filter ? deserializeJson(doc, body, DeserializationOption::Filter(*filter))
                      : deserializeJson(doc, body);
        truncated = body.truncated();
        got = body.bytes();
        if (!api_key) captureRenewedToken(http);
        http.end();
      }
    }
    break;  // GET 成功拿到 body，离开 retry 循环
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
    if (!m.length()) m = api_key ? "qweather api failed" : "portal weather failed";
    err = m + " | " + path_and_query;
    if (qcode.length()) err += String(" qcode=") + qcode;
    err += String(" HTTP ") + code;
    Serial.printf("[Weather] fail: %s heap=%u\n", err.c_str(), (unsigned)ESP.getFreeHeap());
    return false;
  }
  return true;
}

static bool portalWeatherGet(const char *path_and_query, JsonDocument &doc, String &err,
                             const JsonDocument *filter = nullptr) {
  return weatherHttpGet(path_and_query, doc, err, filter, false);
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
  if (!weatherHttpGet(path.c_str(), doc, err, nullptr, true)) {
    out.message = err;
    return false;
  }

  // Portal 包一层 data；和风直连顶层即 {code, location}
  JsonObject data = weatherDataObj(doc);
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

static const char *weatherNotReadyMsg() {
  return qweatherSourceIsApiKey()
             ? "Weather not ready. Set Host/Key + location in web /weather"
             : "Weather not ready. Login + set location in web /weather";
}

bool weatherFetchNow(WeatherNowData &out) {
  out = WeatherNowData{};
  if (!qweatherConfigured()) {
    out.message = weatherNotReadyMsg();
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
    n0["wind360"] = true;
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
  out.wind_360 = jsonField(now["wind360"]);
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
    out.message = weatherNotReadyMsg();
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
    out.message = weatherNotReadyMsg();
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
  int weekday = -1;  // 0=日 … 6=六；7d 周几着色用
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

// 棋盘抖动：1px 色 A、1px 色 B，混成中间色
static void fillDitherRect(EpdSsd2683 &epd, int x, int y, int w, int h, EpdColor a, EpdColor b) {
  if (w <= 0 || h <= 0) return;
  for (int row = y; row < y + h; row++) {
    for (int col = x; col < x + w; col++) {
      epd.setPixel(col, row, ((col + row) & 1) ? b : a);
    }
  }
}

// 7D 柱色带（见 docs/features/behavior.md）：band_lo 为区间下界
//   ≤-10 黑；[-10,0) 红黑抖；[0,10) 红；[10,20) 红黄抖；[20,30) 红白抖；≥30 黄
static void fillTempZoneRect(EpdSsd2683 &epd, int bx, int bar_w, int y0, int y1, int band_lo) {
  if (y1 < y0 || bar_w <= 0) return;
  const int h = y1 - y0 + 1;
  if (band_lo >= 30) {
    epd.fillRect(bx, y0, bar_w, h, Theme::Accent);
  } else if (band_lo >= 20) {
    fillDitherRect(epd, bx, y0, bar_w, h, Theme::Alert, Theme::Bg);
  } else if (band_lo >= 10) {
    fillDitherRect(epd, bx, y0, bar_w, h, Theme::Alert, Theme::Accent);
  } else if (band_lo >= 0) {
    epd.fillRect(bx, y0, bar_w, h, Theme::Alert);
  } else if (band_lo >= -10) {
    fillDitherRect(epd, bx, y0, bar_w, h, Theme::Alert, Theme::Fg);
  } else {
    epd.fillRect(bx, y0, bar_w, h, Theme::Fg);
  }
}

static void fillTempSeg(EpdSsd2683 &epd, int bx, int bar_w, int seg_lo, int seg_hi, int band_lo,
                        int t_min, int t_max, int y_top, int y_bot, int clip_y0, int clip_y1) {
  if (seg_lo > seg_hi) return;
  int y0 = mapTempY(seg_hi, t_min, t_max, y_top, y_bot);
  int y1 = mapTempY(seg_lo, t_min, t_max, y_top, y_bot);
  if (y1 < y0) {
    const int tmp = y0;
    y0 = y1;
    y1 = tmp;
  }
  y0 = imax(y0, clip_y0);
  y1 = imin(y1, clip_y1);
  fillTempZoneRect(epd, bx, bar_w, y0, y1, band_lo);
}

// 半开区间 [lo, hi)：从冷到热填，分界像素归更高一档
static void fillTempBandedBar(EpdSsd2683 &epd, int bx, int bar_w, int t_lo, int t_hi, int t_min,
                              int t_max, int y_top, int y_bot, int clip_y0, int clip_y1) {
  if (bar_w <= 0 || clip_y1 < clip_y0) return;
  if (t_hi < t_lo) {
    const int tmp = t_hi;
    t_hi = t_lo;
    t_lo = tmp;
  }
  static const int kLo[] = {-1000, -10, 0, 10, 20, 30};
  static const int kHi[] = {-10, 0, 10, 20, 30, 1000};
  for (int i = 0; i < 6; i++) {
    if (t_hi < kLo[i] || t_lo >= kHi[i]) continue;
    fillTempSeg(epd, bx, bar_w, imax(t_lo, kLo[i]), imin(t_hi, kHi[i]), kLo[i], t_min, t_max,
                y_top, y_bot, clip_y0, clip_y1);
  }
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
  constexpr int kBarCapGap = 8;  // 柱底与小时/日期标签间距
  const int y_top = y + icon_sz + 5 + above_h;
  const int y_bot = bottom - cap_h - kBarCapGap - min_lab_h;
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
    if (hourly) {
      epd.fillRect(bx, y_hi, bar_w, y_lo - y_hi + 1, Theme::Alert);
      drawCenteredLarge(epd, cx, y_hi - 3 - lh, y_hi, cols[i].above, Theme::Alert);
    } else {
      fillTempBandedBar(epd, bx, bar_w, cols[i].t_lo, cols[i].t_hi, t_min, t_max, y_top, y_bot,
                        y_hi, y_lo);
      // 最低高度撑开时，空隙用端点色带补齐
      const int mapped_hi = mapTempY(cols[i].t_hi, t_min, t_max, y_top, y_bot);
      const int mapped_lo = mapTempY(cols[i].t_lo, t_min, t_max, y_top, y_bot);
      if (y_hi < mapped_hi) {
        fillTempZoneRect(epd, bx, bar_w, y_hi, mapped_hi - 1, cols[i].t_hi);
      }
      if (y_lo > mapped_lo) {
        fillTempZoneRect(epd, bx, bar_w, mapped_lo + 1, y_lo, cols[i].t_lo);
      }
      char hi_s[8], lo_s[8];
      snprintf(hi_s, sizeof(hi_s), "%d", cols[i].t_hi);
      snprintf(lo_s, sizeof(lo_s), "%d", cols[i].t_lo);
      drawCenteredLarge(epd, cx, y_hi - 3 - lh, y_hi, hi_s, Theme::Alert);
      drawCenteredLarge(epd, cx, y_lo + 4, y_lo + 4 + lh, lo_s, Theme::Alert);
    }

    int cy = y_bot + kBarCapGap + min_lab_h;
    if (hourly) {
      drawCenteredLarge(epd, cx, cy, bottom, cols[i].cap1);
    } else {
      drawCenteredLarge(epd, cx, cy, cy + lh, cols[i].cap1);
      const int wd_y = cy + lh + kDateWdGap;
      // 周六、周日红色；其余黑
      if (cols[i].weekday == 0 || cols[i].weekday == 6) {
        drawCenteredLarge(epd, cx, wd_y, bottom, cols[i].cap2, Theme::Alert);
      } else {
        drawCenteredLarge(epd, cx, wd_y, bottom, cols[i].cap2);
      }
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

// 风来向角度：优先 wind360；否则解析中文风向。返回 -1 表示未知。
static int windFromDeg(const WeatherNowData &now) {
  if (now.wind_360.length()) {
    const int d = now.wind_360.toInt();
    if (d >= 0 && d <= 360) return d % 360;
  }
  const String &s = now.wind_dir;
  if (!s.length()) return -1;
  if (s.indexOf("东北") >= 0) return 45;
  if (s.indexOf("东南") >= 0) return 135;
  if (s.indexOf("西南") >= 0) return 225;
  if (s.indexOf("西北") >= 0) return 315;
  if (s.indexOf("北") >= 0) return 0;
  if (s.indexOf("东") >= 0) return 90;
  if (s.indexOf("南") >= 0) return 180;
  if (s.indexOf("西") >= 0) return 270;
  return -1;
}

// 纸飞机实心风向标（由发送箭头 SVG 归一化，尖端朝北=上）。单位约 ±100。
static const int8_t kWindOuter[][2] = {
    {0, -100}, {-4, -99}, {-7, -95}, {-62, 37}, {-62, 41}, {-60, 46}, {-56, 48}, {-51, 47},
    {0, 21},   {53, 46},  {58, 47},  {62, 45},  {64, 41},  {63, 36},  {7, -95},  {4, -99},
};
static const int8_t kWindInner[][2] = {
    {42, 24}, {4, 6}, {0, 5}, {-40, 25}, {0, -73},
};
static constexpr int kWindOuterN = sizeof(kWindOuter) / sizeof(kWindOuter[0]);
static constexpr int kWindInnerN = sizeof(kWindInner) / sizeof(kWindInner[0]);

static bool windPointInPoly(int x, int y, const int (*poly)[2], int n) {
  bool inside = false;
  for (int i = 0, j = n - 1; i < n; j = i++) {
    const int yi = poly[i][1], yj = poly[j][1];
    const int xi = poly[i][0], xj = poly[j][0];
    const bool cross = ((yi > y) != (yj > y));
    if (!cross) continue;
    const int den = yj - yi;
    if (den == 0) continue;
    const int xinter = xi + (int)((long)(xj - xi) * (y - yi) / den);
    if (x < xinter) inside = !inside;
  }
  return inside;
}

// 箭头指向风来向（北=上）。cx,cy 为中心；实心纸飞机造型。
static void drawWindArrow(EpdSsd2683 &epd, int cx, int cy, int deg, EpdColor c) {
  constexpr float kPi = 3.14159265f;
  constexpr float kScale = 0.12f;  // 100 → 12px
  const float rad = (float)deg * kPi / 180.0f;
  const float cos_t = cosf(rad);
  const float sin_t = sinf(rad);

  int outer[kWindOuterN][2];
  int inner[kWindInnerN][2];
  int min_x = 1000, min_y = 1000, max_x = -1000, max_y = -1000;

  auto xform = [&](int ux, int uy, int &ox, int &oy) {
    // 单位：x 东、y 北（尖端 y=-100 → 北）。按气象角顺时针旋转。
    const float mx = (float)ux;
    const float my = -(float)uy;
    const float rx = mx * cos_t + my * sin_t;
    const float ry = -mx * sin_t + my * cos_t;
    ox = cx + (int)lroundf(rx * kScale);
    oy = cy - (int)lroundf(ry * kScale);
  };

  for (int i = 0; i < kWindOuterN; i++) {
    xform(kWindOuter[i][0], kWindOuter[i][1], outer[i][0], outer[i][1]);
    min_x = imin(min_x, outer[i][0]);
    max_x = imax(max_x, outer[i][0]);
    min_y = imin(min_y, outer[i][1]);
    max_y = imax(max_y, outer[i][1]);
  }
  for (int i = 0; i < kWindInnerN; i++) {
    xform(kWindInner[i][0], kWindInner[i][1], inner[i][0], inner[i][1]);
    min_x = imin(min_x, inner[i][0]);
    max_x = imax(max_x, inner[i][0]);
    min_y = imin(min_y, inner[i][1]);
    max_y = imax(max_y, inner[i][1]);
  }

  for (int y = min_y; y <= max_y; y++) {
    for (int x = min_x; x <= max_x; x++) {
      const bool on = windPointInPoly(x, y, outer, kWindOuterN) ^
                      windPointInPoly(x, y, inner, kWindInnerN);
      if (on) epd.setPixel(x, y, c);
    }
  }
}

void weatherDrawNow(EpdSsd2683 &epd, const WeatherNowData &now, const WeatherHourlyData &hourly,
                    const char *status) {
  (void)status;
  epd.clear(Theme::Bg);
  constexpr int kNowTop = 18;       // 上区距顶
  constexpr int kNowIconGap = 20;   // 大图标与右侧信息
  constexpr int kNowBelowGap = 15;  // 上区与下方小时柱
  int y = kNowTop;
  const int bottom = wxBottom();
  if (!now.ok) {
    drawErrorBody(epd, y, now.message.length() ? now.message.c_str() : "(error)");
  } else {
    char line[112];
    char temp_s[16];
    char hum_s[16];
    const int big = 90;
    const int dlh = fontLineHeightDouble();
    wxIconDraw(epd, kWxEdge, y, wxIconFromCode(now.icon.c_str()), big);
    const int tx = kWxEdge + big + kNowIconGap;
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

    // 上区相对 90px 图标纵向居中
    const int lh = fontLineHeight();
    const int block_h = dlh + 4 + lh;
    const int content_y = y + imax(0, (big - block_h) / 2);
    const int loc_y = y + imax(0, (big - dlh) / 2);

    // 一行 2×：天气名 + 气温℃ + 湿度（湿度红黄抖）
    snprintf(temp_s, sizeof(temp_s), "%s℃", now.temp.c_str());
    snprintf(hum_s, sizeof(hum_s), "%s%%", now.humidity.c_str());
    const char *name = now.text.c_str();
    const int gap = 10;
    int cx = tx;
    const int row1_y = content_y;
    const int row1_max_y = row1_y + dlh;
    if (name && name[0]) {
      fontDrawTextWrappedDouble(epd, cx, row1_y, tw - (cx - tx), row1_max_y, name, Theme::Alert);
      cx += largeTextWidth(name) * 2 + gap;
    }
    if (now.temp.length()) {
      fontDrawTextWrappedDouble(epd, cx, row1_y, imax(8, tw - (cx - tx)), row1_max_y, temp_s,
                                Theme::Alert);
      cx += largeTextWidth(temp_s) * 2 + gap;
    }
    if (now.humidity.length() && cx - tx < tw) {
      fontDrawTextWrappedDoubleDither(epd, cx, row1_y, imax(8, tw - (cx - tx)), row1_max_y, hum_s,
                                      Theme::Alert, Theme::Accent);
    }
    int ty = content_y + dlh + 4;

    // 次行：风向箭头 + 级数；空气质量（无 PM2.5）
    int meta_x = tx;
    const int wind_deg = windFromDeg(now);
    if (wind_deg >= 0) {
      drawWindArrow(epd, meta_x + 12, ty + lh / 2, wind_deg, Theme::Fg);
      meta_x += 28;
    }
    if (now.wind_scale.length()) {
      snprintf(line, sizeof(line), "%s级", now.wind_scale.c_str());
      const int ww = largeTextWidth(line);
      fontDrawTextWrapped(epd, meta_x, ty, ww + 4, ty + lh, line, Theme::Fg);
      meta_x += ww + 12;
    }
    if (now.aqi_category.length() || now.aqi.length()) {
      // 类别 + AQI 数值，不再加「空气」前缀
      snprintf(line, sizeof(line), "%s %s", now.aqi_category.c_str(), now.aqi.c_str());
      fontDrawTextWrapped(epd, meta_x, ty, imax(8, (tx + tw) - meta_x), ty + lh, line, Theme::Fg);
    }
    ty += lh + 2;

    if (loc_col > 0) {
      fontDrawTextWrappedDouble(epd, loc_x, loc_y, loc_col, loc_y + dlh, loc, Theme::Fg);
    }
    y = y + big + kNowBelowGap;
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
        cols[i].weekday = weekdayFromYmd(ymd_y, ymd_m, ymd_d);
        snprintf(cols[i].cap1, sizeof(cols[i].cap1), "%02d/%02d", ymd_m, ymd_d);
        snprintf(cols[i].cap2, sizeof(cols[i].cap2), "周%s", weekdayCn(cols[i].weekday));
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
        qweatherSourceIsApiKey()
            ? "天气未就绪：需和风 Host + API Key + 已选位置。\nWeb：打开 /weather 配置并选城市。"
            : "天气未就绪：需 Portal 登录 + 已选位置。\nWeb：/auth 登录，再打开 /weather 选城市。"
              "\n（Portal 模式下和风密钥在服务器）";
    drawErrorPage(epd, mode, msg, "cfg");
    return true;  // UI 已按 mode 刷新；BOOT 切换应看到标题变化，勿 ErrorBurst
  }

  bool fetched_ok = false;
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
      fetched_ok = ok_now || ok_h;
      const char *st = (!ok_now || !ok_h) ? "err" : "OK";
      weatherDrawNow(epd, g_now_buf, g_hourly_buf, st);
      break;
    }
    case WeatherMode::Week14:
    case WeatherMode::Week7: {
      const bool ok = weatherFetchDaily("7d", g_daily_buf);
      if (!ok) errorLog("weather", g_daily_buf.message.length() ? g_daily_buf.message : "daily 7d failed");
      fetched_ok = ok;
      weatherDrawForecast(epd, g_daily_buf, WeatherMode::Week7, ok ? "OK" : "err");
      break;
    }
    default:
      errorLog("weather", "unknown weather mode");
      return false;
  }
  if (fetched_ok) {
    const time_t now = time(nullptr);
    if (now >= 1700000000) saveWeatherLastUnix((uint32_t)now);
  }
  return true;
}
