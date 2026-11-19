#pragma once

#include <Arduino.h>

// 设备端 App 列表；上下短按切换。
enum class AppId : uint8_t {
  Todo = 0,
  Weather,
  Album,
  Count,
};

inline const char *appIdName(AppId id) {
  switch (id) {
    case AppId::Todo:
      return "TODO";
    case AppId::Weather:
      return "WEATHER";
    case AppId::Album:
      return "ALBUM";
    default:
      return "?";
  }
}

inline AppId appIdNext(AppId id) {
  const int n = static_cast<int>(AppId::Count);
  return static_cast<AppId>((static_cast<int>(id) + 1) % n);
}

inline AppId appIdPrev(AppId id) {
  const int n = static_cast<int>(AppId::Count);
  return static_cast<AppId>((static_cast<int>(id) + n - 1) % n);
}
