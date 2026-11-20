#include "app_font.h"
#include "app_power.h"

#include <FS.h>
#include <LittleFS.h>
#include <cstring>

static uint8_t *g_hzk = nullptr;
static uint8_t *g_asc = nullptr;
static uint8_t *g_uni2gb = nullptr;
static size_t g_uni2gb_count = 0;
static bool g_ready = false;

static constexpr int kLineH = 16;
static constexpr int kLineHSmall = 10;

bool fontReady() { return g_ready; }
int fontLineHeight() { return kLineH; }
int fontLineHeightSmall() { return kLineHSmall; }
int fontLineHeightDouble() { return kLineH * 2; }

static bool loadFile(const char *path, uint8_t **out, size_t expect = 0) {
  File f = LittleFS.open(path, "r");
  if (!f) {
    Serial.printf("[Font] missing %s\n", path);
    return false;
  }
  const size_t n = f.size();
  if (expect && n != expect) {
    Serial.printf("[Font] %s size %u expected %u\n", path, (unsigned)n, (unsigned)expect);
  }
  uint8_t *buf = (uint8_t *)ps_malloc(n);
  if (!buf) buf = (uint8_t *)malloc(n);
  if (!buf) {
    f.close();
    return false;
  }
  if (f.read(buf, n) != (int)n) {
    free(buf);
    f.close();
    return false;
  }
  f.close();
  *out = buf;
  return true;
}

bool fontBegin() {
  if (g_ready) return true;
  if (!LittleFS.begin(true)) {
    Serial.println("[Font] LittleFS mount failed");
    return false;
  }
  if (!loadFile("/HZK16", &g_hzk) || !loadFile("/ASC16", &g_asc) || !loadFile("/uni2gb.bin", &g_uni2gb)) {
    Serial.println("[Font] load failed — ASCII 6x8 fallback only for CJK boxes");
    return false;
  }
  g_uni2gb_count = 29780 / 4;  // generated size; recompute from actual
  // Prefer actual size
  File f = LittleFS.open("/uni2gb.bin", "r");
  if (f) {
    g_uni2gb_count = f.size() / 4;
    f.close();
  }
  g_ready = true;
  Serial.printf("[Font] ready HZK=%p ASC=%p uni2gb=%u\n", g_hzk, g_asc, (unsigned)g_uni2gb_count);
  return true;
}

static uint16_t unicodeToGb(uint32_t cp) {
  if (!g_uni2gb || g_uni2gb_count == 0) return 0;
  // binary search on uint16 pairs little-endian
  int lo = 0, hi = (int)g_uni2gb_count - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    const uint16_t u = g_uni2gb[mid * 4] | (g_uni2gb[mid * 4 + 1] << 8);
    if (u == cp) {
      return g_uni2gb[mid * 4 + 2] | (g_uni2gb[mid * 4 + 3] << 8);
    }
    if (u < cp) lo = mid + 1;
    else hi = mid - 1;
  }
  return 0;
}

static int utf8Next(const char *&p, uint32_t &cp) {
  const uint8_t c = (uint8_t)*p;
  if (c == 0) return 0;
  if (c < 0x80) {
    cp = c;
    p++;
    return 1;
  }
  if ((c & 0xE0) == 0xC0 && p[1]) {
    cp = ((c & 0x1F) << 6) | ((uint8_t)p[1] & 0x3F);
    p += 2;
    return 2;
  }
  if ((c & 0xF0) == 0xE0 && p[1] && p[2]) {
    cp = ((c & 0x0F) << 12) | (((uint8_t)p[1] & 0x3F) << 6) | ((uint8_t)p[2] & 0x3F);
    p += 3;
    return 3;
  }
  if ((c & 0xF8) == 0xF0 && p[1] && p[2] && p[3]) {
    cp = ((c & 0x07) << 18) | (((uint8_t)p[1] & 0x3F) << 12) | (((uint8_t)p[2] & 0x3F) << 6) |
         ((uint8_t)p[3] & 0x3F);
    p += 4;
    return 4;
  }
  cp = '?';
  p++;
  return 1;
}

