/**
 * Minimal SSD2683 4-color (BWRY) driver for NOTE4C.
 *
 * Init / refresh sequence adapted from LazyYoun youn-ink-fourcolor-firmware
 * (MIT): custom_lcd_display.cc four-color path (cmd 0xE9 / 0x10 / 0x12 ...).
 */

#include "epd_ssd2683.h"
#include "font6x8.h"
#include "app_power.h"
#include "app_led.h"

bool EpdSsd2683::begin(int sck, int mosi, int cs, int dc, int rst, int busy, int pwr) {
  pin_cs_ = cs;
  pin_dc_ = dc;
  pin_rst_ = rst;
  pin_busy_ = busy;
  pin_pwr_ = pwr;

  pinMode(pin_cs_, OUTPUT);
  pinMode(pin_dc_, OUTPUT);
  pinMode(pin_rst_, OUTPUT);
  pinMode(pin_busy_, INPUT);
  pinMode(pin_pwr_, OUTPUT);
  digitalWrite(pin_cs_, HIGH);
  digitalWrite(pin_dc_, HIGH);
  digitalWrite(pin_rst_, HIGH);
  digitalWrite(pin_pwr_, LOW);

  fb_ = (uint8_t *)ps_malloc(kBufferLen);
  if (!fb_) fb_ = (uint8_t *)malloc(kBufferLen);
  if (!fb_) {
    Serial.println("[EPD] framebuffer alloc failed");
    return false;
  }
  memset(fb_, 0x55, kBufferLen);  // white = 0b01 repeated -> 0x55

  // FSPI on ESP32-S3; CS/DC driven manually like the IDF reference.
  spi_ = new SPIClass(FSPI);
  spi_->begin(sck, -1, mosi, -1);

  ok_ = true;
  return true;
}

void EpdSsd2683::powerOn() {
  digitalWrite(pin_pwr_, HIGH);
  delay(10);
}

void EpdSsd2683::powerOff() {
  digitalWrite(pin_pwr_, LOW);
}

void EpdSsd2683::waitBusy(uint32_t timeout_ms) {
  const uint32_t start = millis();
  while (digitalRead(pin_busy_) == LOW) {
    if (millis() - start > timeout_ms) {
      Serial.println("[EPD] busy timeout");
      break;
    }
    powerPoll();  // allow DOWN 3s power-off during full refresh
    ledUpdate();
    delay(20);
  }
}

void EpdSsd2683::sendCommand(uint8_t cmd) {
  digitalWrite(pin_dc_, LOW);
  digitalWrite(pin_cs_, LOW);
  spi_->beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
  spi_->transfer(cmd);
  spi_->endTransaction();
  digitalWrite(pin_cs_, HIGH);
}

void EpdSsd2683::sendData(uint8_t data) {
  digitalWrite(pin_dc_, HIGH);
  digitalWrite(pin_cs_, LOW);
  spi_->beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
  spi_->transfer(data);
  spi_->endTransaction();
  digitalWrite(pin_cs_, HIGH);
}

void EpdSsd2683::writeBytes(const uint8_t *buf, int len) {
  digitalWrite(pin_dc_, HIGH);
  digitalWrite(pin_cs_, LOW);
  spi_->beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
  spi_->writeBytes(buf, len);
  spi_->endTransaction();
  digitalWrite(pin_cs_, HIGH);
}

void EpdSsd2683::hwReset() {
  digitalWrite(pin_rst_, HIGH);
  delay(10);
  digitalWrite(pin_rst_, LOW);
  delay(20);
  digitalWrite(pin_rst_, HIGH);
  delay(10);
  waitBusy();
}

void EpdSsd2683::initPanel() {
  // Four-color NOTE4C path from reference firmware.
  sendCommand(0xE9);
  sendData(0x01);
}

void EpdSsd2683::turnOnDisplay() {
  sendCommand(0x04);  // power on
  waitBusy();
  delay(10);

  sendCommand(0x12);  // display refresh
  sendData(0x00);
  delay(10);
  waitBusy();

  sendCommand(0x02);  // power off
  sendData(0x00);
  waitBusy();
  delay(20);

  sendCommand(0x07);  // deep sleep
  sendData(0xA5);
  powerOff();
}

void EpdSsd2683::clear(EpdColor color) {
  const uint8_t c = color & 0x03;
  const uint8_t b = (c << 6) | (c << 4) | (c << 2) | c;
  memset(fb_, b, kBufferLen);
}

void EpdSsd2683::setPixel(int x, int y, EpdColor color) {
  if (x < 0 || y < 0 || x >= kWidth || y >= kHeight || !fb_) return;
  const int index = y * kBytesPerRow + (x >> 2);
  const int shift = 6 - 2 * (x & 3);
  const uint8_t mask = 0x03 << shift;
  fb_[index] = (fb_[index] & ~mask) | ((color & 0x03) << shift);
}

void EpdSsd2683::fillRect(int x, int y, int w, int h, EpdColor color) {
  for (int yy = y; yy < y + h; yy++) {
    for (int xx = x; xx < x + w; xx++) setPixel(xx, yy, color);
  }
}

void EpdSsd2683::drawHLine(int x, int y, int w, EpdColor color) {
  for (int i = 0; i < w; i++) setPixel(x + i, y, color);
}

void EpdSsd2683::drawVLine(int x, int y, int h, EpdColor color) {
  for (int i = 0; i < h; i++) setPixel(x, y + i, color);
}

void EpdSsd2683::drawRect(int x, int y, int w, int h, EpdColor color) {
  drawHLine(x, y, w, color);
  drawHLine(x, y + h - 1, w, color);
  drawVLine(x, y, h, color);
  drawVLine(x + w - 1, y, h, color);
}

int EpdSsd2683::drawChar(int x, int y, char c, EpdColor fg, EpdColor bg) {
  if (c < 0x20 || c > 0x7E) c = '?';
  const uint8_t *glyph = FONT6X8[c - 0x20];
  for (int col = 0; col < 6; col++) {
    uint8_t bits = glyph[col];
    for (int row = 0; row < 8; row++) {
      setPixel(x + col, y + row, (bits & (1 << row)) ? fg : bg);
    }
  }
  return 6;
}

void EpdSsd2683::drawText(int x, int y, const char *text, EpdColor fg, EpdColor bg) {
  int cx = x;
  for (const char *p = text; *p; p++) {
    if (*p == '\n') {
      cx = x;
      y += 10;
      continue;
    }
    cx += drawChar(cx, y, *p, fg, bg);
  }
}

bool EpdSsd2683::display() {
  if (!ok_ || !fb_) return false;
  Serial.println("[EPD] full refresh start (may take 10s+)");
  const uint32_t t0 = millis();

  powerOn();
  hwReset();
  initPanel();

  sendCommand(0x10);  // DTM1 write
  waitBusy();
  for (int y = 0; y < kHeight; y++) {
    writeBytes(fb_ + y * kBytesPerRow, kBytesPerRow);
    if ((y & 15) == 15) delay(1);
  }
  turnOnDisplay();

  Serial.printf("[EPD] full refresh done in %lu ms\n", (unsigned long)(millis() - t0));
  return true;
}
