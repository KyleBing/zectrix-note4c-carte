#pragma once

#include <Arduino.h>
#include "epd_ssd2683.h"
#include "app_apps.h"

static constexpr int ALBUM_MAX = 30;
static constexpr int ALBUM_FRAME_BYTES = EpdSsd2683::kBufferLen;  // 30000
static constexpr const char *ALBUM_DIR = "/album";
static constexpr const char *ALBUM_VIS_PATH = "/album/vis.json";

struct AlbumList {
  int count = 0;
  char names[ALBUM_MAX][24];  // e.g. 001.2bp
  bool shown[ALBUM_MAX];      // default true; false = 文件保留但不在设备翻页
};

bool albumEnsureDir();
bool albumList(AlbumList &out);           // 全部文件（含隐藏）
bool albumListVisible(AlbumList &out);    // 仅展示中的
bool albumDelete(const char *name);
bool albumNameOk(const char *name);
bool albumIsShown(const char *name);      // 缺省 true
bool albumSetShown(const char *name, bool show);
bool albumSetShownAll(bool show);
// Allocate next free name like 001.2bp into out_name.
bool albumNextFileName(char *out_name, size_t out_len);
// Load .2bp into framebuffer; overlay header if configured, then full refresh.
// index 针对可见列表。
bool albumShow(EpdSsd2683 &epd, int index);
int albumCountCached();  // 可见张数
