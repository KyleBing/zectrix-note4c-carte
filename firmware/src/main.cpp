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
 * LED: 按键按下即亮、松开即灭；长按阈值到达时闪两下（可松手）；刷屏过程不亮，完成后一闪；慢闪连 WiFi；失败三短闪
 *
 * EPD: 四色全刷可被导航键中断；停手后目标页会完整刷完并停留。
 */

#include <Arduino.h>
#include <Wire.h>
#include <esp_system.h>
#include <esp_sleep.h>

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
#include "app_splash.h"

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
  powerNotifyForegroundApp(g_app);
}

static void suppressKeys(uint32_t ms) {
  g_ignore_keys_until = millis() + ms;
  powerClearNavClicks();
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
    epd.display(false);
    return;
  }

  constexpr int kGap = 3;
  constexpr int kAfterBanner = 10;  // 黑块与下方内容
  constexpr int kMaxY = Theme::FooterY - 4;
  constexpr int kPad = 8;
  constexpr int kBannerShiftX = 30;  // 图标+文案整体右移
  constexpr int kIconTextGap = 20;   // 图标与右侧文字
  char line[64];
  const bool ap = webIsAp();
  const int icon_sz = 50;
  const int banner_y = Theme::HeaderH;
  const int banner_h = kPad + icon_sz + kPad;
  epd.fillRect(0, banner_y, 400, banner_h, Theme::HeaderBg);
  const int icon_x = kPad + kBannerShiftX;
  const int icon_y = banner_y + kPad;
  webModeIconDraw(epd, icon_x, icon_y, ap, icon_sz, false);
  const int tx = icon_x + icon_sz + kIconTextGap;
  const int tw = 400 - tx - kPad;
  // 模式名 + 地址两行，相对图标纵向居中
  const int text_h = fontLineHeight() * 2;
  int ty = icon_y + (icon_sz - text_h) / 2;
  if (ty < icon_y) ty = icon_y;
  ty = fontDrawTextWrapped(epd, tx, ty, tw, banner_y + banner_h, webModeName(), Theme::HeaderAccent,
                           Theme::HeaderBg);
  snprintf(line, sizeof(line), "地址：%s", webUrl());
  fontDrawTextWrapped(epd, tx, ty, tw, banner_y + banner_h, line, Theme::HeaderFg, Theme::HeaderBg);
  int y = banner_y + banner_h + kAfterBanner;
  if (ap) {
    // 热点 / 密码：黄底黑框居中，标签等宽对齐；上下各 10px
    const int lh = fontLineHeight();
    constexpr int kBoxPadX = 14;
    constexpr int kBoxPadY = 8;
    constexpr int kRowGap = 4;
    constexpr int kLabelGap = 12;
    const int label_w = utf8TextWidth("热点");  // 与「密码」同宽
    const int val_w = (utf8TextWidth(WIFI_AP_SSID) > utf8TextWidth(WIFI_AP_PASS))
                          ? utf8TextWidth(WIFI_AP_SSID)
                          : utf8TextWidth(WIFI_AP_PASS);
    const int box_w = kBoxPadX * 2 + label_w + kLabelGap + val_w;
    const int box_h = kBoxPadY * 2 + lh * 2 + kRowGap;
    int box_x = (EpdSsd2683::kWidth - box_w) / 2;
    if (box_x < 8) box_x = 8;
    epd.fillRect(box_x, y, box_w, box_h, Theme::Accent);
    epd.drawRect(box_x, y, box_w, box_h, Theme::Fg);
    const int text_x = box_x + kBoxPadX;
    int row_y = y + kBoxPadY;
    fontDrawTextWrapped(epd, text_x, row_y, label_w, row_y + lh, "热点", Theme::Fg, Theme::Accent);
    fontDrawTextWrapped(epd, text_x + label_w + kLabelGap, row_y, val_w + 4, row_y + lh, WIFI_AP_SSID,
                        Theme::Fg, Theme::Accent);
    row_y += lh + kRowGap;
    fontDrawTextWrapped(epd, text_x, row_y, label_w, row_y + lh, "密码", Theme::Fg, Theme::Accent);
    fontDrawTextWrapped(epd, text_x + label_w + kLabelGap, row_y, val_w + 4, row_y + lh, WIFI_AP_PASS,
                        Theme::Fg, Theme::Accent);
    y = y + box_h + kAfterBanner;
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
  const AppConfig &pcfg = getAppConfig();
  const uint32_t idle = pcfg.idle_off_sec;
  if (idle == 0) {
    fontDrawTextWrapped(epd, 8, y, 384, kMaxY, "无操作自动关机：关闭（配置页期间不会关）", Theme::Fg);
  } else if (pcfg.idle_off_on_power) {
    snprintf(line, sizeof(line), "无操作 %u 秒后关机（接电也关；配置页不计）", (unsigned)idle);
    fontDrawTextWrapped(epd, 8, y, 384, kMaxY, line, Theme::Fg);
  } else {
    snprintf(line, sizeof(line), "无操作 %u 秒后关机（接电不关；配置页不计）", (unsigned)idle);
    fontDrawTextWrapped(epd, 8, y, 384, kMaxY, line, Theme::Fg);
  }
  epd.display(false);
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

enum class NavDrainResult : uint8_t { None, Applied, OpenWeb };
enum class InteractiveShowResult : uint8_t { Ok, Fail, WantWeb };

static void applyBootSkip(uint8_t n) {
  if (!n) return;
  if (!appIdEnabled(g_app)) g_app = appIdFirstEnabled();
  if (g_app == AppId::Weather) {
    for (uint8_t i = 0; i < n; i++) g_weather_mode = weatherModeNext(g_weather_mode);
    Serial.printf("[UI] weather skip %u -> %s\n", (unsigned)n, weatherModeName(g_weather_mode));
  } else if (g_app == AppId::Album) {
    const int count = albumCountCached();
    if (count > 0) g_album_index = (g_album_index + (int)n) % count;
    Serial.printf("[UI] album skip %u -> %d/%d\n", (unsigned)n, g_album_index, count);
  }
}

// After EPD abort: wait for key release + combo quiet, then apply nav (or open web).
static NavDrainResult drainNavAfterEpdAbort() {
  const uint32_t t0 = millis();
  uint32_t released_since = 0;
  for (;;) {
    powerPoll(true);
    ledUpdate();
    wdtPet();

    if (powerTakeUpLong()) return NavDrainResult::OpenWeb;

    if (powerNavAnyHeld()) {
      released_since = 0;
    } else if (released_since == 0) {
      released_since = millis();
    } else if (millis() - released_since >= POWER_KEY_COMBO_MS) {
      break;
    }

    if (millis() - t0 > 4000) break;
    delay(10);
  }

  const uint8_t dn = powerTakeDownClicksNow();
  const uint8_t up = powerTakeUpClicksNow();
  const uint8_t boot = powerTakeBootClicksNow();
  if (dn) {
    skipApps((int)dn);
    Serial.printf("[UI] DOWN x%u (abort) -> %s\n", (unsigned)dn, appIdName(g_app));
    return NavDrainResult::Applied;
  }
  if (up) {
    skipApps(-(int)up);
    Serial.printf("[UI] UP x%u (abort) -> %s\n", (unsigned)up, appIdName(g_app));
    return NavDrainResult::Applied;
  }
  if (boot) {
    Serial.printf("[UI] BOOT x%u (abort) app=%s\n", (unsigned)boot, appIdName(g_app));
    applyBootSkip(boot);
    return NavDrainResult::Applied;
  }
  return NavDrainResult::None;
}

static InteractiveShowResult runInteractiveShow(bool refresh) {
  bool r = refresh;
  for (;;) {
    const bool ok = showCurrentApp(r);
    r = false;
    if (!epd.wasAborted()) {
      return ok ? InteractiveShowResult::Ok : InteractiveShowResult::Fail;
    }
    Serial.println("[UI] EPD refresh interrupted by key");
    const NavDrainResult nav = drainNavAfterEpdAbort();
    if (nav == NavDrainResult::OpenWeb) return InteractiveShowResult::WantWeb;
    if (nav == NavDrainResult::Applied) {
      persistUi();
      continue;  // draw the page the user stopped on
    }
    // Aborted with no counted click — finish the same page cleanly.
    continue;
  }
}

// Enter / toggle config web UI (also used when UP-long interrupts an EPD refresh).
static bool enterConfigWeb() {
  const bool in_web = webIsRunning();
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
  if (!ok) ledSet(LedPattern::ErrorBurst);
  drawConfigWebPage(ok);
  return ok;
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

  const bool timer_wake = powerWokeFromRtcAlarm();
  Serial.printf("[BOOT] wakeup_cause=%d timer_wake=%d af=%d int=%d\n",
                (int)esp_sleep_get_wakeup_cause(), (int)timer_wake, (int)rtcAlarmFired(),
                (int)(digitalRead(PIN_RTC_INT) == LOW));

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
  powerNotifyForegroundApp(g_app);

  // 定时醒：到期才刷；早醒（INT 未抬/同一分钟）只再睡，不翻页
  if (timer_wake) {
    rtcAlarmClear();
    bool due = true;
    if (g_app == AppId::Album) due = albumAutoRefreshDue();
    else if (g_app == AppId::Weather) due = weatherAutoRefreshDue();
    Serial.printf("[PWR] timer job app=%s due=%d\n", appIdName(g_app), (int)due);
    if (due) {
      {
        const time_t now = time(nullptr);
        if (now >= 1700000000) {
          if (g_app == AppId::Album) saveAlbumLastUnix((uint32_t)now);
          else if (g_app == AppId::Weather) saveWeatherLastUnix((uint32_t)now);
        }
      }
      epd.suppressInterruptOnce();
      const bool ok = showCurrentApp(g_app == AppId::Album);
      Serial.printf("[PWR] timer refresh ok=%d -> sleep\n", (int)ok);
      persistUi();
    } else {
      Serial.println("[PWR] timer early wake -> sleep without refresh");
    }
    powerOffNow();  // 不返回
  }

  // 交互开机：清定时深睡标记 + 遗留闹钟
  saveScheduledSleep(false);
  rtcAlarmClear();

  // 新固件刷入后首屏展示 LOGO/版本（每个 APP_BUILD 一次）；平时上电仍不刷屏
  if (splashShowIfNeeded(epd)) {
    Serial.printf("[UI] splash build=%d\n", APP_BUILD);
  }
  ledBootFlash();
  powerIdleSetTimeoutSec(getAppConfig().idle_off_sec);
  suppressKeys(500);  // ignore boot bounce after power-on
}

void loop() {
  static bool busy = false;

  powerPoll(true);
  ledUpdate();
  webLoop();

  const bool boot = powerBootHeld();
  const bool up = powerUpHeld();
  const bool in_web = webIsRunning();
  if (boot || up) powerIdleTouch();
  powerIdlePoll(in_web);

  // While BOOT is held, freeze nav shorts (powerPoll already freezes UP)
  if (boot) {
    powerClearNavClicks();
    return;
  }

  auto finishAction = [&](bool ok) {
    persistUi();
    if (ok) {
      ledDoneFlash();
    } else {
      ledSet(LedPattern::ErrorBurst);
      const uint32_t t = millis();
      while (millis() - t < 1800) {
        ledUpdate();
        powerPoll(false);
        delay(10);
      }
      ledOff();
    }
    suppressKeys(80);
    busy = false;
  };

  auto runShow = [&](bool refresh) {
    const InteractiveShowResult sr = runInteractiveShow(refresh);
    if (sr == InteractiveShowResult::WantWeb) {
      ledSet(LedPattern::BlinkSlow);
      const bool ok = enterConfigWeb();
      if (ok) {
        ledDoneFlash();
      } else {
        ledSet(LedPattern::ErrorBurst);
        const uint32_t t = millis();
        while (millis() - t < 1800) {
          ledUpdate();
          powerPoll(false);
          delay(10);
        }
        ledOff();
      }
      suppressKeys(500);
      busy = false;
      return;
    }
    finishAction(sr == InteractiveShowResult::Ok);
  };

  // App 定时任务：仅当「停留在对应 App」时才跑（配置页 / 按键忙碌不计）
  if (!in_web && !busy && !boot && !up) {
    if (g_app == AppId::Weather && weatherAutoRefreshDue()) {
      busy = true;
      Serial.printf("[UI] weather auto-refresh (every %u s)\n",
                    (unsigned)getAppConfig().weather_refresh_sec);
      runShow(false);
      return;
    }
    if (g_app == AppId::Album && albumAutoRefreshDue()) {
      busy = true;
      Serial.printf("[UI] album auto-advance (every %u s)\n",
                    (unsigned)getAppConfig().album_refresh_sec);
      {
        const time_t now = time(nullptr);
        if (now >= 1700000000) saveAlbumLastUnix((uint32_t)now);  // 先占槽，防连刷
      }
      runShow(true);  // 翻下一张
      return;
    }
    if (g_app == AppId::Calendar && calendarAutoRefreshDue()) {
      busy = true;
      Serial.println("[UI] calendar midnight auto-refresh");
      runShow(false);
      return;
    }
  }

  if (in_web) {
    // Web 配置页投屏：只刷墨水屏，不切换当前 App / 相册索引
    if (!busy && albumDisplayPending()) {
      busy = true;
      albumSetCastBusy(true);
      powerIdleTouch();
      Serial.println("[UI] web cast -> EPD (stay in config web)");
      bool ok = false;
      albumTakeDisplay(epd, &ok);
      albumSetCastBusy(false);
      if (ok) {
        ledDoneFlash();
      } else {
        ledSet(LedPattern::ErrorBurst);
        const uint32_t t = millis();
        while (millis() - t < 1200) {
          ledUpdate();
          powerPoll(false);
          delay(10);
        }
        ledOff();
      }
      suppressKeys(80);
      busy = false;
      return;
    }
    if (powerTakeBootClicksNow()) {
      busy = true;
      Serial.println("[UI] BOOT -> exit web");
      webStop();
      // 配置里关掉了当前 App：跟 showCurrentApp 一样落到第一个可用的，不要再还原回去
      if (!appIdEnabled(g_app)) {
        const AppId was = g_app;
        g_app = appIdFirstEnabled();
        Serial.printf("[UI] web exit: %s disabled -> %s\n", appIdName(was), appIdName(g_app));
      }
      runShow(false);
      return;
    }
    powerClearNavClicks();
  } else if (!busy) {
    const uint8_t n = powerTakeBootBurst();
    if (n) {
      busy = true;
      // 会话若仍指着已禁用 App（旧 bug 残留），先对齐再处理 BOOT
      if (!appIdEnabled(g_app)) {
        const AppId was = g_app;
        g_app = appIdFirstEnabled();
        Serial.printf("[UI] BOOT: %s disabled -> %s\n", appIdName(was), appIdName(g_app));
      }
      Serial.printf("[UI] BOOT x%u app=%s\n", (unsigned)n, appIdName(g_app));
      applyBootSkip(n);
      persistUi();  // keep mode/index if fetch/WDT resets before finishAction
      runShow(false);
      return;
    }
  }

  const bool keys_blocked = busy || (millis() < g_ignore_keys_until);
  if (keys_blocked) {
    powerClearNavClicks();
    return;
  }

  // UP long → config web
  if (powerTakeUpLong()) {
    busy = true;
    ledSet(LedPattern::BlinkSlow);
    const bool ok = enterConfigWeb();
    if (ok) {
      ledDoneFlash();
    } else {
      ledSet(LedPattern::ErrorBurst);
      const uint32_t t = millis();
      while (millis() - t < 1800) {
        ledUpdate();
        powerPoll(false);
        delay(10);
      }
      ledOff();
    }
    suppressKeys(500);
    busy = false;
    return;
  }

  if (!in_web && !busy) {
    const uint8_t up_n = powerTakeUpBurst();
    if (up_n) {
      busy = true;
      skipApps(-(int)up_n);
      Serial.printf("[UI] UP x%u -> %s\n", (unsigned)up_n, appIdName(g_app));
      runShow(false);
      return;
    }

    const uint8_t dn = powerTakeDownBurst();
    if (dn) {
      busy = true;
      skipApps((int)dn);
      Serial.printf("[UI] DOWN x%u -> %s\n", (unsigned)dn, appIdName(g_app));
      runShow(false);
      return;
    }
  } else if (in_web) {
    powerClearNavClicks();
  }
}
