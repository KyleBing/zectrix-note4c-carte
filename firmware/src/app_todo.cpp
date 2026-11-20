#include "app_todo.h"
#include "app_config.h"
#include "app_font.h"
#include "app_wifi.h"
#include "app_battery.h"
#include "app_header.h"
#include "app_footer.h"
#include "app_version.h"
#include "app_errorlog.h"

#include <cstdio>
#include <cstring>

int todoParseContent(const String &content, TodoItem *out, int max_n) {
  if (!out || max_n <= 0) return 0;

  TodoItem undone[TODO_ITEM_MAX];
  TodoItem done[TODO_ITEM_MAX];
  int nu = 0, nd = 0;

  int start = 0;
  const int n = content.length();
  while (start <= n) {
    int end = content.indexOf('\n', start);
    if (end < 0) end = n;
    String line = content.substring(start, end);
    // trim trailing \r
    if (line.endsWith("\r")) line.remove(line.length() - 1);
    start = end + 1;

    if (line.length() < 2) continue;
    const char mark = line.charAt(0);
    if (mark != '-' && mark != '=') continue;
    // diary-vue: prefix is "- " or "= " (char + space); tolerate missing space
    int body_start = 1;
    if (line.length() > 1 && line.charAt(1) == ' ') body_start = 2;
    String rest = line.substring(body_start);
    rest.trim();
    if (rest.isEmpty()) continue;

    TodoItem item;
    item.done = (mark == '=');
    const int hash = rest.indexOf('#');
    if (hash >= 0) {
      item.content = rest.substring(0, hash);
      item.content.trim();
      item.note = rest.substring(hash + 1);
      item.note.trim();
    } else {
      item.content = rest;
    }
    if (item.content.isEmpty() && item.note.isEmpty()) continue;

    if (item.done) {
      if (nd < TODO_ITEM_MAX) done[nd++] = item;
    } else {
      if (nu < TODO_ITEM_MAX) undone[nu++] = item;
    }
  }

  int count = 0;
  for (int i = 0; i < nu && count < max_n; i++) out[count++] = undone[i];
  for (int i = 0; i < nd && count < max_n; i++) out[count++] = done[i];
  return count;
}

static void drawCheckbox(EpdSsd2683 &epd, int x, int y, bool done) {
  constexpr int s = 12;
  epd.drawRect(x, y + 2, s, s, EPD_BLACK);
  if (done) {
    // filled check: inner block + diagonal ticks
    epd.fillRect(x + 2, y + 4, s - 4, s - 4, EPD_BLACK);
    // light "check" notch in white/yellow
    epd.setPixel(x + 3, y + 8, EPD_WHITE);
    epd.setPixel(x + 4, y + 9, EPD_WHITE);
    epd.setPixel(x + 5, y + 10, EPD_WHITE);
    epd.setPixel(x + 6, y + 9, EPD_WHITE);
    epd.setPixel(x + 7, y + 8, EPD_WHITE);
    epd.setPixel(x + 8, y + 7, EPD_WHITE);
    epd.setPixel(x + 9, y + 6, EPD_WHITE);
  }
}

