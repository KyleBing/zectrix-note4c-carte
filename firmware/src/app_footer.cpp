#include "app_footer.h"

#include "app_icons.h"
#include "app_theme.h"
#include "app_version.h"

#include <cstdio>
#include <cstring>

static constexpr int kGap = 3;

using IconDrawFn = void (*)(EpdSsd2683 &, int, int, EpdColor);

static int drawIconBadge(EpdSsd2683 &epd, int x, int y, IconDrawFn draw) {
  // Square yellow background; glyph centered inside.
  epd.fillRect(x, y, ICON_BADGE_SIZE, ICON_BADGE_SIZE, Theme::Accent);
  const int icon_x = x + (ICON_BADGE_SIZE - ICON_SIZE) / 2;
  const int icon_cy = y + ICON_BADGE_SIZE / 2;
  draw(epd, icon_x, icon_cy, Theme::Fg);
  return ICON_BADGE_SIZE + kGap;
}

static int drawLabel(EpdSsd2683 &epd, int x, int text_y, const char *label) {
  if (!label || !label[0]) return 0;
  epd.drawText(x, text_y, label, Theme::Fg);
  return (int)strlen(label) * 6;
}

void footerDraw(EpdSsd2683 &epd, const char *boot_action, const char *dn_action) {
  if (!boot_action || !boot_action[0]) boot_action = "refresh";
  if (!dn_action || !dn_action[0]) dn_action = "next";

  epd.drawHLine(Theme::MarginX, Theme::FooterY, 384, Theme::Fg);

  // Badge square sits just above 6x8 text baseline
  const int badge_y = 280;
  const int text_y = 282;
  int cx = Theme::MarginX;

  cx += drawIconBadge(epd, cx, badge_y, iconDrawBootCircle);
  cx += drawLabel(epd, cx, text_y, boot_action);
  cx += 6;

  cx += drawIconBadge(epd, cx, badge_y, iconDrawArrowDown);
  cx += drawLabel(epd, cx, text_y, dn_action);
  cx += 6;

  cx += drawIconBadge(epd, cx, badge_y, iconDrawArrowUp);
  cx += drawLabel(epd, cx, text_y, "prev");
  cx += 6;

  epd.drawText(cx, text_y, "hold:web/off", Theme::Fg);

  char ver[24];
  snprintf(ver, sizeof(ver), "%s(%d)", APP_VERSION, APP_BUILD);
  const int ver_w = (int)strlen(ver) * 6;
  epd.drawText(400 - Theme::MarginX - ver_w, text_y, ver, Theme::Fg);
}
