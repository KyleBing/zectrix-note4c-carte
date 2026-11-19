#pragma once

#include <Arduino.h>

// Try wifi_active first, then other saved profiles. Returns true if STA connected.
bool wifiConnectSaved(uint32_t timeout_ms = 15000);

// WPA2 SoftAP password must be >= 8 chars (ESP32). Defaults: note4c / note4c4c
bool wifiStartAp(const char *ssid = "note4c", const char *pass = "note4c4c");
void wifiStopAp();

static constexpr const char *WIFI_AP_SSID = "note4c";
static constexpr const char *WIFI_AP_PASS = "note4c4c";

bool wifiIsStaConnected();
String wifiStaIp();
String wifiApIp();
int wifiStaRssi();
String wifiCurrentSsid();

void wifiRadioOff();
