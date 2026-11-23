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

// label 占 4 字宽、右对齐；CJK=16px、ASCII=8px，缺的用半角空格补齐
static void formatLabel(char *out, size_t n, const char *k) {
  constexpr int kField = 4;
  const int len = utf8Len(k);
  int missing = kField - len;
  if (missing < 0) missing = 0;
  // 每个缺字补 2 个半角空格（对齐到汉字宽）
  char pad[16];
  const int pad_n = missing * 2;
  for (int i = 0; i < pad_n && i < (int)sizeof(pad) - 1; ++i) pad[i] = ' ';
  pad[pad_n < (int)sizeof(pad) ? pad_n : (int)sizeof(pad) - 1] = '\0';
  snprintf(out, n, "%s%s", pad, k ? k : "");
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
    if (!cfg.weather_refresh_en) {
      y = drawKv(epd, y, "天气定时", "关");
    } else {
      uint32_t d = 0, h = 0, m = 0;
      weatherRefreshToDhm(cfg.weather_refresh_sec, d, h, m);
      snprintf(val, sizeof(val), "开 %u 天 %u 时 %u 分", (unsigned)d, (unsigned)h, (unsigned)m);
      y = drawKv(epd, y, "天气定时", val, Theme::Alert);
    }
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
  y = drawKv(epd, y, "日历刷新", "跨天自动(停在日历时)");

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