static void blitScaled(EpdSsd2683 &epd, int x, int y, int col, int row, int scale, EpdColor c) {
  if (scale <= 1) {
    epd.setPixel(x + col, y + row, c);
    return;
  }
  epd.fillRect(x + col * scale, y + row * scale, scale, scale, c);
}

static void blitScaledDither(EpdSsd2683 &epd, int x, int y, int col, int row, int scale, EpdColor a,
                             EpdColor b) {
  const int px = x + col * scale;
  const int py = y + row * scale;
  if (scale <= 1) {
    epd.setPixel(px, py, ((px + py) & 1) ? b : a);
    return;
  }
  for (int dy = 0; dy < scale; dy++) {
    for (int dx = 0; dx < scale; dx++) {
      epd.setPixel(px + dx, py + dy, ((px + dx + py + dy) & 1) ? b : a);
    }
  }
}

static void drawAscDither(EpdSsd2683 &epd, int x, int y, char c, EpdColor a, EpdColor b, EpdColor bg,
                          int scale) {
  if (!g_asc || (uint8_t)c > 127) c = '?';
  const uint8_t *glyph = g_asc + (uint8_t)c * 16;
  for (int row = 0; row < 16; row++) {
    uint8_t bits = glyph[row];
    for (int col = 0; col < 8; col++) {
      const bool on = (bits & (0x80 >> col)) != 0;
      if (on) blitScaledDither(epd, x, y, col, row, scale, a, b);
      else blitScaled(epd, x, y, col, row, scale, bg);
    }
  }
}

static void drawHzkDither(EpdSsd2683 &epd, int x, int y, uint16_t gb, EpdColor a, EpdColor b,
                          EpdColor bg, int scale) {
  const int sc = scale <= 1 ? 1 : scale;
  const int box = 16 * sc;
  if (!g_hzk || gb < 0xA1A1) {
    epd.drawRect(x, y, box, box, a);
    return;
  }
  const uint8_t hi = (gb >> 8) & 0xFF;
  const uint8_t lo = gb & 0xFF;
  if (hi < 0xA1 || lo < 0xA1) {
    epd.drawRect(x, y, box, box, a);
    return;
  }
  const uint32_t offset = (uint32_t)(94 * (hi - 0xA1) + (lo - 0xA1)) * 32;
  const uint8_t *m = g_hzk + offset;
  for (int row = 0; row < 16; row++) {
    for (int byteCol = 0; byteCol < 2; byteCol++) {
      uint8_t bits = m[row * 2 + byteCol];
      for (int bit = 0; bit < 8; bit++) {
        const bool on = (bits & (0x80 >> bit)) != 0;
        const int col = byteCol * 8 + bit;
        if (on) blitScaledDither(epd, x, y, col, row, sc, a, b);
        else blitScaled(epd, x, y, col, row, sc, bg);
      }
    }
  }
}

static void drawAsc(EpdSsd2683 &epd, int x, int y, char c, EpdColor fg, EpdColor bg, int scale = 1) {
  if (!g_asc || (uint8_t)c > 127) c = '?';
  const uint8_t *glyph = g_asc + (uint8_t)c * 16;
  for (int row = 0; row < 16; row++) {
    uint8_t bits = glyph[row];
    for (int col = 0; col < 8; col++) {
      const bool on = (bits & (0x80 >> col)) != 0;
      blitScaled(epd, x, y, col, row, scale, on ? fg : bg);
    }
  }
}

