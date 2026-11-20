#include "app_gfx_icon.h"
#include "icon_bitmaps.h"

#include <cstdlib>

WxIcon wxIconFromCode(const char *qweather_icon) {
  const int c = qweather_icon ? atoi(qweather_icon) : 0;
  if (c == 100 || c == 150) return WxIcon::Sunny;
  if (c == 101 || c == 102 || c == 103 || c == 151 || c == 152 || c == 153) return WxIcon::Cloudy;
  if (c == 104 || c == 154) return WxIcon::Overcast;
  if (c == 300 || c == 301 || c == 309 || c == 350 || c == 351) return WxIcon::Sprinkle;
  if (c == 302 || c == 303 || c == 304) return WxIcon::Thunder;
  if (c >= 305 && c <= 399) return WxIcon::Rain;
  if (c >= 400 && c <= 499) return WxIcon::Snow;
  if (c == 500 || c == 501 || c == 509 || c == 510 || c == 514 || c == 515) return WxIcon::Fog;
  if (c == 502 || c == 511 || c == 512 || c == 513) return WxIcon::Smog;
  if (c == 503 || c == 504 || c == 507 || c == 508) return WxIcon::Sandstorm;
  if (c == 900 || c == 901) return WxIcon::Tornado;
  return WxIcon::Cloudy;
}

static uint8_t srcPixel(const uint8_t *bits, int src, int x, int y) {
  if (x < 0 || y < 0 || x >= src || y >= src) return 1;
  const int bpr = (src + 3) / 4;
  const int index = y * bpr + (x >> 2);
  const int shift = 6 - 2 * (x & 3);
  return (pgm_read_byte(&bits[index]) >> shift) & 3;
}

static void blit2bpp(EpdSsd2683 &epd, int dx, int dy, const uint8_t *bits, int src, int dst,
                     bool skip_white) {
  if (!bits || dst <= 0) return;
  for (int y = 0; y < dst; y++) {
    const int sy = y * src / dst;
    for (int x = 0; x < dst; x++) {
      const int sx = x * src / dst;
      const uint8_t c = srcPixel(bits, src, sx, sy);
      if (skip_white && c == EPD_WHITE) continue;
      epd.setPixel(dx + x, dy + y, static_cast<EpdColor>(c));
    }
  }
}

void wxIconDraw(EpdSsd2683 &epd, int x, int y, WxIcon id, int dst) {
  const int i = static_cast<int>(id);
  if (i < 0 || i >= static_cast<int>(WxIcon::Count)) return;
  blit2bpp(epd, x, y, kWxIconBits[i], WX_ICON_SRC, dst, true);
}

void webModeIconDraw(EpdSsd2683 &epd, int x, int y, bool ap_mode, int dst, bool skip_white) {
  blit2bpp(epd, x, y, ap_mode ? kWebAp : kWebLan, WEB_ICON_SRC, dst, skip_white);
}
