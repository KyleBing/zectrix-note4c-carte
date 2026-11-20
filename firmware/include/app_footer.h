#pragma once

#include "epd_ssd2683.h"

// Footer: yellow square badges (○ / ▼ / ▲ isosceles) + labels + version.
// boot_action e.g. "refresh" / "mode" / "next"
// dn_action   e.g. "next" / "nextApp"
void footerDraw(EpdSsd2683 &epd, const char *boot_action, const char *dn_action = "next");
