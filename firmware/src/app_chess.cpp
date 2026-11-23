#include "app_chess.h"

#include "app_errorlog.h"
#include "app_font.h"
#include "app_theme.h"
#include "chess_sprites.h"

#include <cstdio>
#include <cstring>

// 棋子种类：空 / 王后车象马兵
enum class Piece : uint8_t { None = 0, K, Q, R, B, N, P };

struct Sq {
  Piece p = Piece::None;
  bool white = true;
};

struct Endgame {
  const char *name;   // English title
  const char *fen;    // board + side to move
  const char *goal;   // English goal hint
};

// Classic endgames / short tactics (FEN from textbooks; step hints are illustrative)
static const Endgame kEndgames[] = {
    {"Lucena", "3K4/3P2k1/8/8/8/8/r7/5R2 w", "White builds bridge; promote"},
    {"Philidor", "1r3k2/R7/8/5PK1/8/8/8/8 b", "Black holds 3rd rank; draw"},
    {"Opposition", "8/8/8/3k4/8/3KP3/8/8 w", "White takes opposition; promote"},
    {"Vancura", "R7/6k1/P4r2/8/3K4/8/8/8 w", "Rook+pawn edge; can draw"},
    {"Q vs K", "8/8/8/4k3/8/4K3/8/3Q4 w", "White mates in ~10"},
    {"R vs K", "8/8/8/4k3/8/4K3/8/R7 w", "White mates in ~16"},
    {"2R vs K", "8/8/8/4k3/8/8/8/R3K2R w", "White mates in ~5"},
    {"2B vs K", "8/8/8/4k3/8/4K3/8/2B1B3 w", "White mates in ~18"},
    {"BN vs K", "8/8/8/4k3/8/4K3/8/2B1N3 w", "Longest mate ~33"},
    {"Back rank", "6k1/5ppp/8/8/8/8/5PPP/4R1K1 w", "White mates in 1"},
    {"Smothered", "5r1k/6pp/7N/8/8/8/8/4K2Q w", "White mates in 2"},
    {"Saavedra", "8/8/1KP5/3r4/8/8/8/k7 w", "White wins in ~6"},
    {"Reti", "7K/8/k1P5/7p/8/8/8/8 w", "White stalemate or promote"},
    {"Q vs R", "8/8/8/4k3/4r3/4K3/8/3Q4 w", "White wins in ~20"},
    {"Pawn race", "8/4P3/8/8/8/8/4p3/4K1k1 w", "White promotes first"},
    {"Escort", "8/8/8/3k4/8/2PK4/8/8 w", "King escorts pawn; promote"},
    {"R+P vs R", "8/8/8/3k4/3P4/3K4/8/3r3R w", "White advances; precise"},
    {"2P vs K", "8/8/8/3k4/8/2PPK3/8/8 w", "White promotes in ~9"},
    {"Opp. bishops", "8/8/8/3k4/2b5/3K4/4B3/8 w", "Opposite bishops; usually draw"},
    {"N vs P", "8/8/8/3kP3/8/3K4/8/5N2 w", "Knight stops pawn; win/draw"},
};

static constexpr int kEndgameN = (int)(sizeof(kEndgames) / sizeof(kEndgames[0]));

int chessEndgameCount() { return kEndgameN; }

static Piece pieceFromChar(char c, bool &white) {
  white = (c >= 'A' && c <= 'Z');
  const char u = white ? c : (char)(c - 'a' + 'A');
  switch (u) {
    case 'K':
      return Piece::K;
    case 'Q':
      return Piece::Q;
    case 'R':
      return Piece::R;
    case 'B':
      return Piece::B;
    case 'N':
      return Piece::N;
    case 'P':
      return Piece::P;
    default:
      return Piece::None;
  }
}

static bool parseFenBoard(const char *fen, Sq board[8][8], bool &white_to_move) {
  for (int r = 0; r < 8; r++)
    for (int f = 0; f < 8; f++) board[r][f] = {};

  int r = 0, f = 0;
  const char *p = fen;
  while (*p && *p != ' ') {
    if (*p == '/') {
      r++;
      f = 0;
      if (r > 7) return false;
      p++;
      continue;
    }
    if (*p >= '1' && *p <= '8') {
      f += *p - '0';
      if (f > 8) return false;
      p++;
      continue;
    }
    if (f > 7 || r > 7) return false;
    bool w = true;
    const Piece pc = pieceFromChar(*p, w);
    if (pc == Piece::None) return false;
    board[r][f].p = pc;
    board[r][f].white = w;
    f++;
    p++;
  }
  white_to_move = true;
  if (*p == ' ') {
    p++;
    if (*p == 'b' || *p == 'B') white_to_move = false;
  }
  return r == 7 && f == 8;
}

static void countCaptured(const Sq board[8][8], Piece *cap_w, int &nw, Piece *cap_b, int &nb) {
  int need[7] = {0, 0, 1, 2, 2, 2, 8};
  int have_w[7] = {};
  int have_b[7] = {};
  for (int r = 0; r < 8; r++) {
    for (int f = 0; f < 8; f++) {
      const Sq &s = board[r][f];
      if (s.p == Piece::None || s.p == Piece::K) continue;
      const int i = (int)s.p;
      if (i < 2 || i > 6) continue;
      if (s.white)
        have_w[i]++;
      else
        have_b[i]++;
    }
  }
  nw = nb = 0;
  static const Piece order[] = {Piece::Q, Piece::R, Piece::B, Piece::N, Piece::P};
  for (Piece p : order) {
    const int i = (int)p;
    int miss_w = need[i] - have_w[i];
    int miss_b = need[i] - have_b[i];
    if (miss_w < 0) miss_w = 0;
    if (miss_b < 0) miss_b = 0;
    while (miss_w-- > 0 && nw < 16) cap_w[nw++] = p;
    while (miss_b-- > 0 && nb < 16) cap_b[nb++] = p;
  }
}

