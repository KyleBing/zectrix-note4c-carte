#include "app_splash.h"
#include "app_config.h"
#include "app_font.h"
#include "app_theme.h"
#include "app_version.h"

#include <qrcode.h>

#include <cstdio>
#include <cstring>

static constexpr const char *kDocsUrl = "https://kylebing.github.io/zectrix-note4c-carte/";
static constexpr const char *kGithub = "github.com/KyleBing/zectrix-note4c-carte";

// 4×4 Bayer，用于四色抖动渐变
static uint8_t bayer4(int x, int y) {
  static const uint8_t k[4][4] = {
      {0, 8, 2, 10},
      {12, 4, 14, 6},
      {3, 11, 1, 9},
      {15, 7, 13, 5},
  };
  return k[y & 3][x & 3];
}

// 左上黑 → 红 → 黄 → 右下白（沿对角 + Bayer 混色）
static EpdColor gradColor(int lx, int ly, int w, int h) {
  // t: 0 左上 … 1 右下
  const int t256 = ((lx * 128) / (w > 1 ? w - 1 : 1) + (ly * 128) / (h > 1 ? h - 1 : 1));
  // 三段：黑→红、红→黄、黄→白，每段约 85
  EpdColor a = Theme::Fg, b = Theme::Alert;
  int local = t256;
  if (t256 < 85) {
    a = Theme::Fg;
    b = Theme::Alert;
    local = t256;
  } else if (t256 < 170) {
    a = Theme::Alert;
    b = Theme::Accent;
    local = t256 - 85;
  } else {
    a = Theme::Accent;
    b = Theme::Bg;
    local = t256 - 170;
  }
  // local 0..84 → 混合比例；Bayer 0..15
  const int thresh = (local * 16) / 85;
  return (bayer4(lx, ly) < thresh) ? b : a;
}

// 外框 + 对角四色渐变面板 + 右下圆圈
static void drawLogo(EpdSsd2683 &epd, int cx, int cy, int size) {
  const int x = cx - size / 2;
  const int y = cy - size / 2;
  epd.fillRect(x, y, size, size, Theme::Bg);
  for (int i = 0; i < 2; i++) {  // 边框减 1px（原 3 → 2）
    epd.drawRect(x + i, y + i, size - 2 * i, size - 2 * i, Theme::Fg);
  }

  const int inset = size * 10 / 100;
  const int ix = x + inset;
  const int iy = y + inset;
  const int iw = size - inset * 2;
  const int ih = (iw * 670) / 880;
  const int iy2 = iy + (iw - ih) / 8 - 1;
  for (int py = 0; py < ih; py++) {
    for (int px = 0; px < iw; px++) {
      epd.setPixel(ix + px, iy2 + py, gradColor(px, py, iw, ih));
    }
  }
  epd.drawRect(ix, iy2, iw, ih, Theme::Fg);

  const int r = size * 7 / 100;
  const int ox = x + size - inset - r - 2;
  const int oy = y + size - inset - r - 2 + 2;
  epd.drawCircle(ox, oy, r, Theme::Fg);
}

static int textW(const char *utf8, bool dbl) {
  if (!utf8) return 0;
  const int aw = dbl ? 16 : 8;
  const int cw = dbl ? 32 : 16;
  int w = 0;
  const uint8_t *p = (const uint8_t *)utf8;
  while (*p) {
    if (*p < 0x80) {
      w += aw;
      p++;
    } else if (*p < 0xE0) {
      w += cw;
      p += 2;
    } else if (*p < 0xF0) {
      w += cw;
      p += 3;
    } else {
      w += cw;
      p += 4;
    }
  }
  return w;
}

static void drawTextAt(EpdSsd2683 &epd, int x, int y, const char *s, EpdColor fg, EpdColor bg,
                       bool dbl) {
  const int w = textW(s, dbl);
  const int lh = dbl ? fontLineHeightDouble() : fontLineHeight();
  if (dbl) {
    fontDrawTextWrappedDouble(epd, x, y, w + 4, y + lh, s, fg, bg);
  } else {
    fontDrawTextWrapped(epd, x, y, w + 4, y + lh, s, fg, bg);
  }
}

