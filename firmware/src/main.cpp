/**
 * NOTE4C hardware self-test — Serial + e-paper status page
 *
 * Full EPD refresh is slow (~10s+). Screen updates:
 *   - once at boot
 *   - again when BOOT is short-pressed
 */

#include <Arduino.h>
#include <Wire.h>
#include <esp_chip_info.h>
#include <esp_flash.h>
#include <esp_system.h>
#include <stdio.h>

#include "board.h"
#include "epd_ssd2683.h"

static EpdSsd2683 epd;

struct HwStatus {
  bool boot, up, down;
  float bat_mv;
  int chrg, stdby, nfc_field, rtc_int, busy;
  bool rtc_ok, nfc_ok, es_ok;
  uint32_t heap_free, psram_free;
};

static bool i2cAck(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

static void i2cScan(const char *tag) {
  Serial.printf("[I2C] scan (%s)\n", tag);
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    if (!i2cAck(addr)) continue;
    found++;
    const char *name = "";
    if (addr == I2C_ADDR_RTC) name = "  << PCF8563 RTC";
    else if (addr == I2C_ADDR_NFC) name = "  << NFC";
    else if (addr == I2C_ADDR_ES8311) name = "  << ES8311?";
    Serial.printf("  - 0x%02X%s\n", addr, name);
  }
  if (!found) Serial.println("  (none)");
}

static float readBatteryApproxMv() {
  const int samples = 16;
  uint32_t sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogReadMilliVolts(PIN_BAT_ADC);
    delay(2);
  }
  return (sum / (float)samples) * 2.0f;  // ~2:1 divider estimate
}

static HwStatus sampleStatus() {
  HwStatus s{};
  s.boot = digitalRead(PIN_BTN_BOOT) == LOW;
  s.up = digitalRead(PIN_BTN_UP) == LOW;
  s.down = digitalRead(PIN_BTN_DOWN) == LOW;
  s.bat_mv = readBatteryApproxMv();
  s.chrg = digitalRead(PIN_CHRG);
  s.stdby = digitalRead(PIN_STDBY);
  s.nfc_field = digitalRead(PIN_NFC_FIELD);
  s.rtc_int = digitalRead(PIN_RTC_INT);
  s.busy = digitalRead(PIN_EPD_BUSY);
  s.rtc_ok = i2cAck(I2C_ADDR_RTC);
  s.nfc_ok = i2cAck(I2C_ADDR_NFC);
  s.es_ok = i2cAck(I2C_ADDR_ES8311);
  s.heap_free = ESP.getFreeHeap();
  s.psram_free = ESP.getFreePsram();
  return s;
}

static const char *okStr(bool v) { return v ? "OK" : "--"; }

static void drawStatusPage(const HwStatus &s) {
  char line[48];

  epd.clear(EPD_WHITE);

  // Color swatches (verify BWRY)
  epd.fillRect(0, 0, 100, 16, EPD_BLACK);
  epd.fillRect(100, 0, 100, 16, EPD_WHITE);
  epd.drawRect(100, 0, 100, 16, EPD_BLACK);
  epd.fillRect(200, 0, 100, 16, EPD_RED);
  epd.fillRect(300, 0, 100, 16, EPD_YELLOW);

  epd.drawText(8, 24, "NOTE4C SELFTEST", EPD_BLACK);
  epd.drawText(8, 36, "SSD2683 400x300 BWRY", EPD_BLACK);
  epd.drawHLine(8, 48, 384, EPD_BLACK);

  snprintf(line, sizeof(line), "PWR HOLD: ON");
  epd.drawText(8, 56, line, EPD_BLACK);

  snprintf(line, sizeof(line), "BTN BOOT=%d UP=%d DOWN=%d", s.boot, s.up, s.down);
  epd.drawText(8, 70, line, EPD_BLACK);

  snprintf(line, sizeof(line), "BAT ~%.0f mV (approx)", s.bat_mv);
  epd.drawText(8, 84, line, EPD_BLACK);

  snprintf(line, sizeof(line), "CHRG=%d  STDBY=%d", s.chrg, s.stdby);
  epd.drawText(8, 98, line, EPD_BLACK);

  snprintf(line, sizeof(line), "RTC %s  NFC %s  ES8311 %s",
           okStr(s.rtc_ok), okStr(s.nfc_ok), okStr(s.es_ok));
  epd.drawText(8, 112, line, s.rtc_ok ? EPD_BLACK : EPD_RED);

  snprintf(line, sizeof(line), "NFC_FIELD=%d  RTC_INT=%d", s.nfc_field, s.rtc_int);
  epd.drawText(8, 126, line, EPD_BLACK);

  snprintf(line, sizeof(line), "EPD_BUSY=%d", s.busy);
  epd.drawText(8, 140, line, EPD_BLACK);

  snprintf(line, sizeof(line), "Heap %lu  PSRAM %lu",
           (unsigned long)s.heap_free, (unsigned long)s.psram_free);
  epd.drawText(8, 154, line, EPD_BLACK);

  epd.drawHLine(8, 170, 384, EPD_BLACK);
  epd.drawText(8, 180, "Legend: OK=ACK  --=missing", EPD_BLACK);
  epd.drawText(8, 194, "BOOT: refresh screen", EPD_YELLOW);
  epd.drawText(8, 208, "DOWN 3s: power off (battery)", EPD_RED);
  epd.drawText(8, 230, "Full refresh ~10s+ is normal", EPD_BLACK);

  epd.display();
}

