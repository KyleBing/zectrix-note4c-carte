#include "app_status.h"

#include "app_album.h"
#include "app_apps.h"
#include "app_battery.h"
#include "app_config.h"
#include "app_errorlog.h"
#include "app_font.h"
#include "app_footer.h"
#include "app_header.h"
#include "app_rtc.h"
#include "app_theme.h"
#include "app_version.h"
#include "app_wifi.h"

#include <FS.h>
#include <LittleFS.h>
#include <cstdio>
#include <esp_system.h>

// 统计 UTF-8 字符数（用于 label 对齐；ASCII/CJK 各计 1）
static int utf8Len(const char *s) {
  int n = 0;
  if (!s) return 0;
  while (*s) {
    const unsigned char c = (unsigned char)*s++;
    if ((c & 0xC0) != 0x80) ++n;
  }
  return n;
}

// 拷贝下一个 UTF-8 字符到 dst，返回字节数；失败返回 0
static int copyUtf8Char(char *dst, size_t dst_n, const char *s) {
  if (!dst || dst_n < 2 || !s || !s[0]) return 0;
  const unsigned char c = (unsigned char)s[0];
  int nbytes = 1;
  if ((c & 0x80) == 0) nbytes = 1;
  else if ((c & 0xE0) == 0xC0) nbytes = 2;
  else if ((c & 0xF0) == 0xE0) nbytes = 3;
  else if ((c & 0xF8) == 0xF0) nbytes = 4;
  else return 0;
  if ((size_t)nbytes + 1 > dst_n) return 0;
  for (int i = 0; i < nbytes; ++i) {
    if (!s[i]) return 0;
    dst[i] = s[i];
  }
  dst[nbytes] = '\0';
  return nbytes;
}

// label 占 4 字宽：二字中间插两个全角空格（固　　件）；其它仍右对齐半角空格补齐
static void formatLabel(char *out, size_t n, const char *k) {
  constexpr int kField = 4;
  if (!k) k = "";
  const int len = utf8Len(k);
  if (len == 2) {
    char a[8], b[8];
    const int na = copyUtf8Char(a, sizeof(a), k);
    const int nb = na ? copyUtf8Char(b, sizeof(b), k + na) : 0;
    if (na && nb) {
      // U+3000 IDEOGRAPHIC SPACE ×2
      snprintf(out, n, "%s\xE3\x80\x80\xE3\x80\x80%s", a, b);
      return;
    }
  }
  int missing = kField - len;
  if (missing < 0) missing = 0;
  // 每个缺字补 2 个半角空格（对齐到汉字宽）
  char pad[16];
  const int pad_n = missing * 2;
  for (int i = 0; i < pad_n && i < (int)sizeof(pad) - 1; ++i) pad[i] = ' ';
  pad[pad_n < (int)sizeof(pad) ? pad_n : (int)sizeof(pad) - 1] = '\0';
  snprintf(out, n, "%s%s", pad, k);
}

// 字库为 GB2312：避免中间点等非常用符号，改用 ASCII 分隔
static int drawKv(EpdSsd2683 &epd, int y, const char *k, const char *v, EpdColor vfg = Theme::Fg) {
  constexpr int kMaxY = Theme::FooterY - 4;
  if (y + fontLineHeight() > kMaxY) return y;
  char label[24];
  formatLabel(label, sizeof(label), k);
  char line[96];
  snprintf(line, sizeof(line), "%s  %s", label, v);
  return fontDrawTextWrapped(epd, Theme::MarginX, y, 384, kMaxY, line, vfg) + 2;
}

static void fmtDuration(char *buf, size_t n, int32_t sec) {
  if (sec < 0) {
    snprintf(buf, n, "未知");
    return;
  }
  if (sec < 60) {
    snprintf(buf, n, "%d 秒", (int)sec);
    return;
  }
  const int32_t m = sec / 60;
  if (m < 60) {
    snprintf(buf, n, "%d 分 %d 秒", (int)m, (int)(sec % 60));
    return;
  }
  const int32_t h = m / 60;
  if (h < 48) {
    snprintf(buf, n, "%d 小时 %d 分", (int)h, (int)(m % 60));
    return;
  }
  snprintf(buf, n, "%d 天 %d 小时", (int)(h / 24), (int)(h % 24));
}

