#pragma once

#include <Arduino.h>

// 固件错误事件环：LittleFS `/logs/errors.json`，Web /errors 可查看。
static constexpr int ERROR_LOG_MAX = 40;
static constexpr size_t ERROR_LOG_MSG_MAX = 180;
static constexpr size_t ERROR_LOG_SRC_MAX = 16;

struct ErrorLogEntry {
  uint32_t unix_t = 0;     // 0 = 时间未知
  uint32_t uptime_ms = 0;
  char src[ERROR_LOG_SRC_MAX] = {};
  char msg[ERROR_LOG_MSG_MAX] = {};
};

// LittleFS 已挂载后调用（fontBegin 之后）。
bool errorLogBegin();

void errorLog(const char *src, const char *msg);
void errorLog(const char *src, const String &msg);

int errorLogCount();
bool errorLogGet(int newest_index, ErrorLogEntry &out);  // 0 = 最新
bool errorLogClear();

// 追加 HTML 列表到 String（供 Web 页）。
void errorLogAppendHtml(String &out);
