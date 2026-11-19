#pragma once

#include "epd_ssd2683.h"

bool fontBegin();  // mount LittleFS + load HZK16 / ASC16 / uni2gb
bool fontReady();

// Draw UTF-8 text with 16px line height (ASCII 8x16 + CJK 16x16).
// max_w is the right edge width from the left margin used for wrapping (see wrap_x).
// wrap_x: x used when wrapping to the next line (hanging indent); <0 means same as x.
// Returns y just below the last drawn line.
int fontDrawTextWrapped(EpdSsd2683 &epd, int x, int y, int max_w, int max_y, const char *utf8,
                        EpdColor fg = EPD_BLACK, EpdColor bg = EPD_WHITE, int wrap_x = -1);

int fontLineHeight();