static void drawCentered(EpdSsd2683 &epd, int y, const char *s, EpdColor fg, bool dbl) {
  const int w = textW(s, dbl);
  drawTextAt(epd, (EpdSsd2683::kWidth - w) / 2, y, s, fg, Theme::Bg, dbl);
}

static void drawCenteredIn(EpdSsd2683 &epd, int left, int width, int y, const char *s, EpdColor fg,
                           bool dbl) {
  const int w = textW(s, dbl);
  drawTextAt(epd, left + (width - w) / 2, y, s, fg, Theme::Bg, dbl);
}

// 外框与 logo 同为 size×size
static void drawDocsQrBoxed(EpdSsd2683 &epd, int x, int y, int size) {
  constexpr int kVer = 3;
  constexpr int kQuiet = 2;
  constexpr int kBorder = 2;
  QRCode qr;
  uint8_t buf[qrcode_getBufferSize(kVer)];
  if (qrcode_initText(&qr, buf, kVer, ECC_LOW, kDocsUrl) != 0) return;

  const int mods = qr.size + kQuiet * 2;  // 33
  const int avail = size - 2 * kBorder;
  const int scale = avail / mods;
  if (scale < 1) return;
  const int qr_px = mods * scale;
  const int qx = x + (size - qr_px) / 2;
  const int qy = y + (size - qr_px) / 2;

  epd.fillRect(x, y, size, size, Theme::Bg);
  for (int i = 0; i < kBorder; i++) {
    epd.drawRect(x + i, y + i, size - 2 * i, size - 2 * i, Theme::Fg);
  }
  for (int my = 0; my < qr.size; my++) {
    for (int mx = 0; mx < qr.size; mx++) {
      if (!qrcode_getModule(&qr, mx, my)) continue;
      epd.fillRect(qx + (mx + kQuiet) * scale, qy + (my + kQuiet) * scale, scale, scale, Theme::Fg);
    }
  }
}

void splashDraw(EpdSsd2683 &epd) {
  if (!epd.ok()) return;
  epd.clear(Theme::Bg);

  constexpr int kW = EpdSsd2683::kWidth;
  constexpr int kH = EpdSsd2683::kHeight;
  constexpr int kBox = 104;  // logo 与缩小后的二维码同大
  constexpr int kColW = kW / 2;  // 左右各半，各自居中
  const int lh = fontLineHeight();
  const int dlh = fontLineHeightDouble();

  // 顶：标题（整行居中）；除底两行外整体再下移 5px
  int y = 17;
  drawCentered(epd, y, "Carte", Theme::Fg, true);
  y += dlh + 4;
  char ver[40];
  snprintf(ver, sizeof(ver), "%s (%d)", APP_VERSION, APP_BUILD);
  drawCentered(epd, y, ver, Theme::Alert, false);
  y += lh + 12 + 5;  // LOGO/二维码整体下移 5px

  // 中：左右分栏，LOGO / 二维码同尺寸，各向中间靠 5px
  const int box_y = y;
  constexpr int kInward = 5;
  drawLogo(epd, kColW / 2 + kInward, box_y + kBox / 2, kBox);
  drawDocsQrBoxed(epd, kColW + (kColW - kBox) / 2 - kInward, box_y, kBox);

  // 说明书距二维码 10px，随二维码内移
  const int cap_y = box_y + kBox + 10;
  drawCenteredIn(epd, kColW - kInward, kColW, cap_y, "说明书", Theme::Fg, false);

  // 底两行：距底边 20px（上移 10px），行距 8px，整行居中；提示用红字
  constexpr int kBotPad = 20;
  constexpr int kLineGap = 8;
  const int line2_y = kH - kBotPad - lh;
  const int line1_y = line2_y - kLineGap - lh;
  drawCentered(epd, line1_y, kGithub, Theme::Fg, false);
  drawCentered(epd, line2_y, "刷机完成, 按任意键继续", Theme::Alert, false);

  epd.display();
}

bool splashShowIfNeeded(EpdSsd2683 &epd) {
  const int seen = loadSplashBuild();
  if (seen == APP_BUILD) return false;
  splashDraw(epd);
  saveSplashBuild(APP_BUILD);
  return true;
}
