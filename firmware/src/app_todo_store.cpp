#include "app_todo_store.h"

#include <FS.h>
#include <LittleFS.h>

static bool ensureTodoDir() {
  if (LittleFS.exists("/todo")) return true;
  return LittleFS.mkdir("/todo");
}

bool todoLocalExists() { return LittleFS.exists(TODO_LOCAL_PATH); }

bool todoLocalLoad(String &out) {
  out = "";
  if (!LittleFS.exists(TODO_LOCAL_PATH)) return false;
  File f = LittleFS.open(TODO_LOCAL_PATH, "r");
  if (!f) return false;
  out = f.readString();
  f.close();
  return true;
}

bool todoLocalSave(const String &content) {
  if (!ensureTodoDir()) return false;
  File f = LittleFS.open(TODO_LOCAL_PATH, "w");
  if (!f) return false;
  const size_t n = f.print(content);
  f.close();
  return n == content.length() || (content.isEmpty() && n == 0);
}
