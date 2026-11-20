#pragma once

#include <Arduino.h>

#include "app_config.h"

// 设备端 App 列表；上下短按切换（跳过未启用）。
enum class AppId : uint8_t {
  Todo = 0,
  Calendar,
  Weather,
  Album,
  Count,
};

inline uint8_t appsEnabledDefaultMask() {
  return static_cast<uint8_t>((1u << static_cast<int>(AppId::Count)) - 1u);
}

inline bool appIdEnabled(AppId id) {
  const int i = static_cast<int>(id);
  if (i < 0 || i >= static_cast<int>(AppId::Count)) return false;
  const uint8_t mask = getAppConfig().apps_enabled;
  return (mask & (1u << i)) != 0;
}

inline const char *appIdName(AppId id) {
  switch (id) {
    case AppId::Todo:
      return "TODO";
    case AppId::Calendar:
      return "CALENDAR";
    case AppId::Weather:
      return "WEATHER";
    case AppId::Album:
      return "ALBUM";
    default:
      return "?";
  }
}

inline const char *appIdLabelZh(AppId id) {
  switch (id) {
    case AppId::Todo:
      return "待办";
    case AppId::Calendar:
      return "日历";
    case AppId::Weather:
      return "天气";
    case AppId::Album:
      return "相册";
    default:
      return "?";
  }
}

inline const char *appIdHintZh(AppId id) {
  switch (id) {
    case AppId::Todo:
      return "Portal 待办列表";
    case AppId::Calendar:
      return "月历 / 农历";
    case AppId::Weather:
      return "实况与预报";
    case AppId::Album:
      return "墨水屏相册";
    default:
      return "";
  }
}

inline AppId appIdFirstEnabled() {
  const int n = static_cast<int>(AppId::Count);
  for (int i = 0; i < n; i++) {
    const AppId id = static_cast<AppId>(i);
    if (appIdEnabled(id)) return id;
  }
  return AppId::Todo;
}

inline AppId appIdNext(AppId id) {
  const int n = static_cast<int>(AppId::Count);
  AppId cur = id;
  for (int step = 0; step < n; step++) {
    cur = static_cast<AppId>((static_cast<int>(cur) + 1) % n);
    if (appIdEnabled(cur)) return cur;
  }
  return id;
}

inline AppId appIdPrev(AppId id) {
  const int n = static_cast<int>(AppId::Count);
  AppId cur = id;
  for (int step = 0; step < n; step++) {
    cur = static_cast<AppId>((static_cast<int>(cur) + n - 1) % n);
    if (appIdEnabled(cur)) return cur;
  }
  return id;
}

inline int appIdEnabledCount() {
  int c = 0;
  const int n = static_cast<int>(AppId::Count);
  for (int i = 0; i < n; i++) {
    if (appIdEnabled(static_cast<AppId>(i))) c++;
  }
  return c;
}
