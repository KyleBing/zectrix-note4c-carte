#pragma once

#include <Arduino.h>

// Quiet window after last short-release before a multi-click burst is taken
// (BOOT album/weather skip, UP/DOWN app skip).
static constexpr uint32_t POWER_KEY_COMBO_MS = 500;

void powerBegin();

// Call often (loop + blocking waits). If DOWN held ~3s → soft off (deep sleep).
// latch_short=true counts short DOWN releases into the combo burst.
// During EPD refresh pass latch_short=false so bumps don't queue as app switches.
void powerPoll(bool latch_short = true);

// DOWN short-release combo (same timing as BOOT).
uint8_t powerTakeDownBurst();  // 0 until quiet POWER_KEY_COMBO_MS after last click
void powerClearDownClicks();

// BOOT releases: counted even during EPD waitBusy (powerPoll).
uint8_t powerTakeBootBurst();      // 0 until quiet after last click
uint8_t powerTakeBootClicksNow();  // drain immediately (web exit)
bool powerBootHeld();

// Soft power-off: release PWR_HOLD, deep sleep; press DOWN to wake (full reboot).
void powerOffNow();

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
