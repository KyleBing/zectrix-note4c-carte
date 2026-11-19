#include "app_font.h"

#include <FS.h>
#include <LittleFS.h>
#include <cstring>

static uint8_t *g_hzk = nullptr;
static uint8_t *g_asc = nullptr;
static uint8_t *g_uni2gb = nullptr;
static size_t g_uni2gb_count = 0;
static bool g_ready = false;

static constexpr int kLineH = 16;

bool fontReady() { return g_ready; }
int fontLineHeight() { return kLineH; }

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

static void drawAsc(EpdSsd2683 &epd, int x, int y, char c, EpdColor fg, EpdColor bg) {
  if (!g_asc || (uint8_t)c > 127) c = '?';
  const uint8_t *glyph = g_asc + (uint8_t)c * 16;
  for (int row = 0; row < 16; row++) {
    uint8_t bits = glyph[row];
    for (int col = 0; col < 8; col++) {
      const bool on = (bits & (0x80 >> col)) != 0;
      epd.setPixel(x + col, y + row, on ? fg : bg);
    }
  }
}

static void drawHzk(EpdSsd2683 &epd, int x, int y, uint16_t gb, EpdColor fg, EpdColor bg) {
  if (!g_hzk || gb < 0xA1A1) {
    epd.drawRect(x, y, 16, 16, fg);
    return;
  }
  const uint8_t hi = (gb >> 8) & 0xFF;
  const uint8_t lo = gb & 0xFF;
  if (hi < 0xA1 || lo < 0xA1) {
    epd.drawRect(x, y, 16, 16, fg);
    return;
  }
  const uint32_t offset = (uint32_t)(94 * (hi - 0xA1) + (lo - 0xA1)) * 32;
  const uint8_t *m = g_hzk + offset;
  for (int row = 0; row < 16; row++) {
    for (int byteCol = 0; byteCol < 2; byteCol++) {
      uint8_t bits = m[row * 2 + byteCol];
      for (int bit = 0; bit < 8; bit++) {
        const bool on = (bits & (0x80 >> bit)) != 0;
        epd.setPixel(x + byteCol * 8 + bit, y + row, on ? fg : bg);
      }
    }
  }
}

static int glyphWidth(uint32_t cp) {
  if (cp == '\n' || cp == '\r') return 0;
  if (cp < 0x80) return 8;
  return 16;
}

int fontDrawTextWrapped(EpdSsd2683 &epd, int x, int y, int max_w, int max_y, const char *utf8,
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
    cy += kLineH;
  };

  while (*p && cy + kLineH <= max_y) {
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
    } else {
      const uint16_t gb = unicodeToGb(cp);
      drawHzk(epd, cx, cy, gb, fg, bg);
      cx += 16;
    }
  }
  if (!drew) return y;
  return cy + kLineH;
}
