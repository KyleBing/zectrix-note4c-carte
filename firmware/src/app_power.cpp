#include "app_power.h"
#include "app_battery.h"
#include "app_config.h"
#include "app_rtc.h"
#include "board.h"
#include "app_led.h"

#include <esp_sleep.h>
#include <esp_task_wdt.h>
#include <esp_idf_version.h>
#include "driver/rtc_io.h"
#include "driver/gpio.h"

#include <time.h>

extern bool loopTaskWDTEnabled;

static int g_wdt_long_op = 0;
static bool g_wdt_had_loop = false;
static bool g_wdt_had_idle0 = false;
static bool g_wdt_had_idle1 = false;

static bool idleWatched(int core) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  TaskHandle_t idle = xTaskGetIdleTaskHandleForCore(core);
#else
  TaskHandle_t idle = xTaskGetIdleTaskHandleForCPU(core);
#endif
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

static uint32_t g_up_since = 0;
static bool g_up_long_done = false;
static bool g_up_held = false;
static bool g_up_long_latched = false;
static uint8_t g_up_clicks = 0;
static uint32_t g_up_last_ms = 0;

static uint32_t g_idle_since = 0;
static uint32_t g_idle_sec = 0;

static bool g_boot_down = false;
static uint8_t g_boot_clicks = 0;
static uint32_t g_boot_last_ms = 0;

static bool g_nav_armed = true;

static AppId g_fg_app = AppId::Todo;

void powerNotifyForegroundApp(AppId id) { g_fg_app = id; }

static void pollBootClicks() {
  const bool boot = digitalRead(PIN_BTN_BOOT) == LOW;
  if (boot) powerIdleTouch();
  if (g_boot_down && !boot) {
    if (g_boot_clicks < 40) g_boot_clicks++;
    g_boot_last_ms = millis();
  }
  g_boot_down = boot;
}

static void pollUpClicks(bool latch_short) {
  const bool up = digitalRead(PIN_BTN_UP) == LOW;
  g_up_held = up;
  if (up) {
    powerIdleTouch();
    if (g_up_since == 0) {
      g_up_since = millis();
      g_up_long_done = false;
    }
    if (!g_up_long_done && (millis() - g_up_since > 1500)) {
      g_up_long_done = true;
      g_up_clicks = 0;
      g_up_long_latched = true;
      ledAckFlash();
    }
    return;
  }
  if (g_up_since != 0 && !g_up_long_done) {
    const uint32_t held = millis() - g_up_since;
    if (latch_short && held >= 40 && held < 1500) {
      if (g_up_clicks < 40) g_up_clicks++;
      g_up_last_ms = millis();
    }
  }
  g_up_since = 0;
  g_up_long_done = false;
}

bool powerBootHeld() { return g_boot_down; }

bool powerUpHeld() { return g_up_held; }

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

uint8_t powerTakeDownClicksNow() {
  const uint8_t n = g_down_clicks;
  g_down_clicks = 0;
  return n;
}

void powerClearDownClicks() { g_down_clicks = 0; }

uint8_t powerTakeUpBurst() {
  if (g_up_clicks == 0 || g_up_held) return 0;
  if (millis() - g_up_last_ms < POWER_KEY_COMBO_MS) return 0;
  const uint8_t n = g_up_clicks;
  g_up_clicks = 0;
  return n;
}

uint8_t powerTakeUpClicksNow() {
  const uint8_t n = g_up_clicks;
  g_up_clicks = 0;
  return n;
}

void powerClearUpClicks() { g_up_clicks = 0; }

bool powerTakeUpLong() {
  if (!g_up_long_latched) return false;
  g_up_long_latched = false;
  return true;
}

void powerNavDisarmUntilRelease() { g_nav_armed = false; }

bool powerNavArmed() { return g_nav_armed; }

bool powerNavAnyHeld() { return g_boot_down || g_down_held || g_up_held; }

bool powerNavShouldAbort() {
  if (!g_nav_armed) return false;
  if (g_boot_clicks || g_down_clicks || g_up_clicks) return true;
  if (g_up_long_latched) return true;
  if (g_boot_down || g_down_held || g_up_held) return true;
  return false;
}

void powerClearNavClicks() {
  g_boot_clicks = 0;
  g_down_clicks = 0;
  g_up_clicks = 0;
  g_up_long_latched = false;
}

