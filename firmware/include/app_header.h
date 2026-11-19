#pragma once

#include <cstdio>
#include <cstring>

#include "app_apps.h"
#include "app_battery.h"
#include "app_theme.h"
#include "epd_ssd2683.h"

// Draw black header bar + title, then "n/N" slot just left of battery.
// Returns battery left edge (for optional status text).
inline int headerDrawAppBar(EpdSsd2683 &epd, AppId id, const char *title) {
  epd.fillRect(0, 0, 400, Theme::HeaderH, Theme::HeaderBg);
  if (title && title[0]) {
    epd.drawText(Theme::MarginX, 7, title, Theme::HeaderFg, Theme::HeaderBg);
  }
  const int bat_left = batteryDrawHeader(epd);
  char slot[8];
  snprintf(slot, sizeof(slot), "%d/%d", static_cast<int>(id) + 1, static_cast<int>(AppId::Count));
  const int slot_w = (int)strlen(slot) * 6;
  const int slot_x = bat_left - 8 - slot_w;
  if (slot_x > Theme::MarginX + 40) {
    epd.drawText(slot_x, 7, slot, Theme::HeaderAccent, Theme::HeaderBg);
  }
  return bat_left;
}
