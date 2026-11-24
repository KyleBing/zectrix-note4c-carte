#pragma once

#include <Arduino.h>

static constexpr const char *TODO_LOCAL_PATH = "/todo/local.txt";

// 本机待办正文（日记原文格式）；空表示尚未设置。
bool todoLocalLoad(String &out);
bool todoLocalSave(const String &content);
bool todoLocalExists();
