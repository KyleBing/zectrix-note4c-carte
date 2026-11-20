#pragma once

#include <cstdio>
#include <cstring>

#include "app_apps.h"
#include "app_battery.h"
#include "app_theme.h"
#include "epd_ssd2683.h"

// Draw black header bar + title, then a single-row app indicator left of battery.
// Idle: 3×7 white · Selected: 3×11 white · gap 2px · vertically centered.
// title 仅适合 ASCII（6x8）；中文请在外部用 fontDrawTextWrapped 画。
// Returns leftmost x of right chrome (app indicator, else battery). Keep status/title left of this.
inline int headerDrawAppBar(EpdSsd2683 &epd, AppId id, const char *title) {
  epd.fillRect(0, 0, 400, Theme::HeaderH, Theme::HeaderBg);
  if (title && title[0]) {
    epd.drawText(Theme::MarginX, 7, title, Theme::HeaderFg, Theme::HeaderBg);
  }
  const int bat_left = batteryDrawHeader(epd);

  constexpr int kDotW = 3;
  constexpr int kDotHIdle = 7;
  constexpr int kDotHSel = 11;
  constexpr int kGap = 2;
  const int n = static_cast<int>(AppId::Count);
  int enabled_n = 0;
  int sel_idx = -1;
  for (int i = 0; i < n; i++) {
    if (!appIdEnabled(static_cast<AppId>(i))) continue;
    if (i == static_cast<int>(id)) sel_idx = enabled_n;
    enabled_n++;
  }
  // 只有一个可显示 App 时不画 indicator（没有切换意义）
  if (enabled_n <= 1) return bat_left;

  const int row_w = enabled_n * kDotW + (enabled_n - 1) * kGap;
  const int row_x = bat_left - 8 - row_w;
  const int mid_y = Theme::HeaderH / 2;
  if (row_x <= Theme::MarginX + 40) return bat_left;

  int slot = 0;
  for (int i = 0; i < n; i++) {
    if (!appIdEnabled(static_cast<AppId>(i))) continue;
    const int cx = row_x + slot * (kDotW + kGap);
    const bool sel = (slot == sel_idx);
    const int h = sel ? kDotHSel : kDotHIdle;
    epd.fillRect(cx, mid_y - h / 2, kDotW, h, Theme::HeaderFg);
    slot++;
  }
  return row_x;  // left edge of indicator — do not draw status over this
}
