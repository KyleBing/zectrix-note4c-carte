#include "app_led.h"
#include "board.h"

#include <driver/gpio.h>

static LedPattern g_pat = LedPattern::Off;
static uint32_t g_t0 = 0;
static int g_last_level = -1;

static inline void ledWrite(bool on) {
  const int level = on ? LED_ON_LEVEL : LED_OFF_LEVEL;
  if (level == g_last_level) return;
  // Official NOTE4C BSP uses gpio_hold; must release before driving.
  gpio_hold_dis((gpio_num_t)PIN_LED);
  digitalWrite(PIN_LED, level);
  g_last_level = level;
}

void ledBegin() {
  gpio_hold_dis((gpio_num_t)PIN_LED);
  gpio_reset_pin((gpio_num_t)PIN_LED);
  pinMode(PIN_LED, OUTPUT);
  // Active-low: HIGH = off (matches zectrix board_power_bsp.cc)
  g_last_level = -1;
  ledWrite(false);
  g_pat = LedPattern::Off;
  g_t0 = millis();
  Serial.printf("[LED] GPIO%d idle=OFF (level=%d, active-low)\n", PIN_LED, digitalRead(PIN_LED));
}

void ledOff() {
  g_pat = LedPattern::Off;
  g_last_level = -1;
  ledWrite(false);
}

void ledSet(LedPattern pattern) {
  if (g_pat == pattern) return;
  g_pat = pattern;
  g_t0 = millis();
  g_last_level = -1;
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
      // ~3 double-pulses then auto-off (web-ready cue, not sticky)
      if (elapsed > 3000) {
        ledOff();
        break;
      }
      const uint32_t cycle = elapsed % 940;
      const bool on = (cycle < 80) || (cycle >= 160 && cycle < 240);
      ledWrite(on);
      break;
    }
    case LedPattern::ErrorBurst: {
      if (elapsed > 2000) {
        ledOff();
        break;
      }
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
