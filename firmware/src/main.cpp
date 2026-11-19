/**
 * NOTE4C — multi-app board (todo / weather / album) + WiFi/Web config
 *
 * Keys (outside web config):
 *   DOWN short       → next app (1→2→3)
 *   UP short         → prev app (3→2→1)
 *   UP hold ~1.5s    → config web
 *   DOWN hold ~3s    → power off
 *   BOOT release     → in-app ONLY (never switches App)
 *
 * LED: 按键按下即亮、松开即灭；长按阈值到达时闪两下（可松手）；刷屏呼吸；慢闪连 WiFi；失败三短闪
 */

#include <Arduino.h>
#include <Wire.h>
#include <esp_system.h>

#include "board.h"
#include "epd_ssd2683.h"
#include "app_config.h"
#include "app_font.h"
#include "app_wifi.h"
#include "app_web.h"
#include "app_todo.h"
#include "app_weather.h"
#include "app_album.h"
#include "app_apps.h"
#include "app_theme.h"
#include "app_led.h"
#include "app_power.h"
#include "app_battery.h"
#include "app_version.h"

static EpdSsd2683 epd;
static AppId g_app = AppId::Todo;
static WeatherMode g_weather_mode = WeatherMode::Now;
static int g_album_index = 0;
static uint32_t g_ignore_keys_until = 0;

static bool i2cAck(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission(true) == 0;
}

static void i2cBusRecover() {
  Wire.end();
  pinMode(PIN_I2C_SDA, INPUT_PULLUP);
  pinMode(PIN_I2C_SCL, OUTPUT);
  for (int i = 0; i < 9; i++) {
    digitalWrite(PIN_I2C_SCL, LOW);
    delayMicroseconds(5);
    digitalWrite(PIN_I2C_SCL, HIGH);
    delayMicroseconds(5);
    if (digitalRead(PIN_I2C_SDA)) break;
  }
  pinMode(PIN_I2C_SDA, OUTPUT);
  digitalWrite(PIN_I2C_SDA, LOW);
  delayMicroseconds(5);
  digitalWrite(PIN_I2C_SCL, HIGH);
  delayMicroseconds(5);
  digitalWrite(PIN_I2C_SDA, HIGH);
  delayMicroseconds(5);
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(100000);
  Wire.setTimeOut(50);
}

static void persistUi() {
  saveUiSession(static_cast<uint8_t>(g_app), static_cast<uint8_t>(g_weather_mode), g_album_index);
}

static void suppressKeys(uint32_t ms) {
  g_ignore_keys_until = millis() + ms;
  powerClearShortPress();
}

