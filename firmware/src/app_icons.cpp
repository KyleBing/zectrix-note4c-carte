#include "app_icons.h"

// Pure isosceles triangle in ICON_SIZE box (base = height = ICON_SIZE).
void iconDrawArrowDown(EpdSsd2683 &epd, int x, int cy, EpdColor color) {
  const int half = ICON_SIZE / 2;
  const int cx = x + half;
  const int top = cy - half;
  const int bot = cy + half;
  epd.fillTriangle(cx, bot, cx - half, top, cx + half, top, color);
}

void iconDrawArrowUp(EpdSsd2683 &epd, int x, int cy, EpdColor color) {
  const int half = ICON_SIZE / 2;
  const int cx = x + half;
  const int top = cy - half;
  const int bot = cy + half;
  epd.fillTriangle(cx, top, cx - half, bot, cx + half, bot, color);
}

void iconDrawBootCircle(EpdSsd2683 &epd, int x, int cy, EpdColor color) {
  const int cx = x + ICON_SIZE / 2;
  epd.drawCircle(cx, cy, ICON_BOOT_R, color);
}
