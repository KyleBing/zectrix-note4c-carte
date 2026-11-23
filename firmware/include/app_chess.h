#pragma once

#include "epd_ssd2683.h"

// 国际象棋残局展示；BOOT 切下一局。index 会钳到合法范围。
bool chessShow(EpdSsd2683 &epd, int &index);
int chessEndgameCount();
