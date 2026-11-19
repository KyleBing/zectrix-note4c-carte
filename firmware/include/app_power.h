#pragma once

#include <Arduino.h>

void powerBegin();

// Call often (loop + blocking waits). If DOWN held ~3s → release PWR_HOLD and halt.
// Works during WiFi / EPD busy so shutdown is available in any situation.
void powerPoll();

// Immediate soft power-off (release latch). USB may keep board alive.
void powerOffNow();