static void drawConfigWebPage(bool ok, bool forced_ap) {
  if (!epd.ok()) return;
  epd.clear(Theme::Bg);
  epd.fillRect(0, 0, 400, Theme::HeaderH, Theme::HeaderBg);
  fontDrawTextWrapped(epd, 8, 3, 220, Theme::HeaderH, "配置网页", Theme::HeaderFg, Theme::HeaderBg);
  batteryDrawHeader(epd);

  if (!ok) {
    fontDrawTextWrapped(epd, 8, 40, 384, 260, "网页启动失败", Theme::Alert);
    fontDrawTextWrapped(epd, 8, 64, 384, 260, "热点密码至少 8 位", Theme::Fg);
    fontDrawTextWrapped(epd, 8, 88, 384, 260, "再试：长按上键约 1.5 秒", Theme::Fg);
    fontDrawTextWrapped(epd, 8, 112, 384, 260, "详情见 USB 串口日志", Theme::Fg);
    epd.display();
    return;
  }

  char line[64];
  snprintf(line, sizeof(line), "模式：%s", strcmp(webModeName(), "AP") == 0 ? "热点" : "无线网");
  fontDrawTextWrapped(epd, 8, 36, 384, 260, line, Theme::Fg);
  snprintf(line, sizeof(line), "地址：%s", webUrl());
  fontDrawTextWrapped(epd, 8, 56, 384, 260, line, Theme::Alert);
  int y = 80;
  if (strcmp(webModeName(), "AP") == 0 || forced_ap) {
    snprintf(line, sizeof(line), "热点名 %s", WIFI_AP_SSID);
    y = fontDrawTextWrapped(epd, 8, y, 384, 260, line, Theme::Fg);
    snprintf(line, sizeof(line), "密码 %s", WIFI_AP_PASS);
    y = fontDrawTextWrapped(epd, 8, y, 384, 260, line, Theme::Fg);
    y = fontDrawTextWrapped(epd, 8, y + 4, 384, 260, "手机连此热点后，用浏览器打开上面的地址", Theme::Fg);
  } else {
    y = fontDrawTextWrapped(epd, 8, y, 384, 260, "请与手机连同一 WiFi，浏览器打开上面的地址", Theme::Fg);
  }
  y = fontDrawTextWrapped(epd, 8, y + 8, 384, 260, "页面：状态、WiFi、鉴权、待办、天气、相册", Theme::Fg);
  y = fontDrawTextWrapped(epd, 8, y + 4, 384, 260, "长按上键：强制开热点", Theme::Fg);
  y = fontDrawTextWrapped(epd, 8, y, 384, 260, "BOOT 键：退出配置，返回应用", Theme::Fg);
  y = fontDrawTextWrapped(epd, 8, y, 384, 260, "长按下键约 3 秒：关机", Theme::Fg);
  const uint32_t idle = getAppConfig().idle_off_sec;
  if (idle == 0) {
    fontDrawTextWrapped(epd, 8, y, 384, 260, "无操作自动关机：关闭（配置页期间不会关）", Theme::Fg);
  } else {
    snprintf(line, sizeof(line), "无操作 %u 秒后关机（配置页期间不会关）", (unsigned)idle);
    fontDrawTextWrapped(epd, 8, y, 384, 260, line, Theme::Fg);
  }
  epd.display();
}

static bool showCurrentApp(bool refresh) {
  Serial.printf("[UI] app=%s refresh=%d boot=%d up=%d dn=%d\n", appIdName(g_app), (int)refresh,
                digitalRead(PIN_BTN_BOOT) == LOW, digitalRead(PIN_BTN_UP) == LOW,
                digitalRead(PIN_BTN_DOWN) == LOW);
  switch (g_app) {
    case AppId::Todo:
      return todoRefreshAndDraw(epd);
    case AppId::Weather:
      if (refresh) g_weather_mode = weatherModeNext(g_weather_mode);
      Serial.printf("[UI] weather mode=%s\n", weatherModeName(g_weather_mode));
      return weatherShowMode(epd, g_weather_mode);
    case AppId::Album: {
      if (refresh) {
        const int n = albumCountCached();
        if (n > 0) g_album_index = (g_album_index + 1) % n;
      }
      return albumShow(epd, g_album_index);
    }
    default:
      return false;
  }
}

