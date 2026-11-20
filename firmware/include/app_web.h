#pragma once

#include <Arduino.h>

// Start config portal. force_ap=true always opens hotspot.
// allow_ap_fallback=false：只连已存 WiFi，失败不切 AP。
bool webStart(bool force_ap = false, bool allow_ap_fallback = true);
void webLoop();
void webStop();
bool webIsRunning();
const char *webUrl();      // http://x.x.x.x
bool webIsAp();
const char *webModeName();  // 「AP 模式」/「同 WiFi 站点」
