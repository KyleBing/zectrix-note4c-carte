#include "app_album.h"
#include "app_config.h"
#include "app_header.h"
#include "app_theme.h"
#include "app_footer.h"
#include "app_version.h"
#include "app_font.h"

#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <time.h>

enum class AlbumPendKind : uint8_t { None, Name, Preview };
static AlbumPendKind g_pend = AlbumPendKind::None;
static char g_pend_name[24] = "";
static bool g_cast_busy = false;

bool albumEnsureDir() {
  if (LittleFS.exists(ALBUM_DIR)) return true;
  return LittleFS.mkdir(ALBUM_DIR);
}

void AlbumList::clear() {
  free(names);
  free(shown);
  names = nullptr;
  shown = nullptr;
  count = 0;
  capacity = 0;
}

bool AlbumList::reserve(int need) {
  if (need <= capacity) return true;
  int ncap = capacity ? capacity : 16;
  while (ncap < need) {
    if (ncap > 10000) return false;
    ncap *= 2;
  }
  auto *nn = static_cast<char(*)[24]>(realloc(names, (size_t)ncap * 24));
  if (!nn) return false;
  names = nn;
  bool *ns = static_cast<bool *>(realloc(shown, (size_t)ncap * sizeof(bool)));
  if (!ns) return false;
  shown = ns;
  capacity = ncap;
  return true;
}

bool AlbumList::push(const char *name, bool is_shown) {
  if (!name || !name[0]) return false;
  if (!reserve(count + 1)) return false;
  strncpy(names[count], name, 23);
  names[count][23] = '\0';
  shown[count] = is_shown;
  count++;
  return true;
}

static bool loadVisDoc(JsonDocument &doc) {
  File f = LittleFS.open(ALBUM_VIS_PATH, "r");
  if (!f) return false;
  const DeserializationError err = deserializeJson(doc, f);
  f.close();
  return !err;
}

static bool nameInHiddenArr(JsonArray arr, const char *name) {
  if (!arr) return false;
  for (JsonVariant v : arr) {
    const char *h = v.as<const char *>();
    if (h && strcmp(h, name) == 0) return true;
  }
  return false;
}

bool albumIsShown(const char *name) {
  if (!albumNameOk(name)) return false;
  JsonDocument doc;
  if (!loadVisDoc(doc)) return true;
  if (!doc["hidden"].is<JsonArray>()) return true;
  return !nameInHiddenArr(doc["hidden"].as<JsonArray>(), name);
}

bool albumSetShown(const char *name, bool show) {
  if (!albumNameOk(name)) return false;
  JsonDocument doc;
  loadVisDoc(doc);
  JsonArray arr = doc["hidden"].is<JsonArray>() ? doc["hidden"].as<JsonArray>() : doc["hidden"].to<JsonArray>();

  const bool was = nameInHiddenArr(arr, name);
  if (show) {
    if (!was) return true;
    JsonDocument next;
    JsonArray out = next["hidden"].to<JsonArray>();
    for (JsonVariant v : arr) {
      const char *h = v.as<const char *>();
      if (!h || strcmp(h, name) == 0) continue;
      out.add(h);
    }
    albumEnsureDir();
    File f = LittleFS.open(ALBUM_VIS_PATH, "w");
    if (!f) return false;
    const bool ok = serializeJson(next, f) > 0;
    f.close();
    return ok;
  }

  if (was) return true;
  arr.add(name);
  albumEnsureDir();
  File f = LittleFS.open(ALBUM_VIS_PATH, "w");
  if (!f) return false;
  const bool ok = serializeJson(doc, f) > 0;
  f.close();
  return ok;
}

bool albumSetShownAll(bool show) {
  AlbumList list;
  if (!albumList(list)) return false;
  if (!albumEnsureDir()) return false;
  File f = LittleFS.open(ALBUM_VIS_PATH, "w");
  if (!f) return false;
  JsonDocument doc;
  JsonArray arr = doc["hidden"].to<JsonArray>();
  if (!show) {
    for (int i = 0; i < list.count; i++) arr.add(list.names[i]);
  }
  const bool ok = serializeJson(doc, f) > 0;
  f.close();
  return ok;
}