void setup() {
  powerBegin();

  pinMode(PIN_BTN_BOOT, INPUT_PULLUP);
  pinMode(PIN_BTN_UP, INPUT_PULLUP);
  pinMode(PIN_STDBY, INPUT);
  pinMode(PIN_CHRG, INPUT);

  pinMode(PIN_NFC_PWR, OUTPUT);
  pinMode(PIN_AUDIO_PWR, OUTPUT);
  pinMode(PIN_SPK_PA, OUTPUT);
  digitalWrite(PIN_NFC_PWR, LOW);
  digitalWrite(PIN_AUDIO_PWR, LOW);
  digitalWrite(PIN_SPK_PA, LOW);

  Serial.begin(115200);
  delay(600);
  Serial.printf("======== NOTE4C apps %s(%d) ========\n", APP_VERSION, APP_BUILD);
  Serial.printf("[BOOT] reset_reason=%d\n", (int)esp_reset_reason());

  ledBegin();
  batteryBegin();

  initAppStorage();
  loadAppConfig();

  if (!epd.begin(PIN_EPD_SCK, PIN_EPD_MOSI, PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY,
                 PIN_EPD_PWR)) {
    Serial.println("[EPD] begin failed");
  }

  fontBegin();

  i2cBusRecover();
  digitalWrite(PIN_AUDIO_PWR, HIGH);
  digitalWrite(PIN_NFC_PWR, HIGH);
  delay(30);
  Serial.printf("[I2C] RTC=%s NFC=%s ES=%s\n", i2cAck(I2C_ADDR_RTC) ? "OK" : "--",
                i2cAck(I2C_ADDR_NFC) ? "OK" : "--", i2cAck(I2C_ADDR_ES8311) ? "OK" : "--");

  ledSet(LedPattern::BlinkSlow);

  uint8_t saved_app = 0, saved_wx = 0;
  int saved_alb = 0;
  if (loadUiSession(saved_app, saved_wx, saved_alb) && saved_app < static_cast<uint8_t>(AppId::Count)) {
    g_app = static_cast<AppId>(saved_app);
    if (saved_wx < static_cast<uint8_t>(WeatherMode::Count)) {
      g_weather_mode = static_cast<WeatherMode>(saved_wx);
    }
    g_album_index = saved_alb;
    Serial.printf("[UI] restore app=%s wx=%s alb=%d\n", appIdName(g_app),
                  weatherModeName(g_weather_mode), g_album_index);
  } else {
    g_app = AppId::Todo;
    g_weather_mode = WeatherMode::Now;
  }

  showCurrentApp(false);
  powerIdleSetTimeoutSec(getAppConfig().idle_off_sec);
  ledOff();
  suppressKeys(500);  // ignore boot bounce after power-on
}

