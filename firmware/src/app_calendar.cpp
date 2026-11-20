#include "app_calendar.h"
#include "app_config.h"
#include "app_font.h"
#include "app_header.h"
#include "app_lunar.h"
#include "app_rtc.h"
#include "app_theme.h"
#include "app_footer.h"
#include "app_version.h"
#include "app_wifi.h"
#include "app_errorlog.h"

#include <cstdio>
#include <cstring>

static int daysInMonth(int y, int m) {
  static const int k[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (m < 1 || m > 12) return 0;
  if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0))) return 29;
  return k[m];
}

// Sakamoto: 0=Sun .. 6=Sat
static int weekdayFromYmd(int y, int m, int d) {
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

static int colFromWeekday(int wd_sun0, bool monday_first) {
  if (monday_first) return (wd_sun0 + 6) % 7;
  return wd_sun0;
}

// Half-size glyph width estimate (ASCII 4px / CJK 8px).
static int textWidthSmall(const char *utf8) {
  if (!utf8) return 0;
  int w = 0;
  const uint8_t *p = (const uint8_t *)utf8;
  while (*p) {
    if (*p < 0x80) {
      w += 4;
      p++;
    } else if (*p < 0xE0) {
      w += 8;
      p += 2;
    } else if (*p < 0xF0) {
      w += 8;
      p += 3;
    } else {
      w += 8;
      p += 4;
    }
  }
  return w;
}

bool calendarShow(EpdSsd2683 &epd) {
  if (!epd.ok()) {
    errorLog("calendar", "EPD not ready");
    return false;
  }

  if (wifiIsStaConnected()) {
    rtcSyncIfDue(6000);
  } else if (!rtcIsValid() && hasSavedWifiConfig()) {
    wifiConnectSaved(12000);
  }

  RtcDateTime now;
  if (!rtcRead(now)) {
    const char *msg = "无法读取时间。\n请联网同步 NTP，或检查 RTC 芯片。";
    errorLog("calendar", msg);
    epd.clear(Theme::Bg);
    headerDrawAppBar(epd, AppId::Calendar, "CALENDAR");
    int y = fontDrawTextWrapped(epd, Theme::MarginX, 40, 384, Theme::FooterY - 4, "错误详情",
                                Theme::Alert);
    y += 4;
    fontDrawTextWrapped(epd, Theme::MarginX, y, 384, Theme::FooterY - 4, msg, Theme::Fg);
    epd.display();
    return true;
  }

  epd.clear(Theme::Bg);
  headerDrawAppBar(epd, AppId::Calendar, nullptr);
  char ym[24];
  snprintf(ym, sizeof(ym), "%d年%d月", now.year, now.month);
  fontDrawTextWrapped(epd, Theme::MarginX, 3, 200, Theme::HeaderH, ym, Theme::HeaderFg,
                      Theme::HeaderBg);

  const bool monday_first = getAppConfig().week_start_monday;

  constexpr int grid_top = 28;
  constexpr int grid_left = Theme::MarginX;
  constexpr int grid_w = 400 - Theme::MarginX * 2;
  constexpr int cols = 7;
  constexpr int cell_w = grid_w / cols;
  constexpr int wd_h = 18;
  constexpr int gap_above_line = 3;
  constexpr int gap_below_line = 3;
  constexpr int rows = 6;
  constexpr int day_lun_gap = 1;

  const int line_y = grid_top + wd_h + gap_above_line;
  const int days_top = line_y + 1 + gap_below_line;

  const int day_h = fontLineHeight();
  const int lun_h = fontLineHeightSmall();
  const int block_h = day_h + day_lun_gap + lun_h;
  const int min_cell_h = block_h + 2;

  // Prefer footer; drop it if cells would be too short.
  const int avail_with_footer = Theme::FooterY - 2 - days_top;
  const bool show_footer = (avail_with_footer / rows) >= min_cell_h;
  const int grid_bottom = show_footer ? (Theme::FooterY - 2) : (EpdSsd2683::kHeight - 2);
  const int cell_h = (grid_bottom - days_top) / rows;

  static const char *kWdSun[] = {"日", "一", "二", "三", "四", "五", "六"};
  static const char *kWdMon[] = {"一", "二", "三", "四", "五", "六", "日"};
  const char **kWd = monday_first ? kWdMon : kWdSun;

  for (int c = 0; c < cols; c++) {
    const int cx = grid_left + c * cell_w + cell_w / 2;
    const bool weekend = monday_first ? (c >= 5) : (c == 0 || c == 6);
    fontDrawTextWrapped(epd, cx - 8, grid_top, 16, grid_top + wd_h, kWd[c],
                        weekend ? Theme::Alert : Theme::Fg);
  }
  epd.drawHLine(grid_left, line_y, grid_w, Theme::Fg);

  const int dim = daysInMonth(now.year, now.month);
  const int first_col = colFromWeekday(weekdayFromYmd(now.year, now.month, 1), monday_first);

  for (int d = 1; d <= dim; d++) {
    const int idx = first_col + (d - 1);
    const int r = idx / cols;
    const int c = idx % cols;
    if (r >= rows) break;

    const int x0 = grid_left + c * cell_w;
    const int y0 = days_top + r * cell_h;
    const bool today = (d == now.day);
    const bool weekend = monday_first ? (c >= 5) : (c == 0 || c == 6);

    LunarDate lun;
    char lun_lab[12] = "";
    if (lunarFromSolar(now.year, now.month, d, lun)) {
      lunarDayLabel(lun, lun_lab, sizeof(lun_lab), true);
    }

    char num[4];
    snprintf(num, sizeof(num), "%d", d);
    const int num_w = (int)strlen(num) * 8;
    const int lun_w = lun_lab[0] ? textWidthSmall(lun_lab) : 0;
    const int use_h = lun_lab[0] ? block_h : day_h;
    const int block_y = y0 + (cell_h - use_h) / 2;
    const int day_x = x0 + (cell_w - num_w) / 2;
    const int lun_x = lun_w > 0 ? x0 + (cell_w - lun_w) / 2 : x0;

    const EpdColor bg = today ? Theme::Alert : Theme::Bg;
    const EpdColor day_fg = today ? Theme::HeaderFg : (weekend ? Theme::Alert : Theme::Fg);
    // Red lunar; today cell is red fill → keep white for contrast
    const EpdColor lun_fg = today ? Theme::HeaderFg : Theme::Alert;

    if (today) {
      epd.fillRect(x0 + 1, y0 + 1, cell_w - 2, cell_h - 2, Theme::Alert);
    }

    fontDrawTextWrapped(epd, day_x, block_y, num_w + 2, block_y + day_h, num, day_fg, bg);
    if (lun_lab[0]) {
      const int lun_y = block_y + day_h + day_lun_gap;
      fontDrawTextWrappedSmall(epd, lun_x, lun_y, lun_w + 2, lun_y + lun_h, lun_lab, lun_fg, bg);
    }
  }

  if (show_footer) {
    footerDraw(epd, "refresh");
  }
  epd.display();
  return true;
}