static void drawHzk(EpdSsd2683 &epd, int x, int y, uint16_t gb, EpdColor fg, EpdColor bg, int scale = 1) {
  const int sc = scale <= 1 ? 1 : scale;
  const int box = 16 * sc;
  if (!g_hzk || gb < 0xA1A1) {
    epd.drawRect(x, y, box, box, fg);
    return;
  }
  const uint8_t hi = (gb >> 8) & 0xFF;
  const uint8_t lo = gb & 0xFF;
  if (hi < 0xA1 || lo < 0xA1) {
    epd.drawRect(x, y, box, box, fg);
    return;
  }
  const uint32_t offset = (uint32_t)(94 * (hi - 0xA1) + (lo - 0xA1)) * 32;
  const uint8_t *m = g_hzk + offset;
  for (int row = 0; row < 16; row++) {
    for (int byteCol = 0; byteCol < 2; byteCol++) {
      uint8_t bits = m[row * 2 + byteCol];
      for (int bit = 0; bit < 8; bit++) {
        const bool on = (bits & (0x80 >> bit)) != 0;
        blitScaled(epd, x, y, byteCol * 8 + bit, row, sc, on ? fg : bg);
      }
    }
  }
}

// ℃：字库有则用 HZK；否则 16×16 自制（ASC「C」+ 左上 3×3 方点）
static void drawCelsiusCustom(EpdSsd2683 &epd, int x, int y, EpdColor fg, EpdColor bg, int scale,
                              bool dither, EpdColor dither_b) {
  const int sc = scale <= 1 ? 1 : scale;
  for (int row = 0; row < 16; row++) {
    for (int col = 0; col < 16; col++) {
      blitScaled(epd, x, y, col, row, sc, bg);
    }
  }
  // ASC C 偏右下，给左上角留出圆点位
  if (g_asc) {
    const uint8_t *glyph = g_asc + (uint8_t)'C' * 16;
    constexpr int kOx = 5;
    constexpr int kOy = 1;
    for (int row = 0; row < 16; row++) {
      if (kOy + row >= 16) break;
      uint8_t bits = glyph[row];
      for (int col = 0; col < 8; col++) {
        if (!(bits & (0x80 >> col))) continue;
        const int c = kOx + col;
        const int r = kOy + row;
        if (c < 0 || c >= 16 || r < 0 || r >= 16) continue;
        if (dither) blitScaledDither(epd, x, y, c, r, sc, fg, dither_b);
        else blitScaled(epd, x, y, c, r, sc, fg);
      }
    }
  }
  // 左上 3×3 方点
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 3; col++) {
      if (dither) blitScaledDither(epd, x, y, col, row, sc, fg, dither_b);
      else blitScaled(epd, x, y, col, row, sc, fg);
    }
  }
}

static void drawCelsius(EpdSsd2683 &epd, int x, int y, EpdColor fg, EpdColor bg, int scale = 1) {
  const uint16_t gb = unicodeToGb(0x2103);
  if (gb) {
    drawHzk(epd, x, y, gb, fg, bg, scale);
    return;
  }
  drawCelsiusCustom(epd, x, y, fg, bg, scale, false, fg);
}

static void drawCelsiusDither(EpdSsd2683 &epd, int x, int y, EpdColor a, EpdColor b, EpdColor bg,
                              int scale) {
  const uint16_t gb = unicodeToGb(0x2103);
  if (gb) {
    drawHzkDither(epd, x, y, gb, a, b, bg, scale);
    return;
  }
  drawCelsiusCustom(epd, x, y, a, bg, scale, true, b);
}

static int glyphWidth(uint32_t cp) {
  if (cp == '\n' || cp == '\r') return 0;
  if (cp < 0x80) return 8;
  return 16;
}

static int glyphWidthSmall(uint32_t cp) {
  if (cp == '\n' || cp == '\r') return 0;
  if (cp < 0x80) return 4;
  return 8;
}

