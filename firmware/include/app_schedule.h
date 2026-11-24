#pragma once

#include "app_apps.h"
#include "app_config.h"

#include <Arduino.h>
#include <ctime>

// 当天分钟数 0..1439；跨午夜时段 start>end 视为过夜。
bool scheduleInQuietNow();
bool scheduleInQuietAtMin(int minute_of_day);

// 若 wake 落在静默时段，推迟到该时段结束（同一天或次日）。
time_t scheduleClampWakeOutOfQuiet(time_t wake);

// 按每日时间表解析「此刻应前台」的 App / 界面。view：天气 0=实况 1=预报，其它为 0。
bool scheduleActiveApp(AppId &out_app, uint8_t &out_view);

// HH:MM 解析；非法返回 false。
bool scheduleParseHm(const String &s, uint16_t &out_min);
void scheduleFormatHm(uint16_t minute, char *buf, size_t n);
