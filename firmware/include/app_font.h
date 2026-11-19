#pragma once

#include "epd_ssd2683.h"

bool fontBegin();  // mount LittleFS + load HZK16 / ASC16 / uni2gb
bool fontReady();

// Draw UTF-8 text with 16px line height (ASCII 8x16 + CJK 16x16).
// max_w is the usable width from wrap origin (see wrap_x).
// wrap_x: x used when wrapping to the next line (hanging indent); <0 means same as x.
// strike: draw a mid-line strike only across each line's glyph width (not full max_w).
// Returns y just below the last drawn line.
int fontDrawTextWrapped(EpdSsd2683 &epd, int x, int y, int max_w, int max_y, const char *utf8,
                        EpdColor fg = EPD_BLACK, EpdColor bg = EPD_WHITE, int wrap_x = -1,
                        bool strike = false, EpdColor strike_color = EPD_BLACK);

// Half-size glyphs (~8px CJK / 4px ASCII), for secondary notes.
int fontDrawTextWrappedSmall(EpdSsd2683 &epd, int x, int y, int max_w, int max_y, const char *utf8,
                             EpdColor fg = EPD_YELLOW, EpdColor bg = EPD_WHITE, int wrap_x = -1);

int fontLineHeight();
int fontLineHeightSmall();
