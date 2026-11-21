#pragma once

#include "epd_ssd2683.h"

// Draw current-month calendar from RTC (today = red bg / white text).
bool calendarShow(EpdSsd2683 &epd);

// 默认每日 0 点跨日刷新：上次画成功的本地日期与今天不同则到期（需有效系统时间）。
bool calendarAutoRefreshDue();
