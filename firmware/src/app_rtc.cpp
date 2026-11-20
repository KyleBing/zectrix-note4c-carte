#include "app_rtc.h"
#include "app_config.h"
#include "board.h"

#include <Preferences.h>
#include <Wire.h>
#include <WiFi.h>
#include <sys/time.h>
#include <time.h>

static Preferences g_rtc_prefs;
static bool g_boot_sync_pending = true;

static uint8_t bcdToBin(uint8_t v) { return (uint8_t)((v >> 4) * 10 + (v & 0x0F)); }
static uint8_t binToBcd(uint8_t v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

static int weekdayFromYmd(int y, int m, int d) {
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

static void applySystemFromRtc(const RtcDateTime &dt) {
  if (!dt.ok) return;
  // RTC holds local wall-clock; mktime uses timezone from configTime().
  struct tm t = {};
  t.tm_year = dt.year - 1900;
  t.tm_mon = dt.month - 1;
  t.tm_mday = dt.day;
  t.tm_hour = dt.hour;
  t.tm_min = dt.minute;
  t.tm_sec = dt.second;
  t.tm_isdst = -1;
  const time_t epoch = mktime(&t);
  if (epoch < 0) return;
  const timeval tv = {.tv_sec = epoch, .tv_usec = 0};
  settimeofday(&tv, nullptr);
}

static bool readPcf8563(RtcDateTime &out) {
  Wire.beginTransmission(I2C_ADDR_RTC);
  Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)I2C_ADDR_RTC, 7) != 7) return false;

  const uint8_t sec = Wire.read();
  const uint8_t min = Wire.read();
  const uint8_t hour = Wire.read();
  const uint8_t day = Wire.read();
  const uint8_t wday = Wire.read();
  const uint8_t month = Wire.read();
  const uint8_t year = Wire.read();

  if (sec & 0x80) return false;  // VL

  out.second = bcdToBin(sec & 0x7F);
  out.minute = bcdToBin(min & 0x7F);
  out.hour = bcdToBin(hour & 0x3F);
  out.day = bcdToBin(day & 0x3F);
  out.weekday = (int)(wday & 0x07);
  out.month = bcdToBin(month & 0x1F);
  out.year = 2000 + bcdToBin(year);
  if (out.month < 1 || out.month > 12 || out.day < 1 || out.day > 31 || out.year < 2000) {
    return false;
  }
  out.ok = true;
  return true;
}

static bool readSystemTime(RtcDateTime &out) {
  time_t now = time(nullptr);
  if (now < 1700000000) return false;
  struct tm t;
  if (!localtime_r(&now, &t)) return false;
  out.year = t.tm_year + 1900;
  out.month = t.tm_mon + 1;
  out.day = t.tm_mday;
  out.hour = t.tm_hour;
  out.minute = t.tm_min;
  out.second = t.tm_sec;
  out.weekday = t.tm_wday;
  out.ok = true;
  return true;
}

static void markNtpDone() {
  const time_t now = time(nullptr);
  if (now > 1700000000) {
    g_rtc_prefs.putULong("ntp_at", (uint32_t)now);
  }
  g_boot_sync_pending = false;
}

static bool intervalDue() {
  const uint32_t last = g_rtc_prefs.getULong("ntp_at", 0);
  if (last == 0) return true;
  const time_t now = time(nullptr);
  if (now < 1700000000) return true;  // no reliable "now" yet → try
  uint16_t days = getAppConfig().ntp_interval_days;
  if (days == 0) days = 30;
  if (days > 365) days = 365;
  return (uint32_t)now >= last + (uint32_t)days * 86400UL;
}

void rtcApplyTimezone() {
  const int32_t off = (int32_t)getAppConfig().tz_offset_min * 60;
  configTime(off, 0, "ntp.aliyun.com", "ntp.tencent.com", "pool.ntp.org");
}

