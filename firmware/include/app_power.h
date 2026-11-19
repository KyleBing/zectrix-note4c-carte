#pragma once

#include <Arduino.h>

void powerBegin();

// Call often (loop + blocking waits). If DOWN held ~3s → release PWR_HOLD and halt.
// latch_short=true also latches a short-release for app switching.
// During EPD refresh pass latch_short=false so bumps don't queue as app switches.
void powerPoll(bool latch_short = true);

// True once after a short DOWN release (not after long-hold power-off path).
bool powerConsumeShortPress();

// Drop any latched short press without acting on it.
void powerClearShortPress();

// BOOT releases: counted even during EPD waitBusy (powerPoll). Combo window 500ms.
uint8_t powerTakeBootBurst();      // 0 until 500ms quiet after last click
uint8_t powerTakeBootClicksNow();  // drain immediately (web exit)
bool powerBootHeld();

// Immediate soft power-off (release latch). USB may keep board alive.
void powerOffNow();

// Firmware idle auto-off (0 = disabled). Not a SoC default.
void powerIdleTouch();
void powerIdleSetTimeoutSec(uint32_t sec);
uint32_t powerIdleTimeoutSec();
void powerIdlePoll(bool inhibit);