bool statusShow(EpdSsd2683 &epd) {
  if (!epd.ok()) {
    errorLog("status", "EPD not ready");
    return false;
  }

  batteryPoll();

  const AppConfig &cfg = getAppConfig();
  epd.clear(Theme::Bg);
  headerDrawAppBar(epd, AppId::Status, "STATUS");

  int y = Theme::HeaderH + 6;
  char val[72];

  // 固件名 + 版本号（编译时间用 ASCII，字库可显示）
  snprintf(val, sizeof(val), "Carte %s (%d)", APP_VERSION, APP_BUILD);
  y = drawKv(epd, y, "固件", val);
  y = drawKv(epd, y, "编译", APP_UPDATE_TIME);

  RtcDateTime now;
  if (rtcRead(now)) {
    snprintf(val, sizeof(val), "%04d-%02d-%02d %02d:%02d:%02d", now.year, now.month, now.day,
             now.hour, now.minute, now.second);
    y = drawKv(epd, y, "时间", val);
  } else {
    y = drawKv(epd, y, "时间", "未设置", Theme::Alert);
  }

  {
    const uint32_t total = ESP.getHeapSize();
    const uint32_t free_h = ESP.getFreeHeap();
    const uint32_t used = total > free_h ? total - free_h : 0;
    snprintf(val, sizeof(val), "已用 %u/%u KB 空闲 %u KB", (unsigned)(used / 1024),
             (unsigned)(total / 1024), (unsigned)(free_h / 1024));
    y = drawKv(epd, y, "内存", val);
  }

  {
    const uint32_t total = ESP.getPsramSize();
    if (total > 0) {
      const uint32_t free_p = ESP.getFreePsram();
      const uint32_t used = total > free_p ? total - free_p : 0;
      snprintf(val, sizeof(val), "已用 %u/%u KB 空闲 %u KB", (unsigned)(used / 1024),
               (unsigned)(total / 1024), (unsigned)(free_p / 1024));
      y = drawKv(epd, y, "扩展内存", val);
    } else {
      y = drawKv(epd, y, "扩展内存", "无");
    }
  }

  {
    const uint64_t total = LittleFS.totalBytes();
    const uint64_t used = LittleFS.usedBytes();
    AlbumList al;
    albumList(al);
    snprintf(val, sizeof(val), "%u/%u KB 相册 %d 张", (unsigned)(used / 1024),
             (unsigned)(total / 1024), al.count);
    y = drawKv(epd, y, "存储", val);
  }

  {
    const BatteryStatus bat = batteryRead();
    if (bat.ok) {
      snprintf(val, sizeof(val), "%u%% %u mV %s", (unsigned)bat.percent, (unsigned)bat.mv,
               batteryChargeLabel(bat.charge));
      y = drawKv(epd, y, "电池", val);
    } else {
      y = drawKv(epd, y, "电池", "不可读", Theme::Alert);
    }
  }

  // 断电时长
  {
    const int32_t unplug = batteryUnpluggedSec();
    if (unplug == 0) {
      y = drawKv(epd, y, "电源", "已接电");
    } else {
      char dur[40];
      fmtDuration(dur, sizeof(dur), unplug);
      snprintf(val, sizeof(val), "已断开 %s", dur);
      y = drawKv(epd, y, "电源", val, Theme::Alert);
    }
  }

  if (wifiIsStaConnected()) {
    const String ssid = wifiCurrentSsid();
    const String ip = wifiStaIp();
    snprintf(val, sizeof(val), "%s %s %ddBm", ssid.c_str(), ip.c_str(), (int)wifiStaRssi());
    y = drawKv(epd, y, "无线", val);
  } else {
    y = drawKv(epd, y, "无线", "未连接");
  }

  y += 4;
  epd.drawHLine(Theme::MarginX, y, 384, Theme::Fg);
  y += 6;

  {
    auto drawWxTimer = [&](const char *lab, bool en, uint32_t sec) {
      if (!en) {
        y = drawKv(epd, y, lab, "关");
        return;
      }
      uint32_t d = 0, h = 0, m = 0;
      weatherRefreshToDhm(sec, d, h, m);
      snprintf(val, sizeof(val), "开 %u 天 %u 时 %u 分", (unsigned)d, (unsigned)h, (unsigned)m);
      y = drawKv(epd, y, lab, val, Theme::Alert);
    };
    drawWxTimer("日天气", cfg.weather_now_refresh_en, cfg.weather_now_refresh_sec);
    drawWxTimer("周天气", cfg.weather_week_refresh_en, cfg.weather_week_refresh_sec);
  }
  {
    if (!cfg.album_refresh_en) {
      y = drawKv(epd, y, "相册定时", "关");
    } else {
      uint32_t d = 0, h = 0, m = 0;
      weatherRefreshToDhm(cfg.album_refresh_sec, d, h, m);
      snprintf(val, sizeof(val), "开 %u 天 %u 时 %u 分", (unsigned)d, (unsigned)h, (unsigned)m);
      y = drawKv(epd, y, "相册定时", val, Theme::Alert);
    }
  }
  {
    if (!cfg.chess_refresh_en) {
      y = drawKv(epd, y, "象棋定时", "关");
    } else {
      uint32_t d = 0, h = 0, m = 0;
      weatherRefreshToDhm(cfg.chess_refresh_sec, d, h, m);
      snprintf(val, sizeof(val), "开 %u 天 %u 时 %u 分", (unsigned)d, (unsigned)h, (unsigned)m);
      y = drawKv(epd, y, "象棋定时", val, Theme::Alert);
    }
    y = drawKv(epd, y, "象棋顺序", cfg.chess_random ? "乱序" : "顺序");
    y = drawKv(epd, y, "相册顺序", cfg.album_random ? "乱序" : "顺序");
  }
  {
    if (!cfg.quote_refresh_en) {
      y = drawKv(epd, y, "名言定时", "关");
    } else {
      uint32_t d = 0, h = 0, m = 0;
      weatherRefreshToDhm(cfg.quote_refresh_sec, d, h, m);
      snprintf(val, sizeof(val), "开 %u 天 %u 时 %u 分", (unsigned)d, (unsigned)h, (unsigned)m);
      y = drawKv(epd, y, "名言定时", val, Theme::Alert);
    }
  }
  y = drawKv(epd, y, "日历刷新", "跨天自动(停在日历时)");
  if (cfg.quiet_en && cfg.quiet_count > 0) {
    snprintf(val, sizeof(val), "开 %u 段", (unsigned)cfg.quiet_count);
    y = drawKv(epd, y, "静默时段", val, Theme::Alert);
  } else {
    y = drawKv(epd, y, "静默时段", "关");
  }
  if (cfg.app_sched_en && cfg.app_sched_count > 0) {
    snprintf(val, sizeof(val), "开 %u 点", (unsigned)cfg.app_sched_count);
    y = drawKv(epd, y, "App时间表", val, Theme::Alert);
  } else {
    y = drawKv(epd, y, "App时间表", "关");
  }

  if (cfg.idle_off_sec == 0) {
    y = drawKv(epd, y, "空闲关机", "关");
  } else {
    snprintf(val, sizeof(val), "%u 秒%s", (unsigned)cfg.idle_off_sec,
             cfg.idle_off_on_power ? " 接电也关" : " 接电不关");
    y = drawKv(epd, y, "空闲关机", val);
  }

  {
    const int nerr = errorLogCount();
    snprintf(val, sizeof(val), "%d 条", nerr);
    y = drawKv(epd, y, "错误日志", val, nerr > 0 ? Theme::Alert : Theme::Fg);
  }

  footerDraw(epd, "refresh");
  epd.display();
  return true;
}