bool albumList(AlbumList &out) {
  out.clear();
  if (!albumEnsureDir()) return false;

  JsonDocument vis;
  loadVisDoc(vis);
  JsonArray hidden;
  if (vis["hidden"].is<JsonArray>()) hidden = vis["hidden"].as<JsonArray>();

  time_t *mtimes = nullptr;
  int mcap = 0;

  File dir = LittleFS.open(ALBUM_DIR);
  if (!dir || !dir.isDirectory()) return false;
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    if (f.isDirectory()) {
      f.close();
      continue;
    }
    const char *name = f.name();
    const char *base = strrchr(name, '/');
    base = base ? base + 1 : name;
    const size_t nlen = strlen(base);
    if (nlen < 5 || nlen >= 24) {
      f.close();
      continue;
    }
    if (strcmp(base + nlen - 4, ".2bp") != 0) {
      f.close();
      continue;
    }
    if (f.size() != (size_t)ALBUM_FRAME_BYTES) {
      Serial.printf("[Album] skip %s size=%u\n", base, (unsigned)f.size());
      f.close();
      continue;
    }
    if (!out.reserve(out.count + 1)) {
      f.close();
      break;
    }
    if (out.count >= mcap) {
      int nc = mcap ? mcap * 2 : 16;
      auto *nm = static_cast<time_t *>(realloc(mtimes, (size_t)nc * sizeof(time_t)));
      if (!nm) {
        f.close();
        break;
      }
      mtimes = nm;
      mcap = nc;
    }
    strncpy(out.names[out.count], base, 23);
    out.names[out.count][23] = '\0';
    out.shown[out.count] = !nameInHiddenArr(hidden, base);
    mtimes[out.count] = f.getLastWrite();
    out.count++;
    f.close();
  }
  dir.close();

  if (out.count > 1 && mtimes) {
    // 最新修改在前；mtime 相同再按文件名降序
    for (int i = 0; i < out.count - 1; i++) {
      for (int j = i + 1; j < out.count; j++) {
        const bool newer = mtimes[j] > mtimes[i] ||
                           (mtimes[j] == mtimes[i] && strcmp(out.names[i], out.names[j]) < 0);
        if (!newer) continue;
        char tmp[24];
        memcpy(tmp, out.names[i], 24);
        memcpy(out.names[i], out.names[j], 24);
        memcpy(out.names[j], tmp, 24);
        const bool sb = out.shown[i];
        out.shown[i] = out.shown[j];
        out.shown[j] = sb;
        const time_t mt = mtimes[i];
        mtimes[i] = mtimes[j];
        mtimes[j] = mt;
      }
    }
  }
  free(mtimes);
  return true;
}

bool albumListVisible(AlbumList &out) {
  AlbumList all;
  if (!albumList(all)) {
    out.clear();
    return false;
  }
  out.clear();
  for (int i = 0; i < all.count; i++) {
    if (!all.shown[i]) continue;
    if (!out.push(all.names[i], true)) {
      out.clear();
      return false;
    }
  }
  return true;
}

int albumCountCached() {
  AlbumList list;
  if (!albumListVisible(list)) return 0;
  return list.count;
}

bool albumNextFileName(char *out_name, size_t out_len) {
  if (!out_name || out_len < 8) return false;
  AlbumList list;
  albumList(list);

  int max_n = 0;
  for (int i = 0; i < list.count; i++) {
    int n = 0;
    if (sscanf(list.names[i], "%d", &n) == 1 && n > max_n) max_n = n;
  }
  // 无数量上限；下一编号。空间不足时在写入阶段失败。
  if (max_n < 0 || max_n >= 999999) return false;
  snprintf(out_name, out_len, "%03d.2bp", max_n + 1);
  return true;
}

bool albumNameOk(const char *name) {
  if (!name || !name[0]) return false;
  const size_t n = strlen(name);
  if (n < 5 || n >= 24) return false;
  if (strchr(name, '/') || strchr(name, '\\') || strstr(name, "..")) return false;
  if (strcmp(name + n - 4, ".2bp") != 0) return false;
  for (size_t i = 0; i < n; i++) {
    const char c = name[i];
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                    c == '.' || c == '_' || c == '-';
    if (!ok) return false;
  }
  return true;
}

bool albumDelete(const char *name) {
  if (!albumNameOk(name)) return false;
  char path[40];
  snprintf(path, sizeof(path), "%s/%s", ALBUM_DIR, name);
  if (!LittleFS.exists(path)) return false;
  const bool ok = LittleFS.remove(path);
  if (ok) albumSetShown(name, true);  // drop from hidden list
  return ok;
}

int albumDeleteAll() {
  AlbumList list;
  if (!albumList(list)) return -1;
  int ok = 0;
  for (int i = 0; i < list.count; i++) {
    if (albumDelete(list.names[i])) ok++;
  }
  // 清空隐藏表（文件已无）
  albumEnsureDir();
  File f = LittleFS.open(ALBUM_VIS_PATH, "w");
  if (f) {
    f.print("{\"hidden\":[]}");
    f.close();
  }
  return ok;
}

