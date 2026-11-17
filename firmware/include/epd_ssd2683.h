#pragma once

#include <Arduino.h>
#include <SPI.h>

// Colors match LazyYoun/rawdraw 2bpp BWRY packing for SSD2683.
enum EpdColor : uint8_t {
  EPD_BLACK = 0,
  EPD_WHITE = 1,
  EPD_YELLOW = 2,
  EPD_RED = 3,
};

class EpdSsd2683 {
 public:
  static constexpr int kWidth = 400;
  static constexpr int kHeight = 300;
  static constexpr int kBytesPerRow = (kWidth * 2 + 7) / 8;  // 100
  static constexpr int kBufferLen = kBytesPerRow * kHeight;  // 30000

  bool begin(int sck, int mosi, int cs, int dc, int rst, int busy, int pwr);
  void clear(EpdColor color = EPD_WHITE);
  void setPixel(int x, int y, EpdColor color);
  void fillRect(int x, int y, int w, int h, EpdColor color);
  void drawHLine(int x, int y, int w, EpdColor color);
  void drawVLine(int x, int y, int h, EpdColor color);
  void drawRect(int x, int y, int w, int h, EpdColor color);
  // 6x8 glyph; returns advance in pixels
  int drawChar(int x, int y, char c, EpdColor fg, EpdColor bg = EPD_WHITE);
  void drawText(int x, int y, const char *text, EpdColor fg, EpdColor bg = EPD_WHITE);
  // Full refresh (~10s+). Panel powered down afterward.
  bool display();

  uint8_t *framebuffer() { return fb_; }
  bool ok() const { return ok_; }

 private:
  void powerOn();
  void powerOff();
  void waitBusy(uint32_t timeout_ms = 120000);
  void sendCommand(uint8_t cmd);
  void sendData(uint8_t data);
  void writeBytes(const uint8_t *buf, int len);
  void hwReset();
  void initPanel();
  void turnOnDisplay();

  SPIClass *spi_ = nullptr;
  uint8_t *fb_ = nullptr;
  int pin_cs_ = -1, pin_dc_ = -1, pin_rst_ = -1, pin_busy_ = -1, pin_pwr_ = -1;
  bool ok_ = false;
};