void loop() {
  static uint32_t up_since = 0;
  static bool up_prev = false;
  static bool up_long_done = false;
  static bool busy = false;

  powerPoll(true);
  ledUpdate();
  webLoop();

  const bool boot = powerBootHeld();
  const bool up = digitalRead(PIN_BTN_UP) == LOW;
  const bool in_web = webIsRunning();
  if (boot || up) powerIdleTouch();
  powerIdlePoll(in_web);

  // While BOOT is held, freeze UP edge tracking (avoid crosstalk with front button)
  if (boot) {
    up_since = 0;
    up_long_done = false;
    up_prev = up;
    powerClearShortPress();
    return;
  }

  auto finishBootAction = [&](bool ok) {
    ledSet(ok ? LedPattern::Off : LedPattern::ErrorBurst);
    persistUi();
    if (ledGet() == LedPattern::ErrorBurst) {
      const uint32_t t = millis();
      while (millis() - t < 1800) {
        ledUpdate();
        powerPoll(false);
        delay(10);
      }
      ledOff();
    }
    up_since = 0;
    up_long_done = false;
    up_prev = digitalRead(PIN_BTN_UP) == LOW;
    suppressKeys(80);
    busy = false;
  };

  if (in_web) {
    if (powerTakeBootClicksNow()) {
      const AppId stayed = g_app;
      busy = true;
      ledSet(LedPattern::Breathe);
      Serial.println("[UI] BOOT -> exit web");
      webStop();
      const bool ok = showCurrentApp(false);
      if (g_app != stayed) g_app = stayed;
      finishBootAction(ok);
      return;
    }
  } else if (!busy) {
    const uint8_t n = powerTakeBootBurst();
    if (n) {
      const AppId stayed = g_app;
      busy = true;
      ledSet(LedPattern::Breathe);
      Serial.printf("[UI] BOOT x%u app=%s\n", (unsigned)n, appIdName(g_app));
      bool ok = false;
      if (g_app == AppId::Weather) {
        for (uint8_t i = 0; i < n; i++) g_weather_mode = weatherModeNext(g_weather_mode);
        Serial.printf("[UI] weather skip %u -> %s\n", (unsigned)n, weatherModeName(g_weather_mode));
        ok = showCurrentApp(false);
      } else if (g_app == AppId::Album) {
        const int count = albumCountCached();
        if (count > 0) g_album_index = (g_album_index + (int)n) % count;
        Serial.printf("[UI] album skip %u -> %d/%d\n", (unsigned)n, g_album_index, count);
        ok = showCurrentApp(false);
      } else {
        ok = showCurrentApp(false);
      }
      if (g_app != stayed) {
        Serial.printf("[UI] BUG app mutated %s -> %s, revert\n", appIdName(stayed), appIdName(g_app));
        g_app = stayed;
      }
      finishBootAction(ok);
      return;
    }
  }

  const bool keys_blocked = busy || (millis() < g_ignore_keys_until);
  if (keys_blocked) {
    // Keep edge detectors in sync but do not act
    if (!up) {
      up_since = 0;
      up_long_done = false;
    } else if (up_since == 0) {
      up_since = millis();
      up_long_done = false;
    }
    up_prev = up;
    powerClearShortPress();
    return;
  }

  // UP: long → web; short → previous app
  if (up) {
    if (up_since == 0) {
      up_since = millis();
      up_long_done = false;
    }
    if (!up_long_done && (millis() - up_since > 1500)) {
      up_long_done = true;
      busy = true;
      ledAckFlash();
      ledSet(LedPattern::BlinkSlow);
      const bool force_ap = in_web;
      Serial.printf("[UI] UP long -> %s web\n", force_ap ? "force AP" : "config");
      const bool ok = webStart(force_ap);
      ledSet(ok ? LedPattern::Breathe : LedPattern::ErrorBurst);
      drawConfigWebPage(ok, force_ap);
      ledSet(ok ? LedPattern::Off : LedPattern::ErrorBurst);
      suppressKeys(500);
      busy = false;
      up_prev = digitalRead(PIN_BTN_UP) == LOW;
      return;
    }
  } else {
    if (up_prev && !up_long_done && up_since != 0) {
      const uint32_t held = millis() - up_since;
      if (held >= 40 && !in_web) {
        busy = true;
        ledSet(LedPattern::Breathe);
        g_app = appIdPrev(g_app);
        if (g_app == AppId::Weather) g_weather_mode = WeatherMode::Now;
        Serial.printf("[UI] UP short -> %s (%d/%d) [THIS switches app, not BOOT]\n",
                      appIdName(g_app), static_cast<int>(g_app) + 1,
                      static_cast<int>(AppId::Count));
        const bool ok = showCurrentApp(false);
        persistUi();
        ledSet(ok ? LedPattern::Off : LedPattern::ErrorBurst);
        if (!ok) {
          const uint32_t t = millis();
          while (millis() - t < 1200) {
            ledUpdate();
            delay(10);
          }
          ledOff();
        }
        suppressKeys(500);
        busy = false;
        up_since = 0;
        up_long_done = false;
        up_prev = false;
        return;
      }
    }
    up_since = 0;
    up_long_done = false;
  }
  up_prev = up;

  // DOWN short → next app
  if (!in_web && powerConsumeShortPress()) {
    busy = true;
    ledSet(LedPattern::Breathe);
    g_app = appIdNext(g_app);
    if (g_app == AppId::Weather) g_weather_mode = WeatherMode::Now;
    Serial.printf("[UI] DOWN short -> %s (%d/%d)\n", appIdName(g_app),
                  static_cast<int>(g_app) + 1, static_cast<int>(AppId::Count));
    const bool ok = showCurrentApp(false);
    persistUi();
    ledSet(ok ? LedPattern::Off : LedPattern::ErrorBurst);
    if (!ok) {
      const uint32_t t = millis();
      while (millis() - t < 1200) {
        ledUpdate();
        delay(10);
      }
      ledOff();
    }
    suppressKeys(500);
    busy = false;
  } else if (in_web) {
    powerClearShortPress();
  }
}
