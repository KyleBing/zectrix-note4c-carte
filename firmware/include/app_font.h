#pragma once

#include "epd_ssd2683.h"

bool fontBegin();  // mount LittleFS + load HZK16 / ASC16 / uni2gb
bool fontReady();

// Draw UTF-8 text with 16px line height (ASCII 8x16 + CJK 16x16).
// max_w is the usable width from wrap origin (see wrap_x).
// wrap_x: x used when wrapping to the next line (hanging indent); <0 means same as x.
// strike: draw a mid-line strike only across each line's glyph width (not full max_w).
// line_gap: 换行额外行距（像素），默认 0。
// Returns y just below the last drawn line.
int fontDrawTextWrapped(EpdSsd2683 &epd, int x, int y, int max_w, int max_y, const char *utf8,
                        EpdColor fg = EPD_BLACK, EpdColor bg = EPD_WHITE, int wrap_x = -1,
                        bool strike = false, EpdColor strike_color = EPD_BLACK, int line_gap = 0);

// 1× 字，亮像素两色棋盘抖动（如周六标签红黄混）。
int fontDrawTextWrappedDither(EpdSsd2683 &epd, int x, int y, int max_w, int max_y, const char *utf8,
                              EpdColor a = EPD_RED, EpdColor b = EPD_YELLOW, EpdColor bg = EPD_WHITE,
                              int wrap_x = -1);

// Half-size glyphs (~8px CJK / 4px ASCII), for secondary notes.
int fontDrawTextWrappedSmall(EpdSsd2683 &epd, int x, int y, int max_w, int max_y, const char *utf8,
                             EpdColor fg = EPD_YELLOW, EpdColor bg = EPD_WHITE, int wrap_x = -1);

int fontLineHeight();
int fontLineHeightSmall();
int fontLineHeightDouble();  // 32px：ASCII 16x32 + CJK 32x32

// 2× HZK/ASC（像素放大），给天气地点等标题用。
// line_gap：换行时额外行距（像素），默认 0。
int fontDrawTextWrappedDouble(EpdSsd2683 &epd, int x, int y, int max_w, int max_y, const char *utf8,
                              EpdColor fg = EPD_BLACK, EpdColor bg = EPD_WHITE, int wrap_x = -1,
                              int line_gap = 0);

// 2× 字，亮像素红黄棋盘抖动（湿度等混色）。
int fontDrawTextWrappedDoubleDither(EpdSsd2683 &epd, int x, int y, int max_w, int max_y,
                                    const char *utf8, EpdColor a = EPD_RED, EpdColor b = EPD_YELLOW,
                                    EpdColor bg = EPD_WHITE, int wrap_x = -1);
