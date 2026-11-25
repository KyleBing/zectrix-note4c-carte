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

// 关 modem sleep；已连则直接返回，否则连保存的 AP
bool wifiPrepareSta(uint32_t timeout_ms = 15000);
// 断连再连，清 ESP32 DNS 假死（常见表现：HTTPS connect → start_ssl_client:-1）
bool wifiForceReconnect(uint32_t timeout_ms = 12000);
// 解析主机；失败时重试；仍失败可配合 wifiForceReconnect
bool wifiResolveHost(const char *host, IPAddress &out, int tries = 3);
