#pragma once

#include <Arduino.h>
#include "epd_ssd2683.h"

enum class BatteryCharge : uint8_t {
  Discharging = 0,
  Charging,
  Full,
};

struct BatteryStatus {
  bool ok = false;
  uint16_t mv = 0;      // pack voltage
  uint8_t percent = 0;  // 0–100
  BatteryCharge charge = BatteryCharge::Discharging;
};

void batteryBegin();
BatteryStatus batteryRead();
const char *batteryChargeLabel(BatteryCharge c);
// 充电中或已充满（通常表示 USB/电源已接入）
bool batteryOnPower();

// 在 loop/powerPoll 中调用：跟踪拔电时刻（NVS）。
void batteryPoll();
// 已断开电源秒数；0=当前接电；-1=未知（无有效时间戳）
int32_t batteryUnpluggedSec();

// Header battery icon only (no percent text) on black bar, right-aligned.
// Returns leftmost x used (so callers can avoid overlap).
int batteryDrawHeader(EpdSsd2683 &epd, int right_x = 392, int y = 5);