// 1:1 像素绘制；黑子只画墨点，白子行内左右墨点之间填白
static void drawPieceSprite(EpdSsd2683 &epd, int cx, int cy, Piece p, bool white) {
  if (p == Piece::None) return;
  const ChessSpr spr = chessSprite((int)p, white);
  if (!spr.rows || spr.h <= 0) return;

  const int x0 = cx - kChessSprW / 2;
  const int y0 = cy - spr.h / 2;

  for (int row = 0; row < spr.h; row++) {
    const uint16_t bits = pgm_read_word(&spr.rows[row]);
    if (white) {
      // 找本行最左/最右墨点，中间空位填白（实体感）
      int lo = -1, hi = -1;
      for (int col = 0; col < kChessSprW; col++) {
        if (bits & (1u << (kChessSprW - 1 - col))) {
          if (lo < 0) lo = col;
          hi = col;
        }
      }
      if (lo < 0) continue;
      for (int col = lo; col <= hi; col++) {
        const bool ink = (bits & (1u << (kChessSprW - 1 - col))) != 0;
        epd.setPixel(x0 + col, y0 + row, ink ? Theme::Fg : Theme::Bg);
      }
    } else {
      for (int col = 0; col < kChessSprW; col++) {
        if (bits & (1u << (kChessSprW - 1 - col))) {
          epd.setPixel(x0 + col, y0 + row, Theme::Fg);
        }
      }
    }
  }
}

bool chessShow(EpdSsd2683 &epd, int &index) {
  if (!epd.ok()) {
    errorLog("chess", "EPD not ready");
    return false;
  }
  if (index < 0) index = 0;
  index %= kEndgameN;

  const Endgame &eg = kEndgames[index];
  Sq board[8][8];
  bool white_tm = true;
  if (!parseFenBoard(eg.fen, board, white_tm)) {
    errorLog("chess", "bad FEN");
    return false;
  }

  Piece cap_by_black[16];
  Piece cap_by_white[16];
  int n_by_b = 0, n_by_w = 0;
  countCaptured(board, cap_by_black, n_by_b, cap_by_white, n_by_w);

  epd.clear(Theme::Bg);

  // 无 header；棋盘尽量大并居中（上下不放字）
  constexpr int kScreenW = EpdSsd2683::kWidth;
  constexpr int kScreenH = EpdSsd2683::kHeight;
  // 王高 33，格子至少 34；取 36 铺满高度
  int cell = kScreenH / 8;
  if (cell > 36) cell = 36;
  if (cell < 34) cell = 34;
  const int board_px = cell * 8;
  const int board_x = (kScreenW - board_px) / 2;
  const int board_y = (kScreenH - board_px) / 2;
  const int side_l = board_x;
  const int side_r = kScreenW - (board_x + board_px);

  // 左侧：英文说明（不放在棋盘上下）
  {
    char idxbuf[12];
    snprintf(idxbuf, sizeof(idxbuf), "%d/%d", index + 1, kEndgameN);
    int y = board_y;
    y = fontDrawTextWrapped(epd, 2, y, side_l - 4, board_y + board_px, idxbuf, Theme::Alert) + 2;
    y = fontDrawTextWrapped(epd, 2, y, side_l - 4, board_y + board_px, eg.name, Theme::Fg) + 2;
    y = fontDrawTextWrapped(epd, 2, y, side_l - 4, board_y + board_px, white_tm ? "White" : "Black",
                            Theme::Accent) +
        4;
    if (eg.goal && eg.goal[0]) {
      fontDrawTextWrapped(epd, 2, y, side_l - 4, board_y + board_px, eg.goal, Theme::Fg);
    }
  }

  // 右侧：吃子列表（1:1 像素棋子）
  {
    int ry = board_y;
    const int rx = board_x + board_px + side_r / 2;
    for (int i = 0; i < n_by_w; i++) {
      const ChessSpr s = chessSprite((int)cap_by_white[i], false);
      if (ry + s.h > board_y + board_px) break;
      drawPieceSprite(epd, rx, ry + s.h / 2, cap_by_white[i], false);
      ry += s.h + 2;
    }
    for (int i = 0; i < n_by_b; i++) {
      const ChessSpr s = chessSprite((int)cap_by_black[i], true);
      if (ry + s.h > board_y + board_px) break;
      drawPieceSprite(epd, rx, ry + s.h / 2, cap_by_black[i], true);
      ry += s.h + 2;
    }
  }

  // 棋盘 + 1:1 棋子
  for (int r = 0; r < 8; r++) {
    for (int f = 0; f < 8; f++) {
      const int x = board_x + f * cell;
      const int y0 = board_y + r * cell;
      const bool dark = ((r + f) & 1) != 0;
      if (dark) epd.fillRect(x, y0, cell, cell, Theme::Accent);
      epd.drawRect(x, y0, cell, cell, Theme::Fg);
      const Sq &s = board[r][f];
      if (s.p != Piece::None) {
        drawPieceSprite(epd, x + cell / 2, y0 + cell / 2, s.p, s.white);
      }
    }
  }
  epd.drawRect(board_x - 1, board_y - 1, board_px + 2, board_px + 2, Theme::Fg);

  epd.display();
  return true;
}
