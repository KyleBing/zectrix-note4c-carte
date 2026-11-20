#pragma once

#include "epd_ssd2683.h"

// Footer key glyphs: equilateral-ish isosceles triangles / circle, drawn in ICON_SIZE box.
static constexpr int ICON_SIZE = 7;        // glyph bounding box (square)
static constexpr int ICON_BADGE_SIZE = 11; // yellow square background
static constexpr int ICON_BOOT_R = 2;      // circle radius inside ICON_SIZE

// x = left of glyph box, cy = vertical center of glyph box
void iconDrawArrowUp(EpdSsd2683 &epd, int x, int cy, EpdColor color);
void iconDrawArrowDown(EpdSsd2683 &epd, int x, int cy, EpdColor color);
void iconDrawBootCircle(EpdSsd2683 &epd, int x, int cy, EpdColor color);