static void printChipInfo() {
  esp_chip_info_t chip;
  esp_chip_info(&chip);
  uint32_t flash_size = 0;
  esp_flash_get_size(NULL, &flash_size);

  Serial.println("======== NOTE4C hardware self-test ========");
  Serial.printf("SDK: %s\n", ESP.getSdkVersion());
  Serial.printf("Chip: %s rev%d cores=%d\n", ESP.getChipModel(), chip.revision, chip.cores);
  Serial.printf("Flash: %u MB | PSRAM free: %u\n",
                (unsigned)(flash_size / (1024 * 1024)), (unsigned)ESP.getFreePsram());
  Serial.println("===========================================");
}

void setup() {
  pinMode(PIN_PWR_HOLD, OUTPUT);
  digitalWrite(PIN_PWR_HOLD, HIGH);

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  pinMode(PIN_BTN_BOOT, INPUT_PULLUP);
  pinMode(PIN_BTN_UP, INPUT_PULLUP);
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
  pinMode(PIN_STDBY, INPUT);
  pinMode(PIN_CHRG, INPUT);
  pinMode(PIN_RTC_INT, INPUT);
  pinMode(PIN_NFC_FIELD, INPUT);

  pinMode(PIN_NFC_PWR, OUTPUT);
  pinMode(PIN_AUDIO_PWR, OUTPUT);
  pinMode(PIN_SPK_PA, OUTPUT);
  digitalWrite(PIN_NFC_PWR, LOW);
  digitalWrite(PIN_AUDIO_PWR, LOW);
  digitalWrite(PIN_SPK_PA, LOW);

  analogReadResolution(12);

  Serial.begin(115200);
  delay(800);
  printChipInfo();

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(100000);
  i2cScan("rails OFF");

  digitalWrite(PIN_AUDIO_PWR, HIGH);
  digitalWrite(PIN_NFC_PWR, HIGH);
  delay(50);
  i2cScan("audio+NFC ON");

  if (!epd.begin(PIN_EPD_SCK, PIN_EPD_MOSI, PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY,
                 PIN_EPD_PWR)) {
    Serial.println("[EPD] begin failed — serial-only mode");
    return;
  }

  const HwStatus s = sampleStatus();
  Serial.printf(
      "BTN B=%d U=%d D=%d | BAT~%.0fmV | CHRG=%d STDBY=%d | RTC=%s NFC=%s ES=%s\n",
      s.boot, s.up, s.down, s.bat_mv, s.chrg, s.stdby, okStr(s.rtc_ok), okStr(s.nfc_ok),
      okStr(s.es_ok));

  drawStatusPage(s);
  Serial.println("Screen updated. Short-press BOOT to refresh again.");
}

void loop() {
  static uint32_t last_print = 0;
  static uint32_t down_since = 0;
  static bool boot_prev = false;
  static bool led_on = false;
  static bool refreshing = false;

  const bool boot = digitalRead(PIN_BTN_BOOT) == LOW;
  const bool up = digitalRead(PIN_BTN_UP) == LOW;
  const bool down = digitalRead(PIN_BTN_DOWN) == LOW;

  if (down) {
    if (down_since == 0) down_since = millis();
    if (millis() - down_since > 3000) {
      Serial.println("[PWR] releasing PWR_HOLD");
      digitalWrite(PIN_PWR_HOLD, LOW);
      delay(1000);
    }
  } else {
    down_since = 0;
  }

  // Rising edge of BOOT (press then release) triggers redraw — wait for release
  // so we don't start a 10s refresh while finger still holds BOOT.
  if (boot_prev && !boot && epd.ok() && !refreshing) {
    refreshing = true;
    Serial.println("[EPD] BOOT release -> refresh status page");
    digitalWrite(PIN_LED, HIGH);
    drawStatusPage(sampleStatus());
    digitalWrite(PIN_LED, LOW);
    refreshing = false;
  }
  boot_prev = boot;

  if (boot) {
    digitalWrite(PIN_LED, HIGH);
  } else if (millis() - last_print >= 1000) {
    led_on = !led_on;
    digitalWrite(PIN_LED, led_on ? HIGH : LOW);
  }

  if (millis() - last_print < 1000) return;
  last_print = millis();

  const HwStatus s = sampleStatus();
  Serial.printf(
      "BTN BOOT=%s UP=%s DOWN=%s | BAT~%.0fmV | CHRG=%d STDBY=%d | RTC=%s NFC=%s ES=%s\n",
      boot ? "DOWN" : "up  ", up ? "DOWN" : "up  ", down ? "DOWN" : "up  ", s.bat_mv, s.chrg,
      s.stdby, okStr(s.rtc_ok), okStr(s.nfc_ok), okStr(s.es_ok));
}
