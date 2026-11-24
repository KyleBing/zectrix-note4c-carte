#include "app_schedule.h"
#include "app_rtc.h"

#include <cstdio>
#include <cstring>

bool scheduleParseHm(const String &s, uint16_t &out_min) {
  out_min = 0;
  String t = s;
  t.trim();
  if (t.isEmpty()) return false;
  int h = -1, m = -1;
  if (sscanf(t.c_str(), "%d:%d", &h, &m) != 2) return false;
  if (h < 0 || h > 23 || m < 0 || m > 59) return false;
  out_min = (uint16_t)(h * 60 + m);
  return true;
}

void scheduleFormatHm(uint16_t minute, char *buf, size_t n) {
  if (!buf || n == 0) return;
  const int h = (int)(minute / 60) % 24;
  const int m = (int)(minute % 60);
  snprintf(buf, n, "%02d:%02d", h, m);
}

static bool minuteInQuietSlot(int minute_of_day, const QuietSlot &s) {
  if (s.start_min == s.end_min) return false;
  if (s.start_min < s.end_min) {
    return minute_of_day >= (int)s.start_min && minute_of_day < (int)s.end_min;
  }
  // 跨午夜：如 22:00–06:00
  return minute_of_day >= (int)s.start_min || minute_of_day < (int)s.end_min;
}

bool scheduleInQuietAtMin(int minute_of_day) {
  const AppConfig &cfg = getAppConfig();
  if (!cfg.quiet_en || cfg.quiet_count == 0) return false;
  if (minute_of_day < 0) minute_of_day = 0;
  if (minute_of_day > 1439) minute_of_day %= 1440;
  for (uint8_t i = 0; i < cfg.quiet_count && i < QUIET_SLOT_MAX; i++) {
    if (minuteInQuietSlot(minute_of_day, cfg.quiet_slots[i])) return true;
  }
  return false;
}

bool scheduleInQuietNow() {
  RtcDateTime now;
  if (!rtcRead(now)) {
    const time_t t = time(nullptr);
    if (t < 1700000000) return false;
    struct tm tm_now;
    if (!localtime_r(&t, &tm_now)) return false;
    return scheduleInQuietAtMin(tm_now.tm_hour * 60 + tm_now.tm_min);
  }
  return scheduleInQuietAtMin((int)now.hour * 60 + (int)now.minute);
}

time_t scheduleClampWakeOutOfQuiet(time_t wake) {
  if (wake <= 0) return wake;
  const AppConfig &cfg = getAppConfig();
  if (!cfg.quiet_en || cfg.quiet_count == 0) return wake;

  for (int guard = 0; guard < 8; guard++) {
    struct tm tm_w;
    if (!localtime_r(&wake, &tm_w)) return wake;
    const int m = tm_w.tm_hour * 60 + tm_w.tm_min;
    if (!scheduleInQuietAtMin(m)) return wake;

    // 推到当前命中时段的结束
    int end_min = -1;
    for (uint8_t i = 0; i < cfg.quiet_count && i < QUIET_SLOT_MAX; i++) {
      const QuietSlot &s = cfg.quiet_slots[i];
      if (!minuteInQuietSlot(m, s)) continue;
      end_min = (int)s.end_min;
      break;
    }
    if (end_min < 0) return wake;
    tm_w.tm_hour = end_min / 60;
    tm_w.tm_min = end_min % 60;
    tm_w.tm_sec = 0;
    tm_w.tm_isdst = -1;
    time_t next = mktime(&tm_w);
    if (next <= wake) next += 86400;  // 跨午夜结束落在「明天」同一时钟
    wake = next;
  }
  return wake;
}

bool scheduleActiveApp(AppId &out_app, uint8_t &out_view) {
  out_app = AppId::Todo;
  out_view = 0;
  const AppConfig &cfg = getAppConfig();
  if (!cfg.app_sched_en || cfg.app_sched_count == 0) return false;

  RtcDateTime now;
  int minute = 0;
  if (rtcRead(now)) {
    minute = (int)now.hour * 60 + (int)now.minute;
  } else {
    const time_t t = time(nullptr);
    if (t < 1700000000) return false;
    struct tm tm_now;
    if (!localtime_r(&t, &tm_now)) return false;
    minute = tm_now.tm_hour * 60 + tm_now.tm_min;
  }

  int best_i = -1;
  int best_at = -1;
  for (uint8_t i = 0; i < cfg.app_sched_count && i < APP_SCHED_MAX; i++) {
    const AppSchedSlot &s = cfg.app_sched[i];
    if (s.app_id >= static_cast<uint8_t>(AppId::Count)) continue;
    if (!appIdEnabled(static_cast<AppId>(s.app_id))) continue;
    if ((int)s.at_min > minute) continue;
    if ((int)s.at_min >= best_at) {
      best_at = (int)s.at_min;
      best_i = (int)i;
    }
  }
  // 若今天还没有任何点，取昨天跨过来的最晚点（日表中最大 at_min）
  if (best_i < 0) {
    for (uint8_t i = 0; i < cfg.app_sched_count && i < APP_SCHED_MAX; i++) {
      const AppSchedSlot &s = cfg.app_sched[i];
      if (s.app_id >= static_cast<uint8_t>(AppId::Count)) continue;
      if (!appIdEnabled(static_cast<AppId>(s.app_id))) continue;
      if ((int)s.at_min >= best_at) {
        best_at = (int)s.at_min;
        best_i = (int)i;
      }
    }
  }
  if (best_i < 0) return false;
  out_app = static_cast<AppId>(cfg.app_sched[best_i].app_id);
  out_view = cfg.app_sched[best_i].view;
  return true;
}