void todoDrawPage(EpdSsd2683 &epd, const DiaryDetail &diary, const char *status_line) {
  epd.clear(EPD_WHITE);

  TodoItem items[TODO_ITEM_MAX];
  const int n = diary.ok ? todoParseContent(diary.content, items, TODO_ITEM_MAX) : 0;
  int undone = 0;
  for (int i = 0; i < n; i++) {
    if (!items[i].done) undone++;
  }

  // Header + app slot n/N (near battery). chrome_left = indicator, keep status left of it.
  const int chrome_left = headerDrawAppBar(epd, AppId::Todo, "TODO");
  char line[48];
  if (n > 0) {
    snprintf(line, sizeof(line), "%d/%d open", undone, n);
    epd.drawText(56, 7, line, EPD_YELLOW, EPD_BLACK);
  }
  if (status_line && status_line[0]) {
    const int sw = (int)strlen(status_line) * 6;
    const int sx = chrome_left - 6 - sw;
    if (sx >= 100) epd.drawText(sx, 7, status_line, EPD_YELLOW, EPD_BLACK);
  }

  // Meta
  int y = 28;
  if (diary.title.length()) {
    y = fontDrawTextWrapped(epd, 8, y, 384, 56, diary.title.c_str(), EPD_RED);
  } else if (diary.date.length()) {
    snprintf(line, sizeof(line), "%s", diary.date.c_str());
    epd.drawText(8, y, line, EPD_BLACK);
    y += 16;
  }
  y += 3;  // 3px below title
  epd.drawHLine(8, y, 384, EPD_BLACK);
  y += 1 + 3;  // line + 3px above content

  constexpr int footer_y = 276;
  constexpr int text_x = 26;
  constexpr int box_x = 8;
  constexpr int text_w = 400 - text_x - 8;

  if (!diary.ok) {
    y = fontDrawTextWrapped(epd, 8, y, 384, footer_y - 4, "错误详情", EPD_RED);
    y += 4;
    fontDrawTextWrapped(epd, 8, y, 384, footer_y - 4,
                        diary.message.length() ? diary.message.c_str() : "(error)", EPD_BLACK);
  } else if (n == 0) {
    if (diary.content.length()) {
      // not todo-formatted; show raw
      fontDrawTextWrapped(epd, 8, y, 384, footer_y - 4, diary.content.c_str(), EPD_BLACK);
    } else {
      fontDrawTextWrapped(epd, 8, y, 384, footer_y - 4, "(no - / = todo lines)", EPD_BLACK);
    }
  } else {
    for (int i = 0; i < n; i++) {
      if (y + fontLineHeight() > footer_y - 4) {
        fontDrawTextWrapped(epd, 8, y, 384, footer_y - 4, "...", EPD_RED);
        break;
      }
      const TodoItem &it = items[i];
      drawCheckbox(epd, box_x, y, it.done);

      if (it.content.length()) {
        y = fontDrawTextWrapped(epd, text_x, y, text_w, footer_y - 4, it.content.c_str(), EPD_BLACK,
                                EPD_WHITE, text_x, it.done, EPD_BLACK);
      }
      // note: 2px below main todo text; red
      if (it.note.length() && y + fontLineHeight() <= footer_y - 4) {
        y += 2;
        y = fontDrawTextWrapped(epd, text_x, y, text_w, footer_y - 4, it.note.c_str(), EPD_RED,
                                EPD_WHITE, text_x);
      }
      y += 4;  // 4px gap between todo items
    }
  }

  footerDraw(epd, "refresh");
  epd.display();
}

bool todoRefreshAndDraw(EpdSsd2683 &epd) {
  if (!epd.ok()) {
    errorLog("todo", "EPD not ready");
    return false;
  }

  DiaryDetail d;
  char status[24] = "fetch...";

  auto failDraw = [&](const char *st) {
    if (d.message.length()) errorLog("todo", d.message);
    todoDrawPage(epd, d, st);
    return true;  // 已刷错误页；勿 ErrorBurst
  };

  if (!wifiIsStaConnected()) {
    if (!hasSavedWifiConfig()) {
      d.message = "无 WiFi 配置。长按上键打开配置网页（热点 Note4C-Carte）。";
      return failDraw("no-wifi");
    }
    strncpy(status, "wifi...", sizeof(status) - 1);
    if (!wifiConnectSaved(18000)) {
      d.message = "WiFi 连接失败。长按上键打开热点配网，检查 SSID/密码。";
      return failDraw("wifi-fail");
    }
  }

  if (!diaryHasToken()) {
    d.message = "未登录 Portal。请在配置网页打开 /auth 登录。";
    return failDraw("no-auth");
  }

  const AppConfig &cfg = getAppConfig();
  if (!cfg.diary_id[0]) {
    d.message = "diary_id 为空。请在 /auth 登录后选择待办日记。";
    return failDraw("no-id");
  }

  Serial.printf("[Todo] fetch diary %s\n", cfg.diary_id);
  if (!diaryFetchById(cfg.diary_id, d)) {
    if (!d.message.length()) d.message = "拉取待办失败";
    else d.message = String("拉取待办失败：") + d.message;
    return failDraw("err");
  }

  snprintf(status, sizeof(status), "OK");
  todoDrawPage(epd, d, status);
  return true;
}
