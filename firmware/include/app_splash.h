#pragma once

#include "epd_ssd2683.h"

// 刷机后首屏：LOGO + Carte + 版本 + 作者 + GitHub。
// 每个 APP_BUILD 只自动展示一次（NVS 记录）。
bool splashShowIfNeeded(EpdSsd2683 &epd);
void splashDraw(EpdSsd2683 &epd);
