#include "app_calendar.h"
#include "app_config.h"
#include "app_font.h"
#include "app_header.h"
#include "app_lunar.h"
#include "app_rtc.h"
#include "app_theme.h"
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

// Width estimate: small = ASCII 4 / CJK 8; full = ASCII 8 / CJK 16.
static int textWidthUtf8(const char *utf8, bool small) {
  if (!utf8) return 0;
  const int aw = small ? 4 : 8;
  const int cw = small ? 8 : 16;
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

static int utf8CharCount(const char *utf8) {
  if (!utf8) return 0;
  int n = 0;
  const uint8_t *p = (const uint8_t *)utf8;
  while (*p) {
    n++;
    if (*p < 0x80)
      p++;
    else if (*p < 0xE0)
      p += 2;
    else if (*p < 0xF0)
      p += 3;
    else
      p += 4;
  }
  return n;
}

// 小字竖排：每字一行，返回占用高度。
static int drawTextVerticalSmall(EpdSsd2683 &epd, int x, int y, int max_y, const char *utf8,
                                 EpdColor fg, EpdColor bg) {
  if (!utf8 || !utf8[0]) return 0;
  const int lh = fontLineHeightSmall();
  const int cw = 8;
  int cy = y;
  const uint8_t *p = (const uint8_t *)utf8;
  while (*p && cy + lh <= max_y) {
    char ch[5] = {0};
    int nbytes = 1;
    if (*p >= 0xF0)
      nbytes = 4;
    else if (*p >= 0xE0)
      nbytes = 3;
    else if (*p >= 0xC0)
      nbytes = 2;
    for (int i = 0; i < nbytes && p[i]; i++) ch[i] = (char)p[i];
    p += nbytes;
    fontDrawTextWrappedSmall(epd, x, cy, cw + 2, cy + lh, ch, fg, bg);
    cy += lh;
  }
  return cy - y;
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

  const bool monday_first = getAppConfig().week_start_monday;

  // 无头部：顶边留小间距，格子吃满屏高
  constexpr int grid_top = 4;
  constexpr int grid_left = Theme::MarginX;
  constexpr int grid_w = 400 - Theme::MarginX * 2;
  constexpr int cols = 7;
  constexpr int cell_w = grid_w / cols;
  constexpr int wd_h = 18;
  constexpr int gap_above_line = 3;
  constexpr int gap_below_line = 3;
  constexpr int rows = 6;
  constexpr int kLunGap = 3;
  constexpr int kLunCw = 8;  // 小字 CJK 宽

  const int line_y = grid_top + wd_h + gap_above_line;
  const int days_top = line_y + 1 + gap_below_line;

  // 2× 公历数字 + 右侧竖排小字农历
  const int day_h = fontLineHeightDouble();
  const int lun_h = fontLineHeightSmall();
  const int grid_bottom = EpdSsd2683::kHeight - 2;
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

    // 格子内农历：仅农历月初显示月份名（正月/二月…）
    LunarDate lun;
    char lun_lab[12] = "";
    if (lunarFromSolar(now.year, now.month, d, lun) && lun.ok && lun.day == 1) {
      lunarDayLabel(lun, lun_lab, sizeof(lun_lab), true);
    }

    char num[4];
    snprintf(num, sizeof(num), "%d", d);
    const int num_w = textWidthUtf8(num, false) * 2;  // 2×
    const bool has_lun = lun_lab[0] != '\0';
    const int lun_total_h = has_lun ? utf8CharCount(lun_lab) * lun_h : 0;
    // 数字 + 竖排农历贴一起（间距 3px），整体在格内居中
    const int block_w = has_lun ? (num_w + kLunGap + kLunCw) : num_w;
    const int block_h = has_lun ? (day_h > lun_total_h ? day_h : lun_total_h) : day_h;
    const int block_x = x0 + (cell_w - block_w) / 2;
    const int block_y = y0 + (cell_h - block_h) / 2;
    const int day_x = block_x;
    const int day_y = block_y + (block_h - day_h) / 2;
    const int lun_x = block_x + num_w + kLunGap;
    const int lun_y = block_y + (block_h - lun_total_h) / 2;

    const EpdColor bg = today ? Theme::HeaderBg : Theme::Bg;
    const EpdColor day_fg = today ? Theme::HeaderFg : (weekend ? Theme::Alert : Theme::Fg);
    const EpdColor lun_fg = today ? Theme::HeaderFg : Theme::Alert;

    if (today) {
      epd.fillRect(x0 + 1, y0 + 1, cell_w - 2, cell_h - 2, Theme::HeaderBg);
    }

    fontDrawTextWrappedDouble(epd, day_x, day_y, num_w + 2, day_y + day_h, num, day_fg, bg);
    if (has_lun) {
      drawTextVerticalSmall(epd, lun_x, lun_y, y0 + cell_h - 1, lun_lab, lun_fg, bg);
    }
  }

  // 今日农历（贴在年月旁）
  char today_lun_lab[24] = "";
  {
    LunarDate today_lun;
    if (lunarFromSolar(now.year, now.month, now.day, today_lun) && today_lun.ok) {
      static const char *kLunMon[] = {"正", "二", "三", "四", "五", "六",
                                     "七", "八", "九", "十", "冬", "腊"};
      char daylab[12] = "";
      lunarDayLabel(today_lun, daylab, sizeof(daylab), false);
      if (today_lun.month >= 1 && today_lun.month <= 12) {
        snprintf(today_lun_lab, sizeof(today_lun_lab), "%s%s月%s", today_lun.leap ? "闰" : "",
                 kLunMon[today_lun.month - 1], daylab);
      }
    }
  }

  // 年月右下角：字体「2026年11月」+ 旁附今日农历
  {
    char lab[20];
    snprintf(lab, sizeof(lab), "%d年%d月", now.year, now.month);

    constexpr int kYmLunGap = 6;
    constexpr int kPad = 2;

    const int th = fontLineHeight();
    const int tw = textWidthUtf8(lab, false);
    const int lun_w = today_lun_lab[0] ? textWidthUtf8(today_lun_lab, false) : 0;
    const int lun_extra = lun_w ? (kYmLunGap + lun_w) : 0;
    const int total_w = tw + lun_extra;
    const int block_h = th;

    const int tx = grid_left + grid_w - total_w - kPad;
    const int ty = days_top + rows * cell_h - block_h - kPad;

    int cx = tx;
    if (lun_w) {
      fontDrawTextWrapped(epd, cx, ty, lun_w + 2, ty + th, today_lun_lab, Theme::Alert, Theme::Bg);
      cx += lun_w + kYmLunGap;
    }

    fontDrawTextWrapped(epd, cx, ty, tw + 4, ty + th, lab, Theme::Fg, Theme::Bg);
  }

  epd.display();
  return true;
}
