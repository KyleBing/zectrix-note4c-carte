#pragma once

#include <Arduino.h>

// Idle = off. Patterns indicate transient / sticky states.
enum class LedPattern : uint8_t {
  Off = 0,
  On,
  BlinkSlow,   // WiFi connecting
  BlinkFast,   // network fetch
  Breathe,     // reserved (刷屏过程不再用灯；保留枚举兼容)
  DoublePulse, // config web / AP ready
  ErrorBurst,  // failure
};

void ledBegin();
void ledSet(LedPattern pattern);
LedPattern ledGet();
void ledUpdate();  // call from loop()
void ledOff();
// 深睡前锁住熄灭电平，避免 GPIO 浮空微亮。
void ledHoldOffForSleep();
// Distinctive blink even while a key is still held (long-press threshold reached).
void ledAckFlash();
// Page refresh finished: one short blink (blocking), then off.
void ledDoneFlash();
// Power-on cue: three fast blinks (blocking), then off. No EPD refresh.
void ledBootFlash();
