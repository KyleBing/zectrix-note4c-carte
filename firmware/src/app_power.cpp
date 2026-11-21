#include "app_power.h"
#include "app_battery.h"
#include "app_config.h"
#include "board.h"
#include "app_led.h"

#include <esp_sleep.h>
#include <esp_task_wdt.h>
#include "driver/rtc_io.h"

extern bool loopTaskWDTEnabled;

static int g_wdt_long_op = 0;
static bool g_wdt_had_loop = false;
static bool g_wdt_had_idle0 = false;
static bool g_wdt_had_idle1 = false;

static bool idleWatched(int core) {
  TaskHandle_t idle = xTaskGetIdleTaskHandleForCPU(core);
  return idle && esp_task_wdt_status(idle) == ESP_OK;
}

void wdtPet() {
  yield();
  if (g_wdt_long_op == 0 && loopTaskWDTEnabled) {
    esp_task_wdt_reset();
  }
}

void wdtLongOpBegin() {
  if (g_wdt_long_op++ > 0) return;
  // Arduino 默认不盯 loop 任务，只盯 CPU0 idle。结束时绝不能 enableLoopWDT /
  // enableCore1WDT，否则会把本来没订阅的任务加进 TWDT，刷屏 10s 必复位。
  g_wdt_had_loop = loopTaskWDTEnabled;
  if (g_wdt_had_loop) disableLoopWDT();
  g_wdt_had_idle0 = idleWatched(0);
  if (g_wdt_had_idle0) disableCore0WDT();
#ifndef CONFIG_FREERTOS_UNICORE
  g_wdt_had_idle1 = idleWatched(1);
  if (g_wdt_had_idle1) disableCore1WDT();
#endif
}

void wdtLongOpEnd() {
  if (g_wdt_long_op <= 0) return;
  if (--g_wdt_long_op > 0) return;
  if (g_wdt_had_idle0) enableCore0WDT();
#ifndef CONFIG_FREERTOS_UNICORE
  if (g_wdt_had_idle1) enableCore1WDT();
#endif
  if (g_wdt_had_loop) enableLoopWDT();
}

static uint32_t g_down_since = 0;
static bool g_down_long_done = false;
static bool g_down_held = false;
static uint8_t g_down_clicks = 0;
static uint32_t g_down_last_ms = 0;

static uint32_t g_idle_since = 0;
static uint32_t g_idle_sec = 0;

static bool g_boot_down = false;
static uint8_t g_boot_clicks = 0;
static uint32_t g_boot_last_ms = 0;

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
  if (millis() - g_boot_last_ms < POWER_KEY_COMBO_MS) return 0;
  const uint8_t n = g_boot_clicks;
  g_boot_clicks = 0;
  return n;
}

uint8_t powerTakeDownBurst() {
  if (g_down_clicks == 0 || g_down_held) return 0;
  if (millis() - g_down_last_ms < POWER_KEY_COMBO_MS) return 0;
  const uint8_t n = g_down_clicks;
  g_down_clicks = 0;
  return n;
}

void powerClearDownClicks() { g_down_clicks = 0; }

void powerBegin() {
  // 默认 5s；HTTPS / 四色全刷经常更长。只拉长超时，不额外订阅 loop 任务。
  esp_task_wdt_init(30, true);
  pinMode(PIN_PWR_HOLD, OUTPUT);
  digitalWrite(PIN_PWR_HOLD, HIGH);
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
  g_down_since = 0;
  g_down_long_done = false;
  g_down_held = false;
  g_down_clicks = 0;
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
  // 接电源时：仅当设置允许「连接电源时仍自动关机」才继续
  if (!getAppConfig().idle_off_on_power && batteryOnPower()) return;
  if (millis() - g_idle_since >= g_idle_sec * 1000UL) {
    Serial.printf("[PWR] idle %u s -> off\n", (unsigned)g_idle_sec);
    powerOffNow();
  }
}

void powerOffNow() {
  Serial.println("[PWR] soft off -> deep sleep, wake on DOWN");
  Serial.flush();
  ledOff();

  // Long-press path still holds DOWN; wait for release or wake would fire immediately.
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
  while (digitalRead(PIN_BTN_DOWN) == LOW) {
    delay(10);
  }
  delay(50);

  // Keep pull-up in deep sleep; wake when DOWN is pressed (active low).
  const gpio_num_t down = (gpio_num_t)PIN_BTN_DOWN;
  rtc_gpio_init(down);
  rtc_gpio_set_direction(down, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en(down);
  rtc_gpio_pulldown_dis(down);
  esp_sleep_enable_ext0_wakeup(down, 0);

  digitalWrite(PIN_PWR_HOLD, LOW);
  Serial.flush();
  esp_deep_sleep_start();
  // Not reached. If USB kept the rail up but sleep failed, still wake on DOWN.
  for (;;) {
    if (digitalRead(PIN_BTN_DOWN) == LOW) {
      delay(30);
      if (digitalRead(PIN_BTN_DOWN) == LOW) {
        digitalWrite(PIN_PWR_HOLD, HIGH);
        ESP.restart();
      }
    }
    delay(50);
  }
}

void powerPoll(bool latch_short) {
  wdtPet();
  pollBootClicks();
  const bool down = digitalRead(PIN_BTN_DOWN) == LOW;
  g_down_held = down;
  if (down) {
    powerIdleTouch();
    if (g_down_since == 0) {
      g_down_since = millis();
      g_down_long_done = false;
    }
    if (!g_down_long_done && (millis() - g_down_since >= 3000)) {
      g_down_long_done = true;
      g_down_clicks = 0;
      ledAckFlash();
      powerOffNow();
    }
    return;
  }

  // release
  if (g_down_since != 0 && !g_down_long_done) {
    const uint32_t held = millis() - g_down_since;
    if (latch_short && held >= 30 && held < 3000) {
      if (g_down_clicks < 40) g_down_clicks++;
      g_down_last_ms = millis();
    }
  }
  g_down_since = 0;
  g_down_long_done = false;
}