static void drawAscSmall(EpdSsd2683 &epd, int x, int y, char c, EpdColor fg, EpdColor bg) {
  if (!g_asc || (uint8_t)c > 127) c = '?';
  const uint8_t *glyph = g_asc + (uint8_t)c * 16;
  for (int row = 0; row < 8; row++) {
    // OR two source rows for denser half-height
    const uint8_t bits = (uint8_t)(glyph[row * 2] | glyph[row * 2 + 1]);
    for (int col = 0; col < 4; col++) {
      const bool on = ((bits & (0x80 >> (col * 2))) != 0) || ((bits & (0x40 >> (col * 2))) != 0);
      epd.setPixel(x + col, y + row, on ? fg : bg);
    }
  }
}

static void drawHzkSmall(EpdSsd2683 &epd, int x, int y, uint16_t gb, EpdColor fg, EpdColor bg) {
  if (!g_hzk || gb < 0xA1A1) {
    epd.drawRect(x, y, 8, 8, fg);
    return;
  }
  const uint8_t hi = (gb >> 8) & 0xFF;
  const uint8_t lo = gb & 0xFF;
  if (hi < 0xA1 || lo < 0xA1) {
    epd.drawRect(x, y, 8, 8, fg);
    return;
  }
  const uint32_t offset = (uint32_t)(94 * (hi - 0xA1) + (lo - 0xA1)) * 32;
  const uint8_t *m = g_hzk + offset;
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      const int sr0 = row * 2;
      const int sr1 = sr0 + 1;
      const int sc0 = col * 2;
      const int sc1 = sc0 + 1;
      auto srcOn = [&](int sr, int sc) -> bool {
        const uint8_t bits = m[sr * 2 + (sc / 8)];
        return (bits & (0x80 >> (sc % 8))) != 0;
      };
      const bool on = srcOn(sr0, sc0) || srcOn(sr0, sc1) || srcOn(sr1, sc0) || srcOn(sr1, sc1);
      epd.setPixel(x + col, y + row, on ? fg : bg);
    }
  }
}

int fontDrawTextWrapped(EpdSsd2683 &epd, int x, int y, int max_w, int max_y, const char *utf8,
                        EpdColor fg, EpdColor bg, int wrap_x, bool strike, EpdColor strike_color) {
  if (!utf8) return y;
  const int origin_x = (wrap_x >= 0) ? wrap_x : x;
  const int right = origin_x + max_w;
  int cx = x;
  int cy = y;
  int line_start_x = x;
  bool drew = false;
  bool line_has_glyph = false;
  const char *p = utf8;

  auto strikeLine = [&]() {
    if (!strike || !line_has_glyph) return;
    const int w = cx - line_start_x;
    if (w > 0) epd.drawHLine(line_start_x, cy + kLineH / 2, w, strike_color);
  };

  auto newline = [&]() {
    strikeLine();
    cx = origin_x;
    line_start_x = origin_x;
    line_has_glyph = false;
    cy += kLineH;
  };

  int glyphs = 0;
  while (*p && cy + kLineH <= max_y) {
    if ((++glyphs & 15) == 0) wdtPet();
    if (*p == '\r') {
      p++;
      continue;
    }
    if (*p == '\n') {
      p++;
      newline();
      continue;
    }

    uint32_t cp = 0;
    utf8Next(p, cp);
    const int w = glyphWidth(cp);
    if (cx + w > right && cx > origin_x) {
      newline();
      if (cy + kLineH > max_y) break;
    }

    drew = true;
    line_has_glyph = true;
    if (!g_ready) {
      if (cp < 0x80) {
        epd.drawChar(cx, cy + 4, (char)cp, fg, bg);
        cx += 6;
      } else {
        epd.drawRect(cx, cy, 12, 12, fg);
        cx += 12;
      }
      continue;
    }

    if (cp < 0x80) {
      drawAsc(epd, cx, cy, (char)cp, fg, bg);
      cx += 8;
    } else if (cp == 0x2103) {
      drawCelsius(epd, cx, cy, fg, bg);
      cx += 16;
    } else {
      const uint16_t gb = unicodeToGb(cp);
      drawHzk(epd, cx, cy, gb, fg, bg);
      cx += 16;
    }
  }
  if (!drew) return y;
  strikeLine();
  return cy + kLineH;
}

