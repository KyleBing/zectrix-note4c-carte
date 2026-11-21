#pragma once

#include <Arduino.h>
#include <time.h>

struct RtcDateTime {
  bool ok = false;
  int year = 0;    // 2000+
  int month = 0;   // 1–12
  int day = 0;     // 1–31
  int hour = 0;
  int minute = 0;
  int second = 0;
  int weekday = 0; // 0=Sun .. 6=Sat
};

// Call after I2C up: load RTC into ESP system clock if valid.
void rtcBegin();

bool rtcRead(RtcDateTime &out);
bool rtcWrite(const RtcDateTime &in);

// True if PCF8563 has valid time (VL clear).
bool rtcIsValid();

// Apply configured timezone to ESP time helpers (configTime offset).
void rtcApplyTimezone();

// Force NTP sync (needs STA). Updates RTC + last-sync stamp.
bool rtcSyncFromNtp(uint32_t timeout_ms = 8000);

// After WiFi connects: sync if boot/reset pending, RTC invalid, or interval elapsed.
// Default interval 7 days (configurable).
bool rtcSyncOnWifiConnected(uint32_t timeout_ms = 8000);

// Soft check (e.g. calendar): only if invalid or overdue; no boot force.
bool rtcSyncIfDue(uint32_t timeout_ms = 6000);

uint32_t rtcLastNtpUnix();  // 0 = never
int rtcDaysSinceNtp();      // -1 = unknown/never

// PCF8563 闹钟（INT → GPIO5）。时间均为本地墙钟。
bool rtcAlarmClear();                 // 清 AF/TF，关 AIE
bool rtcAlarmFired();                 // Control2.AF
bool rtcAlarmSetUnix(time_t wake);    // 设到 wake（本地）；失败返回 false
bool rtcAlarmSetLocal(int year, int month, int day, int hour, int minute);
