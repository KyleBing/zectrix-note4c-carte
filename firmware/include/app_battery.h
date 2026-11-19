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

// Cardputer-style header battery: icon + percent on black bar (right-aligned).
// Returns leftmost x used (so callers can avoid overlap).
int batteryDrawHeader(EpdSsd2683 &epd, int right_x = 392, int y = 5);
