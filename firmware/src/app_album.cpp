#include "app_album.h"
#include "app_config.h"
#include "app_header.h"
#include "app_theme.h"
#include "app_version.h"
#include "app_font.h"

#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <cstdio>
#include <cstring>

bool albumEnsureDir() {
  if (LittleFS.exists(ALBUM_DIR)) return true;
  return LittleFS.mkdir(ALBUM_DIR);
}

static int cmpName(const void *a, const void *b) {
  return strcmp(static_cast<const char *>(a), static_cast<const char *>(b));
}

// Load hidden-name set from vis.json: {"hidden":["001.2bp",...]}
static void loadHidden(char hidden[][24], int &n_hidden) {
  n_hidden = 0;
  File f = LittleFS.open(ALBUM_VIS_PATH, "r");
  if (!f) return;
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return;
  if (!doc["hidden"].is<JsonArray>()) return;
  for (JsonVariant v : doc["hidden"].as<JsonArray>()) {
    const char *name = v.as<const char *>();
    if (!name || !name[0] || n_hidden >= ALBUM_MAX) continue;
    strncpy(hidden[n_hidden], name, 23);
    hidden[n_hidden][23] = '\0';
    n_hidden++;
  }
}

static bool nameInHidden(const char hidden[][24], int n_hidden, const char *name) {
  for (int i = 0; i < n_hidden; i++) {
    if (strcmp(hidden[i], name) == 0) return true;
  }
  return false;
}

bool albumIsShown(const char *name) {
  if (!albumNameOk(name)) return false;
  char hidden[ALBUM_MAX][24];
  int n = 0;
  loadHidden(hidden, n);
  return !nameInHidden(hidden, n, name);
}

bool albumSetShown(const char *name, bool show) {
  if (!albumNameOk(name)) return false;
  char hidden[ALBUM_MAX][24];
  int n = 0;
  loadHidden(hidden, n);
  const bool was = nameInHidden(hidden, n, name);
  if (show) {
    if (!was) return true;
    int w = 0;
    for (int i = 0; i < n; i++) {
      if (strcmp(hidden[i], name) == 0) continue;
      if (w != i) memcpy(hidden[w], hidden[i], 24);
      w++;
    }
    n = w;
  } else {
    if (was) return true;
    if (n >= ALBUM_MAX) return false;
    strncpy(hidden[n], name, 23);
    hidden[n][23] = '\0';
    n++;
  }

  albumEnsureDir();
  File f = LittleFS.open(ALBUM_VIS_PATH, "w");
  if (!f) return false;
  JsonDocument doc;
  JsonArray arr = doc["hidden"].to<JsonArray>();
  for (int i = 0; i < n; i++) arr.add(hidden[i]);
  const bool ok = serializeJson(doc, f) > 0;
  f.close();
  return ok;
}

bool albumList(AlbumList &out) {
  out = AlbumList{};
  if (!albumEnsureDir()) return false;

  char hidden[ALBUM_MAX][24];
  int n_hidden = 0;
  loadHidden(hidden, n_hidden);

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
    if (nlen < 5 || nlen >= sizeof(out.names[0])) {
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
    if (out.count >= ALBUM_MAX) {
      f.close();
      break;
    }
    strncpy(out.names[out.count], base, sizeof(out.names[0]) - 1);
    out.names[out.count][sizeof(out.names[0]) - 1] = '\0';
    out.shown[out.count] = !nameInHidden(hidden, n_hidden, base);
    out.count++;
    f.close();
  }
  dir.close();
  if (out.count > 1) {
    // sort by name; keep shown[] aligned via temp
    for (int i = 0; i < out.count - 1; i++) {
      for (int j = i + 1; j < out.count; j++) {
        if (strcmp(out.names[i], out.names[j]) > 0) {
          char tmp[24];
          memcpy(tmp, out.names[i], 24);
          memcpy(out.names[i], out.names[j], 24);
          memcpy(out.names[j], tmp, 24);
          const bool sb = out.shown[i];
          out.shown[i] = out.shown[j];
          out.shown[j] = sb;
        }
      }
    }
  }
  (void)cmpName;
  return true;
}

bool albumListVisible(AlbumList &out) {
  AlbumList all;
  if (!albumList(all)) {
    out = AlbumList{};
    return false;
  }
  out = AlbumList{};
  for (int i = 0; i < all.count; i++) {
    if (!all.shown[i]) continue;
    strncpy(out.names[out.count], all.names[i], sizeof(out.names[0]) - 1);
    out.names[out.count][sizeof(out.names[0]) - 1] = '\0';
    out.shown[out.count] = true;
    out.count++;
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
  if (list.count >= ALBUM_MAX) return false;
  for (int n = 1; n <= ALBUM_MAX; n++) {
    char cand[16];
    snprintf(cand, sizeof(cand), "%03d.2bp", n);
    bool used = false;
    for (int i = 0; i < list.count; i++) {
      if (strcmp(list.names[i], cand) == 0) {
        used = true;
        break;
      }
    }
    if (!used) {
      strncpy(out_name, cand, out_len - 1);
      out_name[out_len - 1] = '\0';
      return true;
    }
  }
  return false;
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
    epd.drawHLine(Theme::MarginX, Theme::FooterY, 384, Theme::Fg);
    epd.drawText(Theme::MarginX, 282, "BOOT:next  DN:nextApp  UP:prev  hold:web/off", Theme::Fg);
    char ver[24];
    snprintf(ver, sizeof(ver), "%s(%d)", APP_VERSION, APP_BUILD);
    epd.drawText(400 - Theme::MarginX - (int)strlen(ver) * 6, 282, ver, Theme::Fg);
    epd.display();
    return true;
  }

  if (index < 0) index = 0;
  if (index >= list.count) index = list.count - 1;

  char path[40];
  snprintf(path, sizeof(path), "%s/%s", ALBUM_DIR, list.names[index]);
  File f = LittleFS.open(path, "r");
  if (!f || f.size() != (size_t)ALBUM_FRAME_BYTES) {
    if (f) f.close();
    epd.clear(Theme::Bg);
    headerDrawAppBar(epd, AppId::Album, "ALBUM");
    fontDrawTextWrapped(epd, Theme::MarginX, 40, 384, Theme::FooterY - 4, "Bad photo file",
                        Theme::Alert);
    epd.display();
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

  if (getAppConfig().album_show_header) {
    headerDrawAppBar(epd, AppId::Album, "ALBUM");
    char meta[24];
    snprintf(meta, sizeof(meta), "%d/%d", index + 1, list.count);
    epd.drawText(72, 7, meta, Theme::HeaderAccent, Theme::HeaderBg);
  }

  Serial.printf("[Album] show %s (%d/%d) header=%d\n", list.names[index], index + 1, list.count,
                (int)getAppConfig().album_show_header);
  return epd.display();
}
