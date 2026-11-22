#pragma once

#include <Arduino.h>
#include "epd_ssd2683.h"
#include "app_apps.h"

static constexpr int ALBUM_FRAME_BYTES = EpdSsd2683::kBufferLen;  // 30000
static constexpr const char *ALBUM_DIR = "/album";
static constexpr const char *ALBUM_VIS_PATH = "/album/vis.json";
// Web 投屏：未入库的当前编辑帧（不进相册列表）
static constexpr const char *ALBUM_PREVIEW_PATH = "/album_preview.2bp";
static constexpr const char *ALBUM_PREVIEW_TMP = "/album_preview.tmp";

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
// 从任意路径加载 .2bp 并刷屏。overlay_header=false 时原样刷帧（Web 投屏用，不叠相册标题栏）。
bool albumShowPath(EpdSsd2683 &epd, const char *path, const char *meta = nullptr,
                   bool allow_interrupt = true, bool overlay_header = true);
int albumCountCached();  // 可见张数

// Web → main：挂起投屏请求（HTTP 立即返回，loop 里刷屏；不切换当前 App）
bool albumRequestDisplayName(const char *name);
bool albumRequestDisplayPreview();
bool albumDisplayPending();
bool albumCastBusy();  // 墨水屏正在刷投屏帧
void albumSetCastBusy(bool busy);
// 消费挂起请求并刷屏。无请求返回 false；有请求则 *out_ok 有意义。
bool albumTakeDisplay(EpdSsd2683 &epd, bool *out_ok);