void powerBegin() {
  // 默认 5s；HTTPS / 四色全刷经常更长。只拉长超时，不额外订阅 loop 任务。
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  // Arduino-ESP32 3.x / IDF 5：旧的 (timeout_s, panic) 签名已移除。
  esp_task_wdt_config_t twdt = {
      .timeout_ms = 30000,
      .idle_core_mask = 0,
      .trigger_panic = true,
  };
#if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0
  twdt.idle_core_mask |= (1 << 0);
#endif
#if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1
  twdt.idle_core_mask |= (1 << 1);
#endif
  if (esp_task_wdt_reconfigure(&twdt) == ESP_ERR_INVALID_STATE) {
    esp_task_wdt_init(&twdt);
  }
#else
  esp_task_wdt_init(30, true);
#endif
  // 定时深睡用过 gpio_hold；醒来必须解开，否则改不了 PWR_HOLD
  gpio_hold_dis((gpio_num_t)PIN_PWR_HOLD);
  gpio_deep_sleep_hold_dis();
  pinMode(PIN_PWR_HOLD, OUTPUT);
  digitalWrite(PIN_PWR_HOLD, HIGH);
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
  pinMode(PIN_RTC_INT, INPUT_PULLUP);
  g_down_since = 0;
  g_down_long_done = false;
  g_down_held = false;
  g_down_clicks = 0;
  g_up_since = 0;
  g_up_long_done = false;
  g_up_held = false;
  g_up_long_latched = false;
  g_up_clicks = 0;
  g_nav_armed = true;
  g_idle_since = millis();
  pinMode(PIN_BTN_UP, INPUT_PULLUP);
  pinMode(PIN_BTN_BOOT, INPUT_PULLUP);
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

time_t powerNextWakeUnix(AppId app) {
  if (!appIdEnabled(app)) return 0;
  const time_t now = time(nullptr);
  if (now < 1700000000) return 0;

  if (app == AppId::Calendar) {
    struct tm t;
    if (!localtime_r(&now, &t)) return 0;
    t.tm_mday += 1;
    t.tm_hour = 0;
    t.tm_min = 0;
    t.tm_sec = 0;
    t.tm_isdst = -1;
    time_t wake = mktime(&t);
    if (wake <= now) wake += 86400;
    return wake;
  }

  if (app == AppId::Weather) {
    const AppConfig &cfg = getAppConfig();
    if (!cfg.weather_refresh_en) return 0;
    const uint32_t iv = cfg.weather_refresh_sec;
    if (iv < 60) return 0;
    if (!qweatherConfigured()) return 0;
    uint32_t last = loadWeatherLastUnix();
    // 关机起重新计间隔：过期/从未刷新都不要立刻当到期（否则早醒会连刷）
    if (last == 0 || (time_t)last + (time_t)iv <= now) {
      saveWeatherLastUnix((uint32_t)now);
      last = (uint32_t)now;
    }
    return (time_t)last + (time_t)iv;
  }

  if (app == AppId::Album) {
    const AppConfig &cfg = getAppConfig();
    if (!cfg.album_refresh_en) return 0;
    const uint32_t iv = cfg.album_refresh_sec;
    if (iv < 60) return 0;
    uint32_t last = loadAlbumLastUnix();
    if (last == 0 || (time_t)last + (time_t)iv <= now) {
      saveAlbumLastUnix((uint32_t)now);
      last = (uint32_t)now;
    }
    return (time_t)last + (time_t)iv;
  }

  return 0;
}

bool powerWokeFromRtcAlarm() {
  const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  const bool marked = loadScheduledSleep();
  const bool af = rtcAlarmFired();

  if (cause == ESP_SLEEP_WAKEUP_TIMER) return true;

  if (cause == ESP_SLEEP_WAKEUP_EXT1) {
    const uint64_t st = esp_sleep_get_ext1_wakeup_status();
    const bool rtc_pin = (st & (1ULL << PIN_RTC_INT)) != 0;
    const bool down_pin = (st & (1ULL << PIN_BTN_DOWN)) != 0;
    // 纯下键：用户开机，清定时标记
    if (down_pin && !rtc_pin) {
      if (marked) saveScheduledSleep(false);
      return false;
    }
    if (rtc_pin || af) return true;
  }

  // 唤醒原因丢失（看门狗/掉电毛刺）但睡前写过标记 → 仍当定时任务，避免停在开机态
  if (marked) {
    Serial.printf("[PWR] sched mark + cause=%d -> timer job\n", (int)cause);
    return true;
  }
  return false;
}

static void waitDownReleased() {
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
  while (digitalRead(PIN_BTN_DOWN) == LOW) {
    delay(10);
  }
  delay(50);
}

static void enterDeepSleepReleaseHold() {
  Serial.println("[PWR] soft off -> release hold, wake on DOWN");
  saveScheduledSleep(false);
  rtcAlarmClear();
  ledHoldOffForSleep();

  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

  const gpio_num_t down = (gpio_num_t)PIN_BTN_DOWN;
  rtc_gpio_init(down);
  rtc_gpio_set_direction(down, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en(down);
  rtc_gpio_pulldown_dis(down);
  esp_sleep_enable_ext0_wakeup(down, 0);

  digitalWrite(PIN_PWR_HOLD, LOW);
  Serial.flush();
  esp_deep_sleep_start();
}

static void enterDeepSleepScheduled(time_t wake) {
  const time_t now = time(nullptr);
  int64_t sec = (now >= 1700000000) ? ((int64_t)wake - (int64_t)now) : 60;
  if (sec < 60) sec = 60;
  if (sec > 8LL * 24 * 3600) sec = 8LL * 24 * 3600;

  Serial.printf("[PWR] scheduled sleep until unix=%ld (%ld s, keep hold)\n", (long)wake,
                (long)sec);

  // 主唤醒：ESP 定时器（可靠）。RTC 闹钟仅作辅助，INT 不闲就不用，避免立刻再醒。
  saveScheduledSleep(true);
  rtcAlarmClear();
  pinMode(PIN_RTC_INT, INPUT_PULLUP);
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
  delay(5);

  ledHoldOffForSleep();
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

  const gpio_num_t down = (gpio_num_t)PIN_BTN_DOWN;
  rtc_gpio_init(down);
  rtc_gpio_set_direction(down, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en(down);
  rtc_gpio_pulldown_dis(down);

  // 只用下键做 EXT1，避免 RTC INT 毛刺把定时深睡打成「交互开机」
  const uint64_t mask = (1ULL << PIN_BTN_DOWN);
  digitalWrite(PIN_PWR_HOLD, HIGH);
  gpio_hold_en((gpio_num_t)PIN_PWR_HOLD);
  gpio_deep_sleep_hold_en();

  esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW);
  esp_sleep_enable_timer_wakeup((uint64_t)sec * 1000000ULL);

  Serial.flush();
  esp_deep_sleep_start();
}

void powerOffNow() {
  ledOff();
  waitDownReleased();

  // 关耗电外设（与 setup 对称）
  digitalWrite(PIN_NFC_PWR, LOW);
  digitalWrite(PIN_AUDIO_PWR, LOW);
  digitalWrite(PIN_SPK_PA, LOW);

  // 深睡若返回（USB 扰醒等）必须重试，绝不能落到交互 loop
  for (;;) {
    const time_t wake = powerNextWakeUnix(g_fg_app);
    if (wake > 0) {
      enterDeepSleepScheduled(wake);
    } else {
      enterDeepSleepReleaseHold();
    }
    Serial.println("[PWR] deep sleep returned -> retry");
    delay(200);
    gpio_hold_dis((gpio_num_t)PIN_PWR_HOLD);
    gpio_deep_sleep_hold_dis();
    digitalWrite(PIN_PWR_HOLD, HIGH);
    if (digitalRead(PIN_BTN_DOWN) == LOW) {
      saveScheduledSleep(false);
      ESP.restart();
    }
  }
}

void powerPoll(bool latch_short) {
  wdtPet();
  pollBootClicks();

  // While BOOT held, freeze UP edge tracking (crosstalk with front button).
  const bool boot = g_boot_down;
  if (boot) {
    g_up_since = 0;
    g_up_long_done = false;
    g_up_held = digitalRead(PIN_BTN_UP) == LOW;
    g_up_clicks = 0;
    g_up_long_latched = false;
  } else {
    pollUpClicks(latch_short);
  }

  const bool down = digitalRead(PIN_BTN_DOWN) == LOW;
  g_down_held = down;
  if (!g_nav_armed && !powerNavAnyHeld()) g_nav_armed = true;

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
