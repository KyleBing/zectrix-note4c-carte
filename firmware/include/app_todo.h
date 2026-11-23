#pragma once

#include "epd_ssd2683.h"
#include "app_diary.h"

static constexpr int TODO_ITEM_MAX = 40;

struct TodoItem {
  bool done = false;
  String content;
  String note;
};

// Match diary-vue: lines starting with "- " / "= "; optional " #note".
// Returns count; unfinished first, then finished.
int todoParseContent(const String &content, TodoItem *out, int max_n);

// Draw diary content as a todo / note page on the e-paper.
// refreshed_at：本次拉取完成时间；为空则不画刷新时间。
void todoDrawPage(EpdSsd2683 &epd, const DiaryDetail &diary, const char *status_line = nullptr,
                  const char *refreshed_at = nullptr);

// Connect (if needed), fetch configured diary id, draw. Returns false on soft failure.
bool todoRefreshAndDraw(EpdSsd2683 &epd);
