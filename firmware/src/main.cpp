/**
 * Carte — multi-app board (todo / calendar / weather / album) + WiFi/Web config
 * Hardware: ZECTRIX NOTE4C
 *
 * Keys (outside web config):
 *   DOWN short(s)    → next app(s); multi-click within combo window skips N
 *   UP short(s)      → prev app(s); same combo window as BOOT/album
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
#include "app_calendar.h"
#include "app_weather.h"
#include "app_album.h"
#include "app_apps.h"
#include "app_theme.h"
#include "app_led.h"
#include "app_power.h"
#include "app_battery.h"
#include "app_rtc.h"
#include "app_version.h"
#include "app_errorlog.h"
#include "app_gfx_icon.h"

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
  powerClearDownClicks();
}

static void skipApps(int steps) {
  if (steps == 0) return;
  if (steps > 0) {
    for (int i = 0; i < steps; i++) g_app = appIdNext(g_app);
  } else {
    for (int i = 0; i < -steps; i++) g_app = appIdPrev(g_app);
  }
  if (g_app == AppId::Weather) g_weather_mode = WeatherMode::Now;
}

static int utf8TextWidth(const char *s) {
  int w = 0;
  const uint8_t *p = reinterpret_cast<const uint8_t *>(s);
  while (*p) {
    if (*p < 0x80) {
      w += 8;
      p++;
    } else if ((*p & 0xE0) == 0xC0 && p[1]) {
      w += 16;
      p += 2;
    } else if ((*p & 0xF0) == 0xE0 && p[1] && p[2]) {
      w += 16;
      p += 3;
    } else if ((*p & 0xF8) == 0xF0 && p[1] && p[2] && p[3]) {
      w += 16;
      p += 4;
    } else {
      w += 8;
      p++;
    }
  }
  return w;
}

// Web 路径以黄底黑框 tab 画出。返回 tab 区底边 y。
static int drawWebPageTabs(int x, int y, int max_w) {
  static const char *kTabs[] = {"状态", "设置", "WiFi", "鉴权", "待办", "天气", "相册", "错误", "帮助"};
  constexpr int kPadX = 5;
  constexpr int kPadY = 3;
  constexpr int kGap = 4;
  constexpr int kTh = 16 + kPadY * 2;
  int cx = x;
  int cy = y;
  for (const char *label : kTabs) {
    const int tw = utf8TextWidth(label);
    const int twid = tw + kPadX * 2;
    if (cx + twid > x + max_w && cx > x) {
      cx = x;
      cy += kTh + kGap;
    }
    epd.fillRect(cx, cy, twid, kTh, Theme::Accent);
    epd.drawRect(cx, cy, twid, kTh, Theme::Fg);
    fontDrawTextWrapped(epd, cx + kPadX, cy + kPadY, tw, cy + kPadY + 16, label, Theme::Fg,
                        Theme::Accent);
    cx += twid + kGap;
  }
  return cy + kTh;
}

static void drawConfigWebPage(bool ok) {
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

  constexpr int kGap = 3;
  constexpr int kMaxY = Theme::FooterY - 4;
  constexpr int kPad = 8;
  char line[64];
  const bool ap = webIsAp();
  const int icon_sz = 50;
  const int banner_y = Theme::HeaderH;
  const int banner_h = kPad + icon_sz + kPad;
  epd.fillRect(0, banner_y, 400, banner_h, Theme::HeaderBg);
  webModeIconDraw(epd, kPad, banner_y + kPad, ap, icon_sz, false);
  const int tx = kPad + icon_sz + kPad;
  const int tw = 400 - tx - kPad;
  int ty = banner_y + kPad;
  ty = fontDrawTextWrapped(epd, tx, ty, tw, banner_y + banner_h, webModeName(), Theme::HeaderAccent,
                           Theme::HeaderBg);
  snprintf(line, sizeof(line), "地址：%s", webUrl());
  fontDrawTextWrapped(epd, tx, ty, tw, banner_y + banner_h, line, Theme::HeaderFg, Theme::HeaderBg);
  int y = banner_y + banner_h + kGap;
  if (ap) {
    snprintf(line, sizeof(line), "热点名 %s", WIFI_AP_SSID);
    y = fontDrawTextWrapped(epd, 8, y, 384, kMaxY, line, Theme::Fg) + kGap;
    snprintf(line, sizeof(line), "密码 %s", WIFI_AP_PASS);
    y = fontDrawTextWrapped(epd, 8, y, 384, kMaxY, line, Theme::Fg) + kGap;
    y = fontDrawTextWrapped(epd, 8, y, 384, kMaxY, "手机连此热点后，用浏览器打开上面的地址", Theme::Fg) +
        kGap;
  } else {
    y = fontDrawTextWrapped(epd, 8, y, 384, kMaxY, "请与手机连同一 WiFi，浏览器打开上面的地址", Theme::Fg) +
        kGap;
  }
  y = drawWebPageTabs(8, y + 2, 384) + 8;
  y = fontDrawTextWrapped(epd, 8, y, 384, kMaxY,
                          ap ? "长按上键：改用同 WiFi 站点" : "长按上键：改用 AP 模式", Theme::Fg) +
      kGap;
  y = fontDrawTextWrapped(epd, 8, y, 384, kMaxY, "BOOT 键：退出配置，返回应用", Theme::Fg) + kGap;
  y = fontDrawTextWrapped(epd, 8, y, 384, kMaxY, "长按下键约 3 秒：关机；再按下键开机", Theme::Fg) + kGap;
  const uint32_t idle = getAppConfig().idle_off_sec;
  if (idle == 0) {
    fontDrawTextWrapped(epd, 8, y, 384, kMaxY, "无操作自动关机：关闭（配置页期间不会关）", Theme::Fg);
  } else {
    snprintf(line, sizeof(line), "无操作 %u 秒后关机（配置页期间不会关）", (unsigned)idle);
    fontDrawTextWrapped(epd, 8, y, 384, kMaxY, line, Theme::Fg);
  }
  epd.display();
}

static bool showCurrentApp(bool refresh) {
  if (!appIdEnabled(g_app)) {
    g_app = appIdFirstEnabled();
    Serial.printf("[UI] snap to enabled app=%s\n", appIdName(g_app));
  }
  Serial.printf("[UI] app=%s refresh=%d boot=%d up=%d dn=%d\n", appIdName(g_app), (int)refresh,
                digitalRead(PIN_BTN_BOOT) == LOW, digitalRead(PIN_BTN_UP) == LOW,
                digitalRead(PIN_BTN_DOWN) == LOW);
  switch (g_app) {
    case AppId::Todo:
      return todoRefreshAndDraw(epd);
    case AppId::Calendar:
      return calendarShow(epd);
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
  // CHRG/STDBY configured in batteryBegin() (CHRG needs pull-up)

  pinMode(PIN_NFC_PWR, OUTPUT);
  pinMode(PIN_AUDIO_PWR, OUTPUT);
  pinMode(PIN_SPK_PA, OUTPUT);
  digitalWrite(PIN_NFC_PWR, LOW);
  digitalWrite(PIN_AUDIO_PWR, LOW);
  digitalWrite(PIN_SPK_PA, LOW);

  Serial.begin(115200);
  delay(600);
  Serial.printf("======== Carte %s(%d) ========\n", APP_VERSION, APP_BUILD);
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
  errorLogBegin();
  {
    const esp_reset_reason_t rr = esp_reset_reason();
    if (rr == ESP_RST_TASK_WDT || rr == ESP_RST_INT_WDT || rr == ESP_RST_WDT) {
      errorLog("sys", "上次复位：看门狗（任务或 idle）。HTTPS/刷屏过久会这样，灯闪几下且墨水屏不更新。");
    } else if (rr == ESP_RST_PANIC) {
      errorLog("sys", "上次复位：Panic（常见于栈溢出）。");
    }
  }

  i2cBusRecover();
  digitalWrite(PIN_AUDIO_PWR, HIGH);
  digitalWrite(PIN_NFC_PWR, HIGH);
  delay(30);
  Serial.printf("[I2C] RTC=%s NFC=%s ES=%s\n", i2cAck(I2C_ADDR_RTC) ? "OK" : "--",
                i2cAck(I2C_ADDR_NFC) ? "OK" : "--", i2cAck(I2C_ADDR_ES8311) ? "OK" : "--");
  rtcBegin();  // 从 PCF8563 灌入系统时间；复位后下次联网会再 NTP 一次

  uint8_t saved_app = 0, saved_wx = 0;
  int saved_alb = 0;
  if (loadUiSession(saved_app, saved_wx, saved_alb) && saved_app < static_cast<uint8_t>(AppId::Count)) {
    g_app = static_cast<AppId>(saved_app);
    if (saved_wx < static_cast<uint8_t>(WeatherMode::Count)) {
      g_weather_mode = weatherModeNormalize(static_cast<WeatherMode>(saved_wx));
    }
    g_album_index = saved_alb;
    Serial.printf("[UI] restore app=%s wx=%s alb=%d\n", appIdName(g_app),
                  weatherModeName(g_weather_mode), g_album_index);
  } else {
    g_app = AppId::Todo;
    g_weather_mode = WeatherMode::Now;
  }
  if (!appIdEnabled(g_app)) {
    g_app = appIdFirstEnabled();
    Serial.printf("[UI] disabled restore -> %s\n", appIdName(g_app));
  }

  // 上电不刷屏（墨水屏保留上一帧）；快闪 3 下表示已开机
  ledBootFlash();
  powerIdleSetTimeoutSec(getAppConfig().idle_off_sec);
  suppressKeys(500);  // ignore boot bounce after power-on
}

void loop() {
  static uint32_t up_since = 0;
  static bool up_prev = false;
  static bool up_long_done = false;
  static uint8_t up_clicks = 0;
  static uint32_t up_last_ms = 0;
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
    up_clicks = 0;
    powerClearDownClicks();
    return;
  }

  auto finishAction = [&](bool ok) {
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
    up_clicks = 0;
    up_prev = digitalRead(PIN_BTN_UP) == LOW;
    suppressKeys(80);
    busy = false;
  };

  if (in_web) {
    if (powerTakeBootClicksNow()) {
      busy = true;
      ledSet(LedPattern::Breathe);
      Serial.println("[UI] BOOT -> exit web");
      webStop();
      // 配置里关掉了当前 App：跟 showCurrentApp 一样落到第一个可用的，不要再还原回去
      if (!appIdEnabled(g_app)) {
        const AppId was = g_app;
        g_app = appIdFirstEnabled();
        Serial.printf("[UI] web exit: %s disabled -> %s\n", appIdName(was), appIdName(g_app));
      }
      const bool ok = showCurrentApp(false);
      finishAction(ok);
      return;
    }
    powerClearDownClicks();
    up_clicks = 0;
  } else if (!busy) {
    const uint8_t n = powerTakeBootBurst();
    if (n) {
      busy = true;
      ledSet(LedPattern::Breathe);
      // 会话若仍指着已禁用 App（旧 bug 残留），先对齐再处理 BOOT
      if (!appIdEnabled(g_app)) {
        const AppId was = g_app;
        g_app = appIdFirstEnabled();
        Serial.printf("[UI] BOOT: %s disabled -> %s\n", appIdName(was), appIdName(g_app));
      }
      Serial.printf("[UI] BOOT x%u app=%s\n", (unsigned)n, appIdName(g_app));
      bool ok = false;
      if (g_app == AppId::Weather) {
        for (uint8_t i = 0; i < n; i++) g_weather_mode = weatherModeNext(g_weather_mode);
        Serial.printf("[UI] weather skip %u -> %s\n", (unsigned)n, weatherModeName(g_weather_mode));
        persistUi();  // keep mode if fetch/WDT resets before finishAction
        ok = showCurrentApp(false);
      } else if (g_app == AppId::Album) {
        const int count = albumCountCached();
        if (count > 0) g_album_index = (g_album_index + (int)n) % count;
        Serial.printf("[UI] album skip %u -> %d/%d\n", (unsigned)n, g_album_index, count);
        persistUi();
        ok = showCurrentApp(false);
      } else {
        ok = showCurrentApp(false);
      }
      finishAction(ok);
      return;
    }
  }

  const bool keys_blocked = busy || (millis() < g_ignore_keys_until);
  if (keys_blocked) {
    // Keep edge detectors in sync but do not act / accumulate
    if (!up) {
      up_since = 0;
      up_long_done = false;
    } else if (up_since == 0) {
      up_since = millis();
      up_long_done = false;
    }
    up_prev = up;
    up_clicks = 0;
    powerClearDownClicks();
    return;
  }

  // UP: long → web; short releases accumulate → prev app(s) after combo quiet
  if (up) {
    if (up_since == 0) {
      up_since = millis();
      up_long_done = false;
    }
    if (!up_long_done && (millis() - up_since > 1500)) {
      up_long_done = true;
      up_clicks = 0;
      powerClearDownClicks();
      busy = true;
      ledAckFlash();
      ledSet(LedPattern::BlinkSlow);
      bool force_ap = false;
      bool allow_ap = true;
      if (in_web) {
        if (webIsAp()) {
          force_ap = false;
          allow_ap = false;
          Serial.println("[UI] UP long -> WiFi mode");
        } else {
          force_ap = true;
          Serial.println("[UI] UP long -> AP mode");
        }
      } else {
        Serial.println("[UI] UP long -> config web");
      }
      bool ok = webStart(force_ap, allow_ap);
      if (!ok && in_web && !force_ap) {
        Serial.println("[UI] WiFi failed, restore AP");
        ok = webStart(true);
      }
      ledSet(ok ? LedPattern::Breathe : LedPattern::ErrorBurst);
      drawConfigWebPage(ok);
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
        if (up_clicks < 40) up_clicks++;
        up_last_ms = millis();
      }
    }
    up_since = 0;
    up_long_done = false;
  }
  up_prev = up;

  if (!in_web && !busy && !up && up_clicks > 0 &&
      (millis() - up_last_ms >= POWER_KEY_COMBO_MS)) {
    const uint8_t n = up_clicks;
    up_clicks = 0;
    busy = true;
    ledSet(LedPattern::Breathe);
    skipApps(-(int)n);
    Serial.printf("[UI] UP x%u -> %s\n", (unsigned)n, appIdName(g_app));
    const bool ok = showCurrentApp(false);
    finishAction(ok);
    return;
  }

  // DOWN short(s) → next app(s) after combo quiet (same as BOOT/album)
  if (!in_web && !busy) {
    const uint8_t n = powerTakeDownBurst();
    if (n) {
      busy = true;
      ledSet(LedPattern::Breathe);
      skipApps((int)n);
      Serial.printf("[UI] DOWN x%u -> %s\n", (unsigned)n, appIdName(g_app));
      const bool ok = showCurrentApp(false);
      finishAction(ok);
    }
  } else if (in_web) {
    powerClearDownClicks();
  }
}
