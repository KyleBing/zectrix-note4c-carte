/**
 * NOTE4C — diary todo board + WiFi/Web config (portal-go JWT)
 *
 * Keys:
 *   BOOT release     → refresh diary todo page
 *   UP short release → config web (STA if WiFi saved, else AP)
 *   UP hold ~1.5s    → force AP config web (note4c / note4c4c)
 *   DOWN hold 3s     → release PWR_HOLD
 *
 * LED: idle OFF; slow=WiFi; fast=fetch; double=web/AP; error=fail
 */

#include <Arduino.h>
#include <Wire.h>

#include "board.h"
#include "epd_ssd2683.h"
#include "app_config.h"
#include "app_font.h"
#include "app_wifi.h"
#include "app_web.h"
#include "app_todo.h"
#include "app_led.h"
#include "app_power.h"
#include "app_version.h"

static EpdSsd2683 epd;

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

static void drawConfigWebPage(bool ok, bool forced_ap) {
  if (!epd.ok()) return;
  epd.clear(EPD_WHITE);
  epd.fillRect(0, 0, 400, 22, EPD_BLACK);
  epd.drawText(8, 7, "CONFIG WEB", EPD_WHITE, EPD_BLACK);

  if (!ok) {
    epd.drawText(8, 40, "Web FAILED", EPD_RED);
    epd.drawText(8, 60, "AP needs pass >= 8 chars", EPD_BLACK);
    epd.drawText(8, 76, "Retry: hold UP 1.5s", EPD_BLACK);
    epd.drawText(8, 100, "See USB serial log", EPD_BLACK);
    epd.display();
    return;
  }

  epd.drawText(8, 40, webModeName(), EPD_BLACK);
  epd.drawText(8, 56, webUrl(), EPD_RED);
  if (strcmp(webModeName(), "AP") == 0 || forced_ap) {
    char line[48];
    snprintf(line, sizeof(line), "SSID %s", WIFI_AP_SSID);
    epd.drawText(8, 80, line, EPD_BLACK);
    snprintf(line, sizeof(line), "PASS %s", WIFI_AP_PASS);
    epd.drawText(8, 96, line, EPD_BLACK);
    epd.drawText(8, 112, "Phone join AP, open URL", EPD_BLACK);
  } else {
    epd.drawText(8, 80, "Same WiFi as phone", EPD_BLACK);
    epd.drawText(8, 96, "Open URL in browser", EPD_BLACK);
  }
  epd.drawText(8, 140, "Tabs: WiFi / Auth / Todo", EPD_BLACK);
  epd.drawText(8, 164, "UP hold: force AP", EPD_BLACK);
  epd.drawText(8, 180, "BOOT: refresh diary", EPD_BLACK);
  epd.display();
}

void setup() {
  powerBegin();  // PWR_HOLD high + DOWN pin
  ledBegin();    // default OFF

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
  Serial.printf("======== NOTE4C todo %s ========\n", APP_VERSION);

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
  todoRefreshAndDraw(epd);
  ledOff();  // idle: LED must stay off (active-low → GPIO HIGH)
}

void loop() {
  static uint32_t up_since = 0;
  static bool boot_prev = false;
  static bool up_prev = false;
  static bool up_long_done = false;
  static bool busy = false;

  powerPoll();  // DOWN hold 3s → power off (any time loop runs)
  ledUpdate();
  webLoop();

  const bool boot = digitalRead(PIN_BTN_BOOT) == LOW;
  const bool up = digitalRead(PIN_BTN_UP) == LOW;

  // BOOT release → refresh todo
  if (boot_prev && !boot && !busy) {
    busy = true;
    ledSet(LedPattern::BlinkFast);
    Serial.println("[UI] BOOT -> refresh todo");
    const bool ok = todoRefreshAndDraw(epd);
    ledSet(ok ? LedPattern::Off : LedPattern::ErrorBurst);
    if (!ok) {
      // show error pattern briefly then off
      const uint32_t t = millis();
      while (millis() - t < 1800) {
        ledUpdate();
        delay(10);
      }
      ledOff();
    }
    busy = false;
  }
  boot_prev = boot;

  // UP press tracking: long hold → force AP; short release → STA-prefer web
  if (up) {
    if (up_since == 0) {
      up_since = millis();
      up_long_done = false;
    }
    if (!busy && !up_long_done && (millis() - up_since > 1500)) {
      up_long_done = true;
      busy = true;
      ledSet(LedPattern::BlinkSlow);
      Serial.println("[UI] UP long -> force AP web");
      const bool ok = webStart(true);
      drawConfigWebPage(ok, true);
      ledSet(ok ? LedPattern::DoublePulse : LedPattern::ErrorBurst);
      busy = false;
    }
  } else {
    if (up_prev && !busy && !up_long_done && up_since != 0) {
      // short press
      busy = true;
      ledSet(LedPattern::BlinkSlow);
      Serial.println("[UI] UP short -> config web");
      const bool ok = webStart(false);
      drawConfigWebPage(ok, false);
      ledSet(ok ? LedPattern::DoublePulse : LedPattern::ErrorBurst);
      if (!ok) {
        const uint32_t t = millis();
        while (millis() - t < 1800) {
          ledUpdate();
          delay(10);
        }
        ledOff();
      }
      busy = false;
    }
    up_since = 0;
    up_long_done = false;
  }
  up_prev = up;

  // Idle / web-ready: keep DoublePulse only while web running; else off
  if (!busy && webIsRunning() && ledGet() != LedPattern::DoublePulse &&
      ledGet() != LedPattern::ErrorBurst) {
    ledSet(LedPattern::DoublePulse);
  }
  if (!busy && !webIsRunning() && ledGet() == LedPattern::DoublePulse) {
    ledOff();
  }
}
