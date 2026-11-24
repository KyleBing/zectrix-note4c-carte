#pragma once

#include "epd_ssd2683.h"

#include <Arduino.h>

// 练习内容等级（可多选，bitmask）
static constexpr uint8_t CHESS_LV_OPENING = 1u << 0;  // 开局
static constexpr uint8_t CHESS_LV_TACTIC = 1u << 1;   // 战术
static constexpr uint8_t CHESS_LV_MIDDLE = 1u << 2;   // 中局
static constexpr uint8_t CHESS_LV_ENDGAME = 1u << 3;  // 残局
static constexpr uint8_t CHESS_LV_ALL =
    (uint8_t)(CHESS_LV_OPENING | CHESS_LV_TACTIC | CHESS_LV_MIDDLE | CHESS_LV_ENDGAME);

// 国际象棋练习展示；BOOT 切下一局。index 为过滤后列表下标。
bool chessShow(EpdSsd2683 &epd, int &index);
// 当前配置等级下的局面数
int chessPositionCount();
// 某等级内置局面数（不看配置过滤；level 为 CHESS_LV_* 单 bit）
int chessCountForLevel(uint8_t level);
// 前进 steps 局：顺序则 +steps，乱序则随机（尽量不重复当前）
void chessAdvanceIndex(int &index, int steps);
