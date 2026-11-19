#include "app_battery.h"
#include "board.h"

// Matches LazyYoun zectrix-s3-epaper-4.2 ReadBatteryStatus curve + 1:1 divider (*2).

void batteryBegin() {
  pinMode(PIN_CHRG, INPUT);
  pinMode(PIN_STDBY, INPUT);
  analogReadResolution(12);
#if defined(ADC_11db)
  analogSetPinAttenuation(PIN_BAT_ADC, ADC_11db);
#endif
}

static uint16_t readPackMv() {
  // Average a few samples; analogReadMilliVolts uses ESP calibration when available.
  int sum = 0;
  int n = 0;
  for (int i = 0; i < 10; i++) {
    const int mv = analogReadMilliVolts(PIN_BAT_ADC);
    if (mv > 0) {
      sum += mv;
      n++;
    }
    delay(2);
  }
  if (n == 0) return 0;
  // Board resistor divider ≈ 1/2
  return static_cast<uint16_t>((sum / n) * 2);
}

static uint8_t percentFromMv(uint16_t mv) {
  // Official NOTE4C curve (same integers as reference firmware).
  int p = (-1 * (int)mv * (int)mv + 9016 * (int)mv - 19189000) / 10000;
  if (p > 100) p = 100;
  if (p < 0) p = 0;
  return static_cast<uint8_t>(p);
}

static BatteryCharge readCharge() {
  // CHRG_L: low = charging; STDBY_H: high = full
  const bool charging = digitalRead(PIN_CHRG) == LOW;
  const bool full = digitalRead(PIN_STDBY) == HIGH;
  if (full && !charging) return BatteryCharge::Full;
  if (full) return BatteryCharge::Full;
  if (charging) return BatteryCharge::Charging;
  return BatteryCharge::Discharging;
}

BatteryStatus batteryRead() {
  BatteryStatus s;
  s.mv = readPackMv();
  if (s.mv == 0) {
    s.ok = false;
    s.percent = 0;
    s.charge = BatteryCharge::Discharging;
    return s;
  }
  s.ok = true;
  s.percent = percentFromMv(s.mv);
  s.charge = readCharge();
  if (s.charge == BatteryCharge::Full) s.percent = 100;
  return s;
}

const char *batteryChargeLabel(BatteryCharge c) {
  switch (c) {
    case BatteryCharge::Charging:
      return "充电中";
    case BatteryCharge::Full:
      return "已充满";
    default:
      return "放电中";
  }
}

static void drawBolt(EpdSsd2683 &epd, int x, int y, EpdColor fg) {
  // Compact lightning bolt (~6x10) for charging
  static const uint8_t bits[10] = {
      0b00110000, 0b00110000, 0b01100000, 0b01100000, 0b11111100,
      0b00110000, 0b00110000, 0b00011000, 0b00011000, 0b00001000,
  };
  for (int row = 0; row < 10; row++) {
    for (int col = 0; col < 8; col++) {
      if (bits[row] & (0x80 >> col)) epd.setPixel(x + col, y + row, fg);
    }
  }
}

int batteryDrawHeader(EpdSsd2683 &epd, int right_x, int y) {
  // Icon only (no percent) — cleaner for device photos / screenshots.
  const BatteryStatus bat = batteryRead();
  const EpdColor accent = EPD_WHITE;

  // Battery body (cardputer-like: head on left, 5 segments)
  // inner = 5*3 + 4*1 = 19 → body_w = 19 + 2*inset = 23
  constexpr int body_w = 23;
  constexpr int body_h = 12;
  constexpr int head_w = 2;
  constexpr int head_h = 6;
  constexpr int segs = 5;
  constexpr int seg_w = 3;
  constexpr int seg_h = 8;
  constexpr int seg_gap = 1;
  constexpr int inset = 2;

  int icon_right = right_x;
  const bool charging = bat.charge == BatteryCharge::Charging;
  if (charging) {
    drawBolt(epd, icon_right - 7, y + 1, accent);
    icon_right -= 9;
  }

  const int body_x = icon_right - body_w;
  const int head_x = body_x - head_w;
  const int head_y = y + (body_h - head_h) / 2;
  epd.fillRect(head_x, head_y, head_w, head_h, accent);
  epd.drawRect(body_x, y, body_w, body_h, accent);

  int filled = 0;
  if (bat.ok) {
    filled = (bat.percent + 19) / 20;  // 0..5
    if (filled > segs) filled = segs;
  }
  const int seg_x0 = body_x + inset;
  const int seg_y = y + inset;
  for (int i = 0; i < segs; i++) {
    // fill from the end opposite the head (right → left)
    if (i >= segs - filled) {
      const int sx = seg_x0 + i * (seg_w + seg_gap);
      epd.fillRect(sx, seg_y, seg_w, seg_h, accent);
    }
  }

  return head_x;
}
