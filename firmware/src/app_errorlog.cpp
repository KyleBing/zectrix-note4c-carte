#include "app_errorlog.h"

#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <ctime>

#include "app_rtc.h"

static constexpr const char *kPath = "/logs/errors.json";
static constexpr const char *kDir = "/logs";

static ErrorLogEntry g_entries[ERROR_LOG_MAX];
static int g_count = 0;   // number of valid entries
static int g_head = 0;    // index of newest in ring
static bool g_ready = false;

static uint32_t nowUnix() {
  RtcDateTime dt;
  if (rtcRead(dt) && dt.ok && dt.year >= 2024) {
    struct tm t = {};
    t.tm_year = dt.year - 1900;
    t.tm_mon = dt.month - 1;
    t.tm_mday = dt.day;
    t.tm_hour = dt.hour;
    t.tm_min = dt.minute;
    t.tm_sec = dt.second;
    const time_t sec = mktime(&t);
    if (sec > 0) return (uint32_t)sec;
  }
  const time_t now = time(nullptr);
  if (now > 1700000000) return (uint32_t)now;
  return 0;
}

static void copyTrunc(char *dst, size_t n, const char *src) {
  if (!dst || n == 0) return;
  if (!src) {
    dst[0] = '\0';
    return;
  }
  strncpy(dst, src, n - 1);
  dst[n - 1] = '\0';
}

static bool ensureDir() {
  if (LittleFS.exists(kDir)) return true;
  return LittleFS.mkdir(kDir);
}

static bool saveToFs() {
  if (!g_ready) return false;
  if (!ensureDir()) return false;

  JsonDocument doc;
  JsonArray arr = doc["entries"].to<JsonArray>();
  // oldest → newest for file readability
  for (int i = g_count - 1; i >= 0; i--) {
    ErrorLogEntry e;
    if (!errorLogGet(i, e)) continue;
    JsonObject o = arr.add<JsonObject>();
    o["t"] = e.unix_t;
    o["up"] = e.uptime_ms;
    o["src"] = e.src;
    o["msg"] = e.msg;
  }

  File f = LittleFS.open(kPath, "w");
  if (!f) {
    Serial.println("[ErrLog] open write failed");
    return false;
  }
  const size_t n = serializeJson(doc, f);
  f.close();
  Serial.printf("[ErrLog] saved %d entries (%u bytes)\n", g_count, (unsigned)n);
  return n > 0;
}

static void pushEntry(const ErrorLogEntry &e) {
  if (g_count < ERROR_LOG_MAX) {
    g_entries[g_count] = e;
    g_head = g_count;
    g_count++;
  } else {
    g_head = (g_head + 1) % ERROR_LOG_MAX;
    g_entries[g_head] = e;
  }
}

static void loadFromFs() {
  g_count = 0;
  g_head = 0;
  if (!LittleFS.exists(kPath)) return;

  File f = LittleFS.open(kPath, "r");
  if (!f) return;
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.printf("[ErrLog] load JSON %s\n", err.c_str());
    return;
  }

  JsonArray arr = doc["entries"].as<JsonArray>();
  if (arr.isNull()) return;
  for (JsonObject o : arr) {
    ErrorLogEntry e;
    e.unix_t = o["t"] | 0u;
    e.uptime_ms = o["up"] | 0u;
    copyTrunc(e.src, sizeof(e.src), o["src"] | "");
    copyTrunc(e.msg, sizeof(e.msg), o["msg"] | "");
    if (!e.msg[0]) continue;
    pushEntry(e);
  }
  Serial.printf("[ErrLog] loaded %d entries\n", g_count);
}

bool errorLogBegin() {
  loadFromFs();
  g_ready = true;
  return true;
}

void errorLog(const char *src, const char *msg) {
  if (!msg || !msg[0]) return;

  ErrorLogEntry e;
  e.unix_t = nowUnix();
  e.uptime_ms = millis();
  copyTrunc(e.src, sizeof(e.src), src && src[0] ? src : "app");
  copyTrunc(e.msg, sizeof(e.msg), msg);
  pushEntry(e);

  Serial.printf("[ErrLog] %s: %s\n", e.src, e.msg);
  if (g_ready) saveToFs();
}

void errorLog(const char *src, const String &msg) { errorLog(src, msg.c_str()); }

int errorLogCount() { return g_count; }

bool errorLogGet(int newest_index, ErrorLogEntry &out) {
  if (newest_index < 0 || newest_index >= g_count) return false;
  int idx;
  if (g_count < ERROR_LOG_MAX) {
    idx = (g_count - 1) - newest_index;
  } else {
    idx = g_head - newest_index;
    while (idx < 0) idx += ERROR_LOG_MAX;
    idx %= ERROR_LOG_MAX;
  }
  out = g_entries[idx];
  return out.msg[0] != '\0';
}

bool errorLogClear() {
  g_count = 0;
  g_head = 0;
  if (LittleFS.exists(kPath)) LittleFS.remove(kPath);
  Serial.println("[ErrLog] cleared");
  return true;
}

static void formatWhen(const ErrorLogEntry &e, char *buf, size_t n) {
  if (e.unix_t >= 1700000000) {
    const time_t t = (time_t)e.unix_t;
    struct tm tm_info;
    if (localtime_r(&t, &tm_info)) {
      snprintf(buf, n, "%04d-%02d-%02d %02d:%02d:%02d", tm_info.tm_year + 1900, tm_info.tm_mon + 1,
               tm_info.tm_mday, tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec);
      return;
    }
  }
  const uint32_t s = e.uptime_ms / 1000u;
  snprintf(buf, n, "开机+%u:%02u:%02u", (unsigned)(s / 3600u), (unsigned)((s % 3600u) / 60u),
           (unsigned)(s % 60u));
}

static void htmlEscapeAppend(String &out, const char *s) {
  if (!s) return;
  for (const char *p = s; *p; ++p) {
    if (*p == '&') out += F("&amp;");
    else if (*p == '<') out += F("&lt;");
    else if (*p == '>') out += F("&gt;");
    else if (*p == '"') out += F("&quot;");
    else out += *p;
  }
}

void errorLogAppendHtml(String &out) {
  if (g_count <= 0) {
    out += F("<p class=hint style=margin:0>暂无错误记录。</p>");
    return;
  }
  out += F("<ul class=err-list>");
  for (int i = 0; i < g_count; i++) {
    ErrorLogEntry e;
    if (!errorLogGet(i, e)) continue;
    char when[40];
    formatWhen(e, when, sizeof(when));
    out += F("<li><div class=err-top><span class=err-src>");
    htmlEscapeAppend(out, e.src);
    out += F("</span><span class=err-when>");
    out += when;
    out += F("</span></div><pre class=err-msg>");
    htmlEscapeAppend(out, e.msg);
    out += F("</pre></li>");
  }
  out += F("</ul>");
}
