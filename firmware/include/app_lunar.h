#pragma once

#include <Arduino.h>

struct LunarDate {
  bool ok = false;
  int year = 0;       // 农历年
  int month = 0;      // 1–12
  int day = 0;        // 1–30
  bool leap = false;  // 闰月
};

// 公历 → 农历（约 1900–2100）。失败时 ok=false。
bool lunarFromSolar(int y, int m, int d, LunarDate &out);

// 写入农历日短名：初一/初二…/廿十/三十；月初一可写正月/二月…（month_name_on_first）。
// buf 建议 ≥ 8 字节（UTF-8）。
void lunarDayLabel(const LunarDate &lun, char *buf, size_t n, bool month_name_on_first = true);
