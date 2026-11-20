#pragma once

#include <Arduino.h>

// Idle = off. Patterns indicate transient / sticky states.
enum class LedPattern : uint8_t {
  Off = 0,
  On,
  BlinkSlow,   // WiFi connecting
  BlinkFast,   // network fetch
  Breathe,     // EPD page refresh
  DoublePulse, // config web / AP ready
  ErrorBurst,  // failure
};

void ledBegin();
void ledSet(LedPattern pattern);
LedPattern ledGet();
void ledUpdate();  // call from loop()
void ledOff();
// Distinctive blink even while a key is still held (long-press threshold reached).
void ledAckFlash();
// Power-on cue: three fast blinks (blocking), then off. No EPD refresh.
void ledBootFlash();
