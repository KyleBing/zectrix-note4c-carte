#pragma once

#include <Arduino.h>

// Start config portal: prefer STA if WiFi saved, else AP (note4c/note4c).
// force_ap=true always opens hotspot.
bool webStart(bool force_ap = false);
void webLoop();
void webStop();
bool webIsRunning();
const char *webUrl();      // http://x.x.x.x
const char *webModeName(); // AP / STA
