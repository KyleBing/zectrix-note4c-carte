#include "app_todo.h"
#include "app_todo_store.h"
#include "app_config.h"
#include "app_font.h"
#include "app_wifi.h"
#include "app_theme.h"
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

void todoDrawPage(EpdSsd2683 &epd, const DiaryDetail &diary) {
  epd.clear(Theme::Bg);

  TodoItem items[TODO_ITEM_MAX];
  const int n = diary.ok ? todoParseContent(diary.content, items, TODO_ITEM_MAX) : 0;

  constexpr int kMargin = Theme::MarginX;
  constexpr int kBottom = EpdSsd2683::kHeight - 4;
  constexpr int text_x = 26;
  constexpr int box_x = kMargin;
  constexpr int text_w = EpdSsd2683::kWidth - text_x - kMargin;

  int y = kMargin;
  // 标题（无 header；不画刷新时间）
  if (diary.title.length()) {
    y = fontDrawTextWrapped(epd, kMargin, y, 384, kMargin + 28, diary.title.c_str(), Theme::Alert);
    y += 3;
    epd.drawHLine(kMargin, y, 384, Theme::Fg);
    y += 1 + 3;
  }

  if (!diary.ok) {
    y = fontDrawTextWrapped(epd, kMargin, y, 384, kBottom, "错误详情", Theme::Alert);
    y += 4;
    fontDrawTextWrapped(epd, kMargin, y, 384, kBottom,
                        diary.message.length() ? diary.message.c_str() : "(error)", Theme::Fg);
  } else if (n == 0) {
    if (diary.content.length()) {
      fontDrawTextWrapped(epd, kMargin, y, 384, kBottom, diary.content.c_str(), Theme::Fg);
    } else {
      fontDrawTextWrapped(epd, kMargin, y, 384, kBottom, "(no - / = todo lines)", Theme::Fg);
    }
  } else {
    constexpr int kTodoLineGap = 4;  // 折行行距
    for (int i = 0; i < n; i++) {
      if (y + fontLineHeight() > kBottom) {
        fontDrawTextWrapped(epd, kMargin, y, 384, kBottom, "...", Theme::Alert);
        break;
      }
      const TodoItem &it = items[i];
      drawCheckbox(epd, box_x, y, it.done);

      if (it.content.length()) {
        y = fontDrawTextWrapped(epd, text_x, y, text_w, kBottom, it.content.c_str(), Theme::Fg,
                                Theme::Bg, text_x, it.done, Theme::Fg, kTodoLineGap);
      }
      // note: 2px below main todo text; red
      if (it.note.length() && y + fontLineHeight() <= kBottom) {
        y += 2;
        y = fontDrawTextWrapped(epd, text_x, y, text_w, kBottom, it.note.c_str(), Theme::Alert,
                                Theme::Bg, text_x, false, Theme::Alert, kTodoLineGap);
      }
      y += 4;  // 条目间距
    }
  }

  epd.display();
}

bool todoRefreshAndDraw(EpdSsd2683 &epd) {
  if (!epd.ok()) {
    errorLog("todo", "EPD not ready");
    return false;
  }

  DiaryDetail d;

  auto failDraw = [&]() {
    if (d.message.length()) errorLog("todo", d.message);
    todoDrawPage(epd, d);
    return true;  // 已刷错误页；勿 ErrorBurst
  };

  const AppConfig &cfg = getAppConfig();

  // 本机正文：不联网，直接画
  if (cfg.todo_use_local) {
    String local;
    if (!todoLocalLoad(local) || local.isEmpty()) {
      d.message = "本机待办为空。请在配置网页 /todo 填写正文，或改回网络日记。";
      return failDraw();
    }
    d.ok = true;
    d.title = "本机待办";
    d.content = local;
    d.category = "local";
    d.message = "OK";
    todoDrawPage(epd, d);
    return true;
  }

  if (!wifiIsStaConnected()) {
    if (!hasSavedWifiConfig()) {
      d.message = "无 WiFi 配置。长按上键打开配置网页（热点 Note4C-Carte）。";
      return failDraw();
    }
    if (!wifiConnectSaved(18000)) {
      d.message = "WiFi 连接失败。长按上键打开热点配网，检查 SSID/密码。";
      return failDraw();
    }
  }

  if (!diaryHasToken()) {
    d.message = "未登录 Portal。请在配置网页打开 /auth 登录。";
    return failDraw();
  }

  if (!cfg.diary_id[0]) {
    d.message = "diary_id 为空。请在 /auth 登录后选择待办日记。";
    return failDraw();
  }

  Serial.printf("[Todo] fetch diary %s\n", cfg.diary_id);
  if (!diaryFetchById(cfg.diary_id, d)) {
    if (!d.message.length()) d.message = "拉取待办失败";
    else d.message = String("拉取待办失败：") + d.message;
    return failDraw();
  }

  todoDrawPage(epd, d);
  return true;
}