int fontDrawTextWrappedDither(EpdSsd2683 &epd, int x, int y, int max_w, int max_y, const char *utf8,
                              EpdColor a, EpdColor b, EpdColor bg, int wrap_x) {
  if (!utf8) return y;
  const int origin_x = (wrap_x >= 0) ? wrap_x : x;
  const int right = origin_x + max_w;
  int cx = x;
  int cy = y;
  bool drew = false;
  const char *p = utf8;

  auto newline = [&]() {
    cx = origin_x;
    cy += kLineH;
  };

  int glyphs = 0;
  while (*p && cy + kLineH <= max_y) {
    if ((++glyphs & 15) == 0) wdtPet();
    if (*p == '\r') {
      p++;
      continue;
    }
    if (*p == '\n') {
      p++;
      newline();
      continue;
    }

    uint32_t cp = 0;
    utf8Next(p, cp);
    const int w = glyphWidth(cp);
    if (cx + w > right && cx > origin_x) {
      newline();
      if (cy + kLineH > max_y) break;
    }

    drew = true;
    if (!g_ready) {
      if (cp < 0x80) {
        epd.drawChar(cx, cy + 4, (char)cp, a, bg);
        cx += 6;
      } else {
        epd.drawRect(cx, cy, 12, 12, a);
        cx += 12;
      }
      continue;
    }

    if (cp < 0x80) {
      drawAscDither(epd, cx, cy, (char)cp, a, b, bg, 1);
      cx += 8;
    } else if (cp == 0x2103) {
      drawCelsiusDither(epd, cx, cy, a, b, bg, 1);
      cx += 16;
    } else {
      const uint16_t gb = unicodeToGb(cp);
      drawHzkDither(epd, cx, cy, gb, a, b, bg, 1);
      cx += 16;
    }
  }
  if (!drew) return y;
  return cy + kLineH;
}

int fontDrawTextWrappedSmall(EpdSsd2683 &epd, int x, int y, int max_w, int max_y, const char *utf8,
                             EpdColor fg, EpdColor bg, int wrap_x) {
  if (!utf8) return y;
  const int origin_x = (wrap_x >= 0) ? wrap_x : x;
  const int right = origin_x + max_w;
  int cx = x;
  int cy = y;
  bool drew = false;
  const char *p = utf8;

  auto newline = [&]() {
    cx = origin_x;
    cy += kLineHSmall;
  };

  int glyphs = 0;
  while (*p && cy + kLineHSmall <= max_y) {
    if ((++glyphs & 15) == 0) wdtPet();
    if (*p == '\r') {
      p++;
      continue;
    }
    if (*p == '\n') {
      p++;
      newline();
      continue;
    }

    uint32_t cp = 0;
    utf8Next(p, cp);
    const int w = glyphWidthSmall(cp);
    if (cx + w > right && cx > origin_x) {
      newline();
      if (cy + kLineHSmall > max_y) break;
    }

    drew = true;
    if (!g_ready) {
      if (cp < 0x80) {
        epd.drawChar(cx, cy, (char)cp, fg, bg);
        cx += 6;
      } else {
        epd.drawRect(cx, cy, 8, 8, fg);
        cx += 8;
      }
      continue;
    }

    if (cp < 0x80) {
      drawAscSmall(epd, cx, cy, (char)cp, fg, bg);
      cx += 4;
    } else {
      const uint16_t gb = unicodeToGb(cp);
      drawHzkSmall(epd, cx, cy, gb, fg, bg);
      cx += 8;
    }
  }
  if (!drew) return y;
  return cy + kLineHSmall;
}

