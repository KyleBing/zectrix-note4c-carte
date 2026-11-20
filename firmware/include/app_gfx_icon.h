#pragma once

#include "epd_ssd2683.h"

// design/weather/*_active.svg quantized to 4-color 2bpp (tools/rasterize_icons.py).
enum class WxIcon : uint8_t {
  Sunny = 0,
  Cloudy,
  Overcast,
  Rain,
  Sprinkle,
  Thunder,
  Snow,
  Fog,
  Smog,
  Sandstorm,
  Tornado,
  Count,
};

WxIcon wxIconFromCode(const char *qweather_icon);

// dst is on-screen size. White is transparent on light backgrounds.
void wxIconDraw(EpdSsd2683 &epd, int x, int y, WxIcon id, int dst);

// design/icon/ap_active.svg / lan_active.svg. skip_white=false keeps white on dark bg.
void webModeIconDraw(EpdSsd2683 &epd, int x, int y, bool ap_mode, int dst, bool skip_white = true);
