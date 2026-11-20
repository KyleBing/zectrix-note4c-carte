#pragma once

#include <Arduino.h>

// Try wifi_active first, then other saved profiles. Returns true if STA connected.
bool wifiConnectSaved(uint32_t timeout_ms = 15000);

// WPA2 SoftAP password must be >= 8 chars (ESP32).
bool wifiStartAp(const char *ssid = "Note4C-Carte", const char *pass = "12345678");
void wifiStopAp();

static constexpr const char *WIFI_AP_SSID = "Note4C-Carte";
static constexpr const char *WIFI_AP_PASS = "12345678";

bool wifiIsStaConnected();
String wifiStaIp();
String wifiApIp();
int wifiStaRssi();
String wifiCurrentSsid();

void wifiRadioOff();