void rtcBegin() {
  g_rtc_prefs.begin("rtc", false);
  g_boot_sync_pending = true;
  rtcApplyTimezone();
  RtcDateTime dt;
  if (readPcf8563(dt)) {
    applySystemFromRtc(dt);
    Serial.printf("[RTC] hydrate %04d-%02d-%02d %02d:%02d:%02d\n", dt.year, dt.month, dt.day,
                  dt.hour, dt.minute, dt.second);
  } else {
    Serial.println("[RTC] invalid / unset (will sync after WiFi)");
  }
}

bool rtcIsValid() {
  RtcDateTime dt;
  return readPcf8563(dt);
}

bool rtcRead(RtcDateTime &out) {
  out = RtcDateTime{};
  if (readPcf8563(out)) return true;
  return readSystemTime(out);
}

bool rtcWrite(const RtcDateTime &in) {
  if (!in.ok || in.year < 2000 || in.month < 1 || in.month > 12 || in.day < 1 || in.day > 31) {
    return false;
  }
  RtcDateTime dt = in;
  if (dt.weekday < 0 || dt.weekday > 6) {
    dt.weekday = weekdayFromYmd(dt.year, dt.month, dt.day);
  }
  Wire.beginTransmission(I2C_ADDR_RTC);
  Wire.write(0x02);
  Wire.write(binToBcd((uint8_t)dt.second) & 0x7F);
  Wire.write(binToBcd((uint8_t)dt.minute));
  Wire.write(binToBcd((uint8_t)dt.hour));
  Wire.write(binToBcd((uint8_t)dt.day));
  Wire.write((uint8_t)(dt.weekday & 0x07));
  Wire.write(binToBcd((uint8_t)dt.month));
  Wire.write(binToBcd((uint8_t)(dt.year - 2000)));
  if (Wire.endTransmission(true) != 0) return false;
  applySystemFromRtc(dt);
  return true;
}

bool rtcSyncFromNtp(uint32_t timeout_ms) {
  if (WiFi.status() != WL_CONNECTED) return false;
  rtcApplyTimezone();

  struct tm t;
  const uint32_t deadline = millis() + timeout_ms;
  while ((int32_t)(millis() - deadline) < 0) {
    if (getLocalTime(&t, 200)) {
      RtcDateTime dt;
      dt.ok = true;
      dt.year = t.tm_year + 1900;
      dt.month = t.tm_mon + 1;
      dt.day = t.tm_mday;
      dt.hour = t.tm_hour;
      dt.minute = t.tm_min;
      dt.second = t.tm_sec;
      dt.weekday = t.tm_wday;
      const bool wrote = rtcWrite(dt);
      markNtpDone();
      Serial.printf("[RTC] NTP %04d-%02d-%02d %02d:%02d:%02d wd=%d write=%d\n", dt.year, dt.month,
                    dt.day, dt.hour, dt.minute, dt.second, dt.weekday, (int)wrote);
      return wrote;
    }
    delay(50);
  }
  Serial.println("[RTC] NTP sync timeout");
  return false;
}

bool rtcSyncOnWifiConnected(uint32_t timeout_ms) {
  const bool force = g_boot_sync_pending || !rtcIsValid() || intervalDue();
  if (!force) {
    Serial.printf("[RTC] skip NTP (last=%u days_ago=%d interval=%u)\n",
                  (unsigned)rtcLastNtpUnix(), rtcDaysSinceNtp(),
                  (unsigned)getAppConfig().ntp_interval_days);
    return false;
  }
  Serial.printf("[RTC] NTP due (boot=%d valid=%d interval_due=%d)\n", (int)g_boot_sync_pending,
                (int)rtcIsValid(), (int)intervalDue());
  return rtcSyncFromNtp(timeout_ms);
}

bool rtcSyncIfDue(uint32_t timeout_ms) {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (rtcIsValid() && !intervalDue()) return false;
  return rtcSyncFromNtp(timeout_ms);
}

uint32_t rtcLastNtpUnix() { return g_rtc_prefs.getULong("ntp_at", 0); }

int rtcDaysSinceNtp() {
  const uint32_t last = rtcLastNtpUnix();
  if (last == 0) return -1;
  const time_t now = time(nullptr);
  if (now < 1700000000) return -1;
  return (int)(((uint32_t)now - last) / 86400UL);
}