int fontDrawTextWrappedDouble(EpdSsd2683 &epd, int x, int y, int max_w, int max_y, const char *utf8,
                              EpdColor fg, EpdColor bg, int wrap_x) {
  if (!utf8) return y;
  constexpr int kScale = 2;
  const int line_h = kLineH * kScale;
  const int origin_x = (wrap_x >= 0) ? wrap_x : x;
  const int right = origin_x + max_w;
  int cx = x;
  int cy = y;
  bool drew = false;
  const char *p = utf8;

  auto newline = [&]() {
    cx = origin_x;
    cy += line_h;
  };

  int glyphs = 0;
  while (*p && cy + line_h <= max_y) {
    if ((++glyphs & 15) == 0) wdtPet();
    if (*p == '\r') {
      p++;
      continue;
    }
    if (*p == '\n') {
      p++;
      newline();
      continue;
    }

    uint32_t cp = 0;
    utf8Next(p, cp);
    const int w = glyphWidth(cp) * kScale;
    if (cx + w > right && cx > origin_x) {
      newline();
      if (cy + line_h > max_y) break;
    }

    drew = true;
    if (!g_ready) {
      if (cp < 0x80) {
        epd.drawChar(cx, cy + 8, (char)cp, fg, bg);
        cx += 12;
      } else {
        epd.drawRect(cx, cy, 24, 24, fg);
        cx += 24;
      }
      continue;
    }

    if (cp < 0x80) {
      drawAsc(epd, cx, cy, (char)cp, fg, bg, kScale);
      cx += 8 * kScale;
    } else if (cp == 0x2103) {
      drawCelsius(epd, cx, cy, fg, bg, kScale);
      cx += 16 * kScale;
    } else {
      const uint16_t gb = unicodeToGb(cp);
      drawHzk(epd, cx, cy, gb, fg, bg, kScale);
      cx += 16 * kScale;
    }
  }
  if (!drew) return y;
  return cy + line_h;
}

int fontDrawTextWrappedDoubleDither(EpdSsd2683 &epd, int x, int y, int max_w, int max_y,
                                    const char *utf8, EpdColor a, EpdColor b, EpdColor bg,
                                    int wrap_x) {
  if (!utf8) return y;
  constexpr int kScale = 2;
  const int line_h = kLineH * kScale;
  const int origin_x = (wrap_x >= 0) ? wrap_x : x;
  const int right = origin_x + max_w;
  int cx = x;
  int cy = y;
  bool drew = false;
  const char *p = utf8;

  auto newline = [&]() {
    cx = origin_x;
    cy += line_h;
  };

  int glyphs = 0;
  while (*p && cy + line_h <= max_y) {
    if ((++glyphs & 15) == 0) wdtPet();
    if (*p == '\r') {
      p++;
      continue;
    }
    if (*p == '\n') {
      p++;
      newline();
      continue;
    }

    uint32_t cp = 0;
    utf8Next(p, cp);
    const int w = glyphWidth(cp) * kScale;
    if (cx + w > right && cx > origin_x) {
      newline();
      if (cy + line_h > max_y) break;
    }

    drew = true;
    if (!g_ready) {
      if (cp < 0x80) {
        epd.drawChar(cx, cy + 8, (char)cp, a, bg);
        cx += 12;
      } else {
        epd.drawRect(cx, cy, 24, 24, a);
        cx += 24;
      }
      continue;
    }

    if (cp < 0x80) {
      drawAscDither(epd, cx, cy, (char)cp, a, b, bg, kScale);
      cx += 8 * kScale;
    } else if (cp == 0x2103) {
      drawCelsiusDither(epd, cx, cy, a, b, bg, kScale);
      cx += 16 * kScale;
    } else {
      const uint16_t gb = unicodeToGb(cp);
      drawHzkDither(epd, cx, cy, gb, a, b, bg, kScale);
      cx += 16 * kScale;
    }
  }
  if (!drew) return y;
  return cy + line_h;
}
