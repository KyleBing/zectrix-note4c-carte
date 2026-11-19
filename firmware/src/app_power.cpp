#include "app_power.h"
#include "board.h"
#include "app_led.h"

static uint32_t g_down_since = 0;
static bool g_down_long_done = false;
static bool g_short_pending = false;
static uint32_t g_idle_since = 0;
static uint32_t g_idle_sec = 0;
static bool g_boot_down = false;
static uint8_t g_boot_clicks = 0;
static uint32_t g_boot_last_ms = 0;
static constexpr uint32_t kBootComboMs = 500;

static void pollBootClicks() {
  const bool boot = digitalRead(PIN_BTN_BOOT) == LOW;
  if (boot) powerIdleTouch();
  if (g_boot_down && !boot) {
    if (g_boot_clicks < 40) g_boot_clicks++;
    g_boot_last_ms = millis();
  }
  g_boot_down = boot;
}

bool powerBootHeld() { return g_boot_down; }

uint8_t powerTakeBootClicksNow() {
  const uint8_t n = g_boot_clicks;
  g_boot_clicks = 0;
  return n;
}

uint8_t powerTakeBootBurst() {
  if (g_boot_clicks == 0 || g_boot_down) return 0;
  if (millis() - g_boot_last_ms < kBootComboMs) return 0;
  const uint8_t n = g_boot_clicks;
  g_boot_clicks = 0;
  return n;
}

void powerBegin() {
  pinMode(PIN_PWR_HOLD, OUTPUT);
  digitalWrite(PIN_PWR_HOLD, HIGH);
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
  g_down_since = 0;
  g_down_long_done = false;
  g_short_pending = false;
  g_idle_since = millis();
}

void powerIdleTouch() { g_idle_since = millis(); }

void powerIdleSetTimeoutSec(uint32_t sec) {
  g_idle_sec = sec;
  g_idle_since = millis();
}

uint32_t powerIdleTimeoutSec() { return g_idle_sec; }

void powerIdlePoll(bool inhibit) {
  if (inhibit || g_idle_sec == 0) return;
  if (millis() - g_idle_since >= g_idle_sec * 1000UL) {
    Serial.printf("[PWR] idle %u s -> off\n", (unsigned)g_idle_sec);
    powerOffNow();
  }
}

void powerOffNow() {
  Serial.println("[PWR] releasing PWR_HOLD (off)");
  Serial.flush();
  ledOff();
  digitalWrite(PIN_PWR_HOLD, LOW);
  // If USB is still supplying 5V the board may stay on; spin quietly.
  for (;;) {
    delay(1000);
  }
}

void powerPoll(bool latch_short) {
  pollBootClicks();
  const bool down = digitalRead(PIN_BTN_DOWN) == LOW;
  if (down) {
    powerIdleTouch();
    if (g_down_since == 0) {
      g_down_since = millis();
      g_down_long_done = false;
    }
    if (!g_down_long_done && (millis() - g_down_since >= 3000)) {
      g_down_long_done = true;
      ledAckFlash();
      powerOffNow();
    }
    return;
  }

  // release
  if (g_down_since != 0 && !g_down_long_done) {
    const uint32_t held = millis() - g_down_since;
    if (latch_short && held >= 30 && held < 3000) {
      g_short_pending = true;
    }
  }
  g_down_since = 0;
  g_down_long_done = false;
}

bool powerConsumeShortPress() {
  if (!g_short_pending) return false;
  g_short_pending = false;
  return true;
}

void powerClearShortPress() { g_short_pending = false; }
