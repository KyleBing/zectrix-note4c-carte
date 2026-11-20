#pragma once

#include "epd_ssd2683.h"

// Carte 四色主调（墨水屏可用色：黑 / 白 / 黄 / 红）
//   背景 WHITE · 正文 BLACK · 强调/状态 YELLOW · 警示/标题 RED
namespace Theme {
constexpr EpdColor Bg = EPD_WHITE;
constexpr EpdColor Fg = EPD_BLACK;
constexpr EpdColor Accent = EPD_YELLOW;
constexpr EpdColor Alert = EPD_RED;
constexpr EpdColor HeaderBg = EPD_BLACK;
constexpr EpdColor HeaderFg = EPD_WHITE;
constexpr EpdColor HeaderAccent = EPD_YELLOW;
constexpr int HeaderH = 22;
constexpr int FooterY = 276;
constexpr int MarginX = 8;
}  // namespace Theme
