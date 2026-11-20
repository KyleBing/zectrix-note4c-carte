#include "app_battery.h"
#include "board.h"

// Matches LazyYoun zectrix-s3-epaper-4.2 ReadBatteryStatus curve + 1:1 divider (*2).
// Header icon geometry mirrors Cardputer drawIconBattery / drawIconChargingBolt.

void batteryBegin() {
  // CHRG is open-drain active-LOW; need pull-up or it floats and always looks "charging".
  pinMode(PIN_CHRG, INPUT_PULLUP);
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
  // Official NOTE4C battery curve (same integers as reference firmware).
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

// Same constants / layout as Cardputer app_icons.cpp drawIconBattery.
static constexpr int kSegs = 5;
static constexpr int kSegW = 3;
static constexpr int kSegH = 8;
static constexpr int kSegGap = 1;
static constexpr int kBorder = 1;
static constexpr int kInnerGap = 1;
static constexpr int kHeadW = 2;
static constexpr int kHeadH = 4;
static constexpr int kBoltZoneW = 6;
static constexpr int kBoltGap = 1;

static int batteryInset() { return kBorder + kInnerGap; }
static int batteryInnerW() { return kSegs * kSegW + (kSegs - 1) * kSegGap; }
static int batteryBodyW() { return batteryInnerW() + batteryInset() * 2; }
static int batteryBodyH() { return kSegH + batteryInset() * 2; }
static int batteryIndicatorW() { return kHeadW + batteryBodyW(); }

// Cardputer drawIconChargingBolt — yellow zigzag from two triangles.
static void drawChargingBolt(EpdSsd2683 &epd, int zone_x, int y, int body_h) {
  const int cx = zone_x + kBoltZoneW / 2;
  const int cy = y + body_h / 2;
  const int s = body_h <= 12 ? 1 : 0;
  epd.fillTriangle(cx + 1 - s, cy - 4 + s, cx - 2, cy + 1, cx, cy + 1, EPD_YELLOW);
  epd.fillTriangle(cx - 1 + s, cy + 4 - s, cx + 2, cy - 1, cx, cy - 1, EPD_YELLOW);
}

int batteryDrawHeader(EpdSsd2683 &epd, int right_x, int y) {
  const BatteryStatus bat = batteryRead();
  const bool charging = bat.charge == BatteryCharge::Charging;
  // Cardputer: charging → GREEN body + YELLOW bolt. EPD has no green → YELLOW body.
  const EpdColor accent = charging ? EPD_YELLOW : EPD_WHITE;

  const int body_w = batteryBodyW();
  const int body_h = batteryBodyH();
  const int bolt_offset = charging ? (kBoltZoneW + kBoltGap) : 0;
  const int total_w = batteryIndicatorW() + bolt_offset;
  const int x = right_x - total_w;  // left edge of [bolt?][head][body]

  // Clear strip (header bg is already black; keep for parity with Cardputer)
  epd.fillRect(x, y, total_w, body_h, EPD_BLACK);

  if (charging) {
    drawChargingBolt(epd, x, y, body_h);
  }

  // 正极朝左：head | body
  const int battery_x = x + bolt_offset;
  const int head_x = battery_x;
  const int body_x = battery_x + kHeadW;
  const int head_y = y + (body_h - kHeadH) / 2;
  epd.fillRect(head_x, head_y, kHeadW, kHeadH, accent);
  epd.drawRect(body_x, y, body_w, body_h, accent);

  int filled = 0;
  if (bat.ok) {
    filled = (bat.percent + 19) / 20;
    if (filled > kSegs) filled = kSegs;
    if (filled < 0) filled = 0;
  }
  const int inset = batteryInset();
  const int seg_x0 = body_x + inset;
  const int seg_y = y + inset;
  for (int i = 0; i < kSegs; i++) {
    // 从远离正极一侧开始亮（正极在左 → 从右往左填）
    if (i >= kSegs - filled) {
      const int sx = seg_x0 + i * (kSegW + kSegGap);
      epd.fillRect(sx, seg_y, kSegW, kSegH, accent);
    }
  }

  return x;
}
