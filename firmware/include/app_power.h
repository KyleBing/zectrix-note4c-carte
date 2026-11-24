#pragma once

#include <Arduino.h>
#include "app_apps.h"

// Quiet window after last short-release before a multi-click burst is taken
// (BOOT album/weather skip, UP/DOWN app skip).
static constexpr uint32_t POWER_KEY_COMBO_MS = 500;

void powerBegin();

// 关机前用于计算闹钟的前台 App（main 在切 App / setup 时更新）。
void powerNotifyForegroundApp(AppId id);

// Call often (loop + blocking waits). If DOWN held ~2s → soft off (deep sleep).
// latch_short=true counts short DOWN/UP releases into combo bursts.
// EPD interrupt path: disarm until release, then latch_short=true so new keys abort.
void powerPoll(bool latch_short = true);

// DOWN short-release combo (same timing as BOOT).
uint8_t powerTakeDownBurst();  // 0 until quiet POWER_KEY_COMBO_MS after last click
uint8_t powerTakeDownClicksNow();
void powerClearDownClicks();

// UP short-release combo (prev app); long hold latched separately.
uint8_t powerTakeUpBurst();
uint8_t powerTakeUpClicksNow();
void powerClearUpClicks();
bool powerTakeUpLong();  // true once after UP held ~1.5s
bool powerUpHeld();

// BOOT releases: counted even during EPD waitBusy (powerPoll).
uint8_t powerTakeBootBurst();      // 0 until quiet after last click
uint8_t powerTakeBootClicksNow();  // drain immediately (web exit)
bool powerBootHeld();

// EPD mid-refresh interrupt helpers.
void powerNavDisarmUntilRelease();  // ignore trigger-key release; arm after all keys up
bool powerNavArmed();
bool powerNavAnyHeld();
bool powerNavShouldAbort();  // armed and (pending click or nav key down / UP long)
void powerClearNavClicks();  // BOOT + DOWN + UP shorts (+ clear UP long latch)

// Soft power-off:
// - 当前 App 需要定时醒 → 保持 PWR_HOLD + ESP 定时（+可选 RTC）深睡；DOWN 可醒
// - 否则 → 松 PWR_HOLD（电池近断电）+ 仅 DOWN 醒
void powerOffNow();

// 本次启动应跑「定时刷屏再睡」（TIMER / RTC / 定时深睡标记），非单纯下键交互开机。
bool powerWokeFromRtcAlarm();

// 下一次定时醒来的 unix（本地换算）；0 = 不需要定时醒。
time_t powerNextWakeUnix(AppId app);

// Firmware idle auto-off (0 = disabled). Not a SoC default.
void powerIdleTouch();
void powerIdleSetTimeoutSec(uint32_t sec);
uint32_t powerIdleTimeoutSec();
void powerIdlePoll(bool inhibit);

// Pet task WDT + yield. Call from blocking waits (EPD, HTTP body, WiFi).
void wdtPet();

// Temporarily unsubscribe whatever TWDT already watches (loop / idle0 / idle1).
// Restores only those; does not newly enable loop WDT. Nested.
void wdtLongOpBegin();
void wdtLongOpEnd();

struct WdtLongOpGuard {
  WdtLongOpGuard() { wdtLongOpBegin(); }
  ~WdtLongOpGuard() { wdtLongOpEnd(); }
};
