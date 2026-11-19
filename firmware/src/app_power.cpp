#include "app_power.h"
#include "board.h"
#include "app_led.h"

static uint32_t g_down_since = 0;

void powerBegin() {
  pinMode(PIN_PWR_HOLD, OUTPUT);
  digitalWrite(PIN_PWR_HOLD, HIGH);
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
  g_down_since = 0;
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

void powerPoll() {
  const bool down = digitalRead(PIN_BTN_DOWN) == LOW;
  if (!down) {
    g_down_since = 0;
    return;
  }
  if (g_down_since == 0) {
    g_down_since = millis();
    return;
  }
  if (millis() - g_down_since >= 3000) {
    powerOffNow();
  }
}
