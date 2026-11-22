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
  void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, EpdColor color);
  void drawCircle(int cx, int cy, int r, EpdColor color);
  void fillCircle(int cx, int cy, int r, EpdColor color);
  // 6x8 glyph; returns advance in pixels
  int drawChar(int x, int y, char c, EpdColor fg, EpdColor bg = EPD_WHITE);
  void drawText(int x, int y, const char *text, EpdColor fg, EpdColor bg = EPD_WHITE);
  // Full refresh (~10s+). Panel powered down afterward.
  // allow_interrupt: nav keys can abort mid-waveform so UI can switch immediately.
  // Returns true only if the refresh finished; false on fail or abort (see wasAborted).
  bool display(bool allow_interrupt = true);

  // Next display() ignores allow_interrupt and forces non-interruptible (timer-wake etc.).
  void suppressInterruptOnce() { suppress_interrupt_once_ = true; }

  bool wasAborted() const { return aborted_; }
  uint8_t *framebuffer() { return fb_; }
  bool ok() const { return ok_; }

 private:
  void powerOn();
  void powerOff();
  void stopPanel();
  // false = aborted by key (only when allow_interrupt_); timeout still returns true.
  bool waitBusy(uint32_t timeout_ms = 120000);
  void sendCommand(uint8_t cmd);
  void sendData(uint8_t data);
  void writeBytes(const uint8_t *buf, int len);
  void hwReset();
  void initPanel();
  bool turnOnDisplay();

  SPIClass *spi_ = nullptr;
  uint8_t *fb_ = nullptr;
  int pin_cs_ = -1, pin_dc_ = -1, pin_rst_ = -1, pin_busy_ = -1, pin_pwr_ = -1;
  bool ok_ = false;
  bool allow_interrupt_ = false;
  bool aborted_ = false;
  bool suppress_interrupt_once_ = false;
};