bool albumShowPath(EpdSsd2683 &epd, const char *path, const char *meta, bool allow_interrupt,
                   bool overlay_header) {
  if (!epd.ok() || !path || !path[0]) return false;

  File f = LittleFS.open(path, "r");
  if (!f || f.size() != (size_t)ALBUM_FRAME_BYTES) {
    if (f) f.close();
    epd.clear(Theme::Bg);
    headerDrawAppBar(epd, AppId::Album, "ALBUM");
    fontDrawTextWrapped(epd, Theme::MarginX, 40, 384, Theme::FooterY - 4, "Bad photo file",
                        Theme::Alert);
    epd.display(allow_interrupt);
    return false;
  }

  uint8_t *fb = epd.framebuffer();
  if (!fb) {
    f.close();
    return false;
  }
  const size_t n = f.read(fb, ALBUM_FRAME_BYTES);
  f.close();
  if (n != (size_t)ALBUM_FRAME_BYTES) {
    Serial.printf("[Album] short read %u\n", (unsigned)n);
    return false;
  }

  const bool do_header = overlay_header && getAppConfig().album_show_header;
  if (do_header) {
    headerDrawAppBar(epd, AppId::Album, "ALBUM");
    if (meta && meta[0]) {
      epd.drawText(72, 7, meta, Theme::HeaderAccent, Theme::HeaderBg);
    }
  }

  Serial.printf("[Album] show path=%s meta=%s header=%d\n", path, meta ? meta : "-",
                (int)do_header);
  const bool ok = epd.display(allow_interrupt);
  if (ok && overlay_header) {
    // 仅相册 App 翻页才记 last；Web 投屏不碰定时翻页状态
    const time_t now = time(nullptr);
    if (now >= 1700000000) saveAlbumLastUnix((uint32_t)now);
  }
  return ok;
}

bool albumShow(EpdSsd2683 &epd, int index) {
  if (!epd.ok()) return false;
  AlbumList list;
  albumListVisible(list);

  if (list.count <= 0) {
    epd.clear(Theme::Bg);
    headerDrawAppBar(epd, AppId::Album, "ALBUM");
    fontDrawTextWrapped(epd, Theme::MarginX, 40, 384, Theme::FooterY - 4,
                        "No photos yet.\nOpen Web config -> Album\n"
                        "Edit + dither on phone,\nupload .2bp to device.",
                        Theme::Fg);
    footerDraw(epd, "next", "nextApp");
    epd.display();
    return true;
  }

  if (index < 0) index = 0;
  if (index >= list.count) index = list.count - 1;

  char path[40];
  snprintf(path, sizeof(path), "%s/%s", ALBUM_DIR, list.names[index]);
  char meta[24];
  snprintf(meta, sizeof(meta), "%d/%d", index + 1, list.count);
  Serial.printf("[Album] show %s (%d/%d)\n", list.names[index], index + 1, list.count);
  return albumShowPath(epd, path, meta, /*allow_interrupt=*/true, /*overlay_header=*/true);
}

bool albumRequestDisplayName(const char *name) {
  if (!albumNameOk(name)) return false;
  char path[40];
  snprintf(path, sizeof(path), "%s/%s", ALBUM_DIR, name);
  if (!LittleFS.exists(path)) return false;
  strncpy(g_pend_name, name, sizeof(g_pend_name) - 1);
  g_pend_name[sizeof(g_pend_name) - 1] = '\0';
  g_pend = AlbumPendKind::Name;
  Serial.printf("[Album] queue cast name=%s\n", g_pend_name);
  return true;
}

bool albumRequestDisplayPreview() {
  File f = LittleFS.open(ALBUM_PREVIEW_PATH, "r");
  const size_t sz = f ? f.size() : 0;
  if (f) f.close();
  if (sz != (size_t)ALBUM_FRAME_BYTES) return false;
  g_pend_name[0] = '\0';
  g_pend = AlbumPendKind::Preview;
  Serial.println("[Album] queue cast preview");
  return true;
}

bool albumDisplayPending() { return g_pend != AlbumPendKind::None; }

bool albumCastBusy() { return g_cast_busy; }

void albumSetCastBusy(bool busy) { g_cast_busy = busy; }

bool albumTakeDisplay(EpdSsd2683 &epd, bool *out_ok) {
  if (out_ok) *out_ok = false;
  if (g_pend == AlbumPendKind::None) return false;

  const AlbumPendKind kind = g_pend;
  char name[24];
  strncpy(name, g_pend_name, sizeof(name) - 1);
  name[sizeof(name) - 1] = '\0';
  g_pend = AlbumPendKind::None;
  g_pend_name[0] = '\0';

  bool ok = false;
  if (kind == AlbumPendKind::Name) {
    char path[40];
    snprintf(path, sizeof(path), "%s/%s", ALBUM_DIR, name);
    // Web 配置页投屏：原样刷帧，不切 App、不叠相册标题栏
    ok = albumShowPath(epd, path, nullptr, /*allow_interrupt=*/false, /*overlay_header=*/false);
  } else if (kind == AlbumPendKind::Preview) {
    ok = albumShowPath(epd, ALBUM_PREVIEW_PATH, nullptr, /*allow_interrupt=*/false,
                       /*overlay_header=*/false);
  }
  if (out_ok) *out_ok = ok;
  return true;
}
