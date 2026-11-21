#include "app_led.h"
#include "board.h"

#include <driver/gpio.h>

static LedPattern g_pat = LedPattern::Off;
static uint32_t g_t0 = 0;
static int g_last_level = -1;

static inline void ledWrite(bool on) {
  const int level = on ? LED_ON_LEVEL : LED_OFF_LEVEL;
  if (level == g_last_level) return;
  // Official NOTE4C BSP uses gpio_hold; must release before driving (Carte board).
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

void ledAckFlash() {
  // Key-hold path keeps the LED solid on; two dark pulses cut through that.
  for (int i = 0; i < 2; i++) {
    g_last_level = -1;
    ledWrite(false);
    delay(70);
    g_last_level = -1;
    ledWrite(true);
    delay(70);
  }
}

void ledDoneFlash() {
  g_pat = LedPattern::Off;
  g_last_level = -1;
  ledWrite(true);
  delay(140);
  g_last_level = -1;
  ledWrite(false);
}

void ledBootFlash() {
  g_pat = LedPattern::Off;
  for (int i = 0; i < 3; i++) {
    g_last_level = -1;
    ledWrite(true);
    delay(80);
    g_last_level = -1;
    ledWrite(false);
    delay(80);
  }
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
  // Any key held: LED solid on until release (overrides pattern).
  if (digitalRead(PIN_BTN_BOOT) == LOW || digitalRead(PIN_BTN_UP) == LOW ||
      digitalRead(PIN_BTN_DOWN) == LOW) {
    ledWrite(true);
    return;
  }

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
    case LedPattern::Breathe: {
      const uint32_t period = 1600;
      uint32_t t = elapsed % period;
      if (t > period / 2) t = period - t;
      const uint32_t frame = 16;
      const uint32_t on = (t * frame) / (period / 2);
      const uint32_t phase = millis() % frame;
      ledWrite(on > 0 && phase < on);
      break;
    }
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
