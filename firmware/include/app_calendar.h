#pragma once

#include "epd_ssd2683.h"

// Draw current-month calendar from RTC (today = red bg / white text).
bool calendarShow(EpdSsd2683 &epd);
