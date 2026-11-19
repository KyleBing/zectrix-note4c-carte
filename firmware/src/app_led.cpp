#include "app_led.h"
#include "board.h"

static LedPattern g_pat = LedPattern::Off;
static uint32_t g_t0 = 0;

static inline void ledWrite(bool on) {
  digitalWrite(PIN_LED, on ? LED_ON_LEVEL : LED_OFF_LEVEL);
}

void ledBegin() {
  pinMode(PIN_LED, OUTPUT);
  ledWrite(false);
  g_pat = LedPattern::Off;
  g_t0 = millis();
}

void ledOff() {
  g_pat = LedPattern::Off;
  ledWrite(false);
}

void ledSet(LedPattern pattern) {
  if (g_pat == pattern) return;
  g_pat = pattern;
  g_t0 = millis();
  if (pattern == LedPattern::Off) {
    ledWrite(false);
  } else if (pattern == LedPattern::On) {
    ledWrite(true);
  }
}

LedPattern ledGet() { return g_pat; }

void ledUpdate() {
  const uint32_t elapsed = millis() - g_t0;

  switch (g_pat) {
    case LedPattern::Off:
      ledWrite(false);
      break;
    case LedPattern::On:
      ledWrite(true);
      break;
    case LedPattern::BlinkSlow:
      ledWrite(((elapsed / 400) % 2) == 0);
      break;
    case LedPattern::BlinkFast:
      ledWrite(((elapsed / 120) % 2) == 0);
      break;
    case LedPattern::DoublePulse: {
      const uint32_t cycle = elapsed % 940;
      const bool on = (cycle < 80) || (cycle >= 160 && cycle < 240);
      ledWrite(on);
      break;
    }
    case LedPattern::ErrorBurst: {
      const uint32_t cycle = elapsed % 1020;
      bool on = false;
      if (cycle < 70) on = true;
      else if (cycle >= 140 && cycle < 210) on = true;
      else if (cycle >= 280 && cycle < 350) on = true;
      ledWrite(on);
      break;
    }
  }
}
