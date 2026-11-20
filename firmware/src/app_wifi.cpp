#include "app_wifi.h"
#include "app_config.h"
#include "app_power.h"
#include "app_led.h"
#include "app_rtc.h"

#include <WiFi.h>
#include <cstring>

static constexpr uint32_t kRetryMs = 3500;

static bool tryConnectOne(const char *ssid, const char *password, uint32_t timeout_ms) {
  if (!ssid || !ssid[0]) return false;
  if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == ssid) return true;

  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect(true);
    delay(50);
  }

  const bool radio_was_off = WiFi.getMode() == WIFI_OFF;
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  if (radio_was_off) delay(100);
  WiFi.begin(ssid, password ? password : "");

  const uint32_t deadline = millis() + timeout_ms;
  uint32_t next_retry = millis() + kRetryMs;
  while ((int32_t)(millis() - deadline) < 0) {
    if (WiFi.status() == WL_CONNECTED) return true;
    if ((int32_t)(millis() - next_retry) >= 0) {
      WiFi.begin(ssid, password ? password : "");
      next_retry = millis() + kRetryMs;
    }
    powerPoll();  // allow DOWN 3s power-off while connecting
    ledUpdate();
    delay(100);
  }
  return WiFi.status() == WL_CONNECTED;
}

bool wifiConnectSaved(uint32_t timeout_ms) {
  const AppConfig &cfg = getAppConfig();
  if (cfg.wifi_count <= 0) return false;

  // Prefer active profile first
  int order[WIFI_PROFILE_MAX];
  int n = 0;
  for (int i = 0; i < cfg.wifi_count; i++) {
    if (cfg.wifi_active[0] && strcmp(cfg.wifis[i].ssid, cfg.wifi_active) == 0) {
      order[n++] = i;
      break;
    }
  }
  for (int i = 0; i < cfg.wifi_count; i++) {
    bool already = false;
    for (int j = 0; j < n; j++) {
      if (order[j] == i) {
        already = true;
        break;
      }
    }
    if (!already) order[n++] = i;
  }

  const uint32_t each = timeout_ms / (uint32_t)n;
  for (int k = 0; k < n; k++) {
    const WifiProfile &p = cfg.wifis[order[k]];
    Serial.printf("[WiFi] try %s\n", p.ssid);
    if (tryConnectOne(p.ssid, p.password, each < 4000 ? 4000 : each)) {
      Serial.printf("[WiFi] connected %s ip=%s rssi=%d\n", p.ssid, WiFi.localIP().toString().c_str(),
                    WiFi.RSSI());
      // Remember last successful as active
      if (strcmp(cfg.wifi_active, p.ssid) != 0) {
        setAppConfigWifiActive(p.ssid);
      }
      rtcSyncOnWifiConnected(8000);  // 复位后 / RTC 无效 / 超过周期才 NTP
      return true;
    }
  }
  Serial.println("[WiFi] all profiles failed");
  return false;
}

bool wifiStartAp(const char *ssid, const char *pass) {
  if (!ssid || !ssid[0]) ssid = WIFI_AP_SSID;
  if (!pass) pass = WIFI_AP_PASS;
  // ESP32 SoftAP WPA2 requires password length 8..63; shorter → softAP fails.
  if (pass[0] != '\0' && strlen(pass) < 8) {
    Serial.printf("[WiFi] AP pass too short (%u); using default\n", (unsigned)strlen(pass));
    pass = WIFI_AP_PASS;
  }

  wifiRadioOff();
  delay(150);
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  delay(50);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  const bool ok = WiFi.softAP(ssid, pass, 1 /*channel*/, 0 /*ssid_hidden*/, 4 /*max_conn*/);
  Serial.printf("[WiFi] AP ssid=%s pass=%s ip=%s %s\n", ssid, pass,
                WiFi.softAPIP().toString().c_str(), ok ? "ok" : "FAIL");
  if (!ok) {
    // last resort: open AP (no password)
    const bool open_ok = WiFi.softAP(ssid, nullptr, 1, 0, 4);
    Serial.printf("[WiFi] AP open fallback %s\n", open_ok ? "ok" : "FAIL");
    return open_ok;
  }
  return true;
}

void wifiStopAp() {
  WiFi.softAPdisconnect(true);
}

bool wifiIsStaConnected() {
  return WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED;
}

String wifiStaIp() { return WiFi.localIP().toString(); }
String wifiApIp() { return WiFi.softAPIP().toString(); }
int wifiStaRssi() { return wifiIsStaConnected() ? WiFi.RSSI() : 0; }
String wifiCurrentSsid() { return WiFi.SSID(); }

void wifiRadioOff() {
  if (WiFi.getMode() != WIFI_OFF || WiFi.status() == WL_CONNECTED) {
    const bool was = WiFi.status() == WL_CONNECTED;
    WiFi.disconnect(false);
    if (was) delay(80);
    WiFi.mode(WIFI_OFF);
  }
}
