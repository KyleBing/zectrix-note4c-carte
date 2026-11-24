#pragma once

#include "epd_ssd2683.h"

#include <Arduino.h>

static constexpr int QUOTE_ITEM_MAX = 64;
static constexpr int QUOTE_TEXT_MAX = 240;
static constexpr int QUOTE_AUTHOR_MAX = 48;
static constexpr int QUOTE_SEED_VER = 2;  // 内置列表版本；升级时合并新诗
static constexpr const char *QUOTE_PATH = "/quote/list.json";

struct QuoteItem {
  String text;
  String author;
};

// 名言列表存 LittleFS；index 为当前展示下标。每条含内容 + 作者。
// 正文含换行时按诗词居中排版。
bool quoteLoad(QuoteItem *out_items, int max_n, int &out_n, int &out_index, int *out_seed_ver = nullptr);
bool quoteSave(const QuoteItem *items, int n, int index, int seed_ver = QUOTE_SEED_VER);
int quoteCount();
bool quoteShow(EpdSsd2683 &epd, int &index);
void quoteAdvanceIndex(int &index, int steps);
// 列表为空或种子版本落后时写入 / 合并内置名言与诗词
void quoteEnsureDefaults();
// Web：恢复为当前内置列表（覆盖已有条目）
bool quoteResetToDefaults();

bool quoteAutoRefreshDue();
