#pragma once

#include <Arduino.h>
#include "epd_ssd2683.h"
#include "app_apps.h"

static constexpr int ALBUM_FRAME_BYTES = EpdSsd2683::kBufferLen;  // 30000
static constexpr const char *ALBUM_DIR = "/album";
static constexpr const char *ALBUM_VIS_PATH = "/album/vis.json";

// 动态列表；数量不设硬上限，受 LittleFS 空间限制。析构自动释放。
struct AlbumList {
  int count = 0;
  int capacity = 0;
  char (*names)[24] = nullptr;
  bool *shown = nullptr;

  AlbumList() = default;
  ~AlbumList() { clear(); }
  AlbumList(const AlbumList &) = delete;
  AlbumList &operator=(const AlbumList &) = delete;
  AlbumList(AlbumList &&o) noexcept { *this = static_cast<AlbumList &&>(o); }
  AlbumList &operator=(AlbumList &&o) noexcept {
    if (this == &o) return *this;
    clear();
    count = o.count;
    capacity = o.capacity;
    names = o.names;
    shown = o.shown;
    o.count = o.capacity = 0;
    o.names = nullptr;
    o.shown = nullptr;
    return *this;
  }

  void clear();
  bool reserve(int need);
  bool push(const char *name, bool is_shown);
};

bool albumEnsureDir();
bool albumList(AlbumList &out);           // 全部文件（含隐藏）；按修改时间降序
bool albumListVisible(AlbumList &out);    // 仅展示中的
bool albumDelete(const char *name);
// 删除全部 .2bp；返回成功删除张数（-1 = 列表失败）
int albumDeleteAll();
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
