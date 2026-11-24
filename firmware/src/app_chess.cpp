#include "app_chess.h"

#include "app_config.h"
#include "app_errorlog.h"
#include "app_font.h"
#include "app_theme.h"
#include "chess_pixel_icons.h"
#include "chess_sprites.h"

#include <cstdio>
#include <cstring>
#include <esp_random.h>

// 棋子种类：空 / 王后车象马兵
enum class Piece : uint8_t { None = 0, K, Q, R, B, N, P };

struct Sq {
  Piece p = Piece::None;
  bool white = true;
};

struct Endgame {
  const char *name;  // 中文标题（宜短，左侧窄栏）
  const char *fen;   // board + side to move
  const char *goal;  // 中文练习提示
  uint8_t level;     // CHESS_LV_*
};

// 练习局面：开局 / 战术 / 中局 / 残局
static const Endgame kEndgames[] = {
    // —— 开局入门 ——
    {"起始局面", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w", "认识子力摆放", CHESS_LV_OPENING},
    {"王兵开局", "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b", "占中心，出子", CHESS_LV_OPENING},
    {"意大利", "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b", "象瞄 f7", CHESS_LV_OPENING},
    {"西班牙", "r1bqkbnr/pppp1ppp/2n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R b", "牵住马，压中心",
     CHESS_LV_OPENING},
    {"西西里", "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w", "不对称反击", CHESS_LV_OPENING},
    {"法兰西", "rnbqkbnr/pppp1ppp/4p3/8/4P3/8/PPPP1PPP/RNBQKBNR w", "封闭中心入门", CHESS_LV_OPENING},
    {"卡罗卡恩", "rnbqkbnr/pp1ppppp/2p5/8/4P3/8/PPPP1PPP/RNBQKBNR w", "稳健对 e4", CHESS_LV_OPENING},
    {"后翼弃兵", "rnbqkbnr/ppp1pppp/8/3p4/2PP4/8/PP2PPPP/RNBQKBNR b", "争 d5 中心", CHESS_LV_OPENING},
    {"伦敦体系", "rnbqkb1r/ppp1pppp/5n2/3p4/3P1B2/5N2/PPP1PPPP/RN1QKB1R b", "简单易学体系",
     CHESS_LV_OPENING},
    {"王翼弃兵", "rnbqkbnr/pppp1ppp/8/4p3/4PP2/8/PPPP2PP/RNBQKBNR b", "开线攻王", CHESS_LV_OPENING},
    {"苏格兰", "r1bqkbnr/pppp1ppp/2n5/4p3/3PP3/5N2/PPP2PPP/RNBQKB1R b", "早开中心", CHESS_LV_OPENING},
    {"双马防御", "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w", "攻守皆可",
     CHESS_LV_OPENING},

    // —— 战术练习 ——
    {"学者杀", "r1bqkbnr/pppp1ppp/2n5/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w", "一步杀：后吃 f7",
     CHESS_LV_TACTIC},
    {"底线杀", "6k1/5ppp/8/8/8/8/5PPP/4R1K1 w", "一步杀：车到 e8", CHESS_LV_TACTIC},
    {"闷杀", "5r1k/6pp/7N/8/8/8/8/4K2Q w", "后送吃再马杀", CHESS_LV_TACTIC},
    {"马双重打击", "4k3/8/8/3N4/8/8/8/3QK2r w", "马将军兼捉后", CHESS_LV_TACTIC},
    {"双重打击", "6k1/5ppp/8/2q5/8/8/5PPP/2R3K1 w", "车捉后兼底线", CHESS_LV_TACTIC},
    {"象牵制", "r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w", "看清被牵的子",
     CHESS_LV_TACTIC},
    {"钉死牵制", "4k3/8/8/8/4b3/8/4R3/4K3 w", "车钉象对王", CHESS_LV_TACTIC},
    {"串击", "4k3/8/8/3q4/8/8/3R4/3K4 w", "车串击后与王", CHESS_LV_TACTIC},
    {"发现攻击", "6k1/5ppp/8/8/4b3/8/4BPPP/R5K1 w", "移象亮出车", CHESS_LV_TACTIC},
    {"双重将军", "6k1/5ppp/8/8/8/8/4BPPP/4R1K1 w", "象将军兼开车", CHESS_LV_TACTIC},
    {"消除守卫", "6k1/5ppp/8/4n3/8/8/4QPPP/6K1 w", "兑马再捉后翼", CHESS_LV_TACTIC},
    {"引入", "6k1/5ppp/8/8/8/5Q2/5PPP/6K1 w", "想办法引入杀", CHESS_LV_TACTIC},
    {"过载", "2r3k1/5ppp/8/8/8/8/5PPP/2RQ2K1 w", "守卫子负担过重", CHESS_LV_TACTIC},
    {"拦截", "6k1/5ppp/8/8/2b5/8/5PPP/R5K1 w", "挡住长线攻击", CHESS_LV_TACTIC},
    {"诱离", "6k1/5rpp/8/8/8/8/5PPP/5RK1 w", "引开车的防守", CHESS_LV_TACTIC},
    {"李加尔杀", "r2qkbnr/ppp2ppp/2np4/4p3/2B1P1b1/5N2/PPPP1PPP/RNBQK2R w", "弃后经典杀法",
     CHESS_LV_TACTIC},
    {"阿纳斯塔", "5r1k/1R4pp/8/8/8/8/5PPP/6NK w", "车马配合边杀", CHESS_LV_TACTIC},
    {"阿拉伯杀", "7k/6pp/8/8/8/8/5NPP/R5K1 w", "车到边线配合马", CHESS_LV_TACTIC},
    {"风车雏形", "6k1/5rpp/8/8/4B3/8/5PPP/R5K1 w", "发现将军连吃", CHESS_LV_TACTIC},

    // —— 中局意识 ——
    {"弱格 f7", "r1bqkb1r/pppp1ppp/2n2n2/4p1N1/2B1P3/8/PPPP1PPP/RNBQK2R w", "盯住 f7 弱点",
     CHESS_LV_MIDDLE},
    {"开放线", "r3k2r/ppp2ppp/2n5/3pp3/3PP3/2N5/PPP2PPP/R3K2R w", "争夺开放线", CHESS_LV_MIDDLE},
    {"异象中局", "r1bq1rk1/ppp2ppp/2n2n2/3p4/1b1P4/2N1PN2/PP1B1PPP/R2QKB1R w", "出子与兑子",
     CHESS_LV_MIDDLE},
    {"兵结构", "rnbq1rk1/pp2ppbp/3p1np1/8/3NP3/2N1B3/PPP2PPP/R2QKB1R w", "观察兵形弱点",
     CHESS_LV_MIDDLE},
    {"王前兵洞", "rnbq1rk1/ppp2ppp/3p1n2/4p3/1bB1P3/2N2N2/PPPP1PPP/R1BQ1RK1 b", "注意王翼空洞",
     CHESS_LV_MIDDLE},
    {"挂子提醒", "r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/3P1N2/PPP2PPP/RNBQK2R b", "先看是否被捉",
     CHESS_LV_MIDDLE},

    // —— 残局基础 ——
    {"对王", "8/8/8/3k4/8/3KP3/8/8 w", "占对王后升变", CHESS_LV_ENDGAME},
    {"后对王", "8/8/8/4k3/8/4K3/8/3Q4 w", "后配合王逼杀", CHESS_LV_ENDGAME},
    {"车对王", "8/8/8/4k3/8/4K3/8/R7 w", "车切边再逼杀", CHESS_LV_ENDGAME},
    {"双车对王", "8/8/8/4k3/8/8/8/R3K2R w", "双车梯子杀", CHESS_LV_ENDGAME},
    {"卢塞纳", "3K4/3P2k1/8/8/8/8/r7/5R2 w", "架桥升变", CHESS_LV_ENDGAME},
    {"菲利多尔", "1r3k2/R7/8/5PK1/8/8/8/8 b", "守三线求和", CHESS_LV_ENDGAME},
    {"护兵", "8/8/8/3k4/8/2PK4/8/8 w", "王护送兵升变", CHESS_LV_ENDGAME},
    {"兵赛跑", "8/4P3/8/8/8/8/4p3/4K1k1 w", "谁先升变", CHESS_LV_ENDGAME},
    {"异色象", "8/8/8/3k4/2b5/3K4/4B3/8 w", "多半和棋", CHESS_LV_ENDGAME},
    {"马对兵", "8/8/8/3kP3/8/3K4/8/5N2 w", "马能否拦住兵", CHESS_LV_ENDGAME},
};

static constexpr int kEndgameN = (int)(sizeof(kEndgames) / sizeof(kEndgames[0]));

static uint8_t chessLevelMask() {
  uint8_t m = getAppConfig().chess_levels;
  if (m == 0) m = CHESS_LV_ALL;
  return m;
}

static int chessRawAt(int filt) {
  const uint8_t mask = chessLevelMask();
  int n = 0;
  for (int i = 0; i < kEndgameN; i++) {
    if ((kEndgames[i].level & mask) == 0) continue;
    if (n == filt) return i;
    n++;
  }
  return -1;
}

int chessPositionCount() {
  const uint8_t mask = chessLevelMask();
  int n = 0;
  for (int i = 0; i < kEndgameN; i++) {
    if (kEndgames[i].level & mask) n++;
  }
  return n;
}

int chessCountForLevel(uint8_t level) {
  if (level == 0) return 0;
  int n = 0;
  for (int i = 0; i < kEndgameN; i++) {
    if (kEndgames[i].level & level) n++;
  }
  return n;
}

void chessAdvanceIndex(int &index, int steps) {
  const int n = chessPositionCount();
  if (n <= 0) {
    index = 0;
    return;
  }
  if (index < 0) index = 0;
  index %= n;

  if (getAppConfig().chess_random) {
    if (n == 1) {
      index = 0;
      return;
    }
    // 乱序：随机一局，尽量不与当前相同
    int next = (int)(esp_random() % (uint32_t)n);
    if (next == index) next = (next + 1 + (int)(esp_random() % (uint32_t)(n - 1))) % n;
    index = next;
    (void)steps;
  } else {
    int s = steps % n;
    if (s < 0) s += n;
    index = (index + s) % n;
  }
}

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

// 竖排：每字一行，返回下一 y；ASCII 宽 8、CJK 宽 16
static int chessDrawVertical(EpdSsd2683 &epd, int x, int y, int max_y, const char *utf8,
                             EpdColor fg) {
  if (!utf8 || !utf8[0]) return y;
  const int lh = fontLineHeight();
  int cy = y;
  const uint8_t *p = (const uint8_t *)utf8;
  while (*p && cy + lh <= max_y) {
    char ch[5] = {0};
    int nbytes = 1;
    int cw = 8;
    if (*p >= 0xF0) {
      nbytes = 4;
      cw = 16;
    } else if (*p >= 0xE0) {
      nbytes = 3;
      cw = 16;
    } else if (*p >= 0xC0) {
      nbytes = 2;
      cw = 16;
    }
    for (int i = 0; i < nbytes && p[i]; i++) ch[i] = (char)p[i];
    p += nbytes;
    fontDrawTextWrapped(epd, x, cy, cw + 2, cy + lh, ch, fg);
    cy += lh;
  }
  return cy;
}

// 2bpp 像素：读一格颜色（0 黑 / 1 白 / 2 黄 / 3 红）
static uint8_t chessPixel2bpp(const uint8_t *bits, int src, int x, int y) {
  if (!bits || x < 0 || y < 0 || x >= src || y >= src) return 1;
  const int bpr = (src + 3) / 4;
  const int index = y * bpr + (x >> 2);
  const int shift = 6 - 2 * (x & 3);
  return (pgm_read_byte(&bits[index]) >> shift) & 3;
}

// 仅非 White 图标；白方同一套图，绘制时黑→红。kCook→车。
static const uint8_t *chessPixelBits(Piece p, int &src) {
  src = 0;
  switch (p) {
    case Piece::K:
      src = kKing_SRC;
      return kKing;
    case Piece::Q:
      src = kQueen_SRC;
      return kQueen;
    case Piece::R:
      src = kCook_SRC;  // Cook 暂作车
      return kCook;
    case Piece::B:
      src = kBishop_SRC;
      return kBishop;
    case Piece::N:
      src = kKnight_SRC;
      return kKnight;
    case Piece::P:
      src = kPawn_SRC;
      return kPawn;
    default:
      return nullptr;
  }
}

// 边缘连通白当透明；白方：黑→红，白仍白（无描边）
static void drawPixelPiece(EpdSsd2683 &epd, int cx, int cy, const uint8_t *bits, int src,
                           bool white) {
  if (!bits || src <= 0 || src > 32) return;
  // 静态避免 ESP 栈压力（32×32 flood）
  static uint32_t pad[32];
  static int16_t qx[32 * 32];
  static int16_t qy[32 * 32];
  for (int y = 0; y < src; y++) pad[y] = 0;

  int qh = 0, qt = 0;
  auto push = [&](int x, int y) {
    if (x < 0 || y < 0 || x >= src || y >= src) return;
    const uint32_t bit = 1u << x;
    if (pad[y] & bit) return;
    if (chessPixel2bpp(bits, src, x, y) != 1) return;  // 仅 flood 白
    pad[y] |= bit;
    qx[qt] = (int16_t)x;
    qy[qt] = (int16_t)y;
    qt++;
  };
  for (int i = 0; i < src; i++) {
    push(i, 0);
    push(i, src - 1);
    push(0, i);
    push(src - 1, i);
  }
  while (qh < qt) {
    const int x = qx[qh];
    const int y = qy[qh];
    qh++;
    push(x + 1, y);
    push(x - 1, y);
    push(x, y + 1);
    push(x, y - 1);
  }

  const int x0 = cx - src / 2;
  const int y0 = cy - src / 2;
  for (int y = 0; y < src; y++) {
    for (int x = 0; x < src; x++) {
      if (pad[y] & (1u << x)) continue;
      uint8_t c = chessPixel2bpp(bits, src, x, y);
      if (white && c == 0) c = 3;  // 黑→红
      epd.setPixel(x0 + x, y0 + y, static_cast<EpdColor>(c));
    }
  }
}

// 优先 pixel 图标；缺的回退 design mask/ink sprite
static void drawPieceSprite(EpdSsd2683 &epd, int cx, int cy, Piece p, bool white) {
  if (p == Piece::None) return;

  int src = 0;
  const uint8_t *pix = chessPixelBits(p, src);
  if (pix) {
    drawPixelPiece(epd, cx, cy, pix, src, white);
    return;
  }

  const ChessSpr spr = chessSprite((int)p, white);
  if (!spr.mask || !spr.ink || spr.w <= 0 || spr.h <= 0) return;

  const int x0 = cx - spr.w / 2;
  const int y0 = cy - spr.h / 2;
  const int bpr = (spr.w + 7) / 8;

  for (int row = 0; row < spr.h; row++) {
    for (int col = 0; col < spr.w; col++) {
      const int bi = row * bpr + (col >> 3);
      const uint8_t bit = (uint8_t)(1u << (7 - (col & 7)));
      if (!(pgm_read_byte(&spr.mask[bi]) & bit)) continue;
      const bool ink = (pgm_read_byte(&spr.ink[bi]) & bit) != 0;
      // 白方回退：实心红、空心白
      if (white) {
        epd.setPixel(x0 + col, y0 + row, ink ? Theme::Alert : Theme::Bg);
      } else {
        epd.setPixel(x0 + col, y0 + row, ink ? Theme::Fg : Theme::Bg);
      }
    }
  }
}

bool chessShow(EpdSsd2683 &epd, int &index) {
  if (!epd.ok()) {
    errorLog("chess", "EPD not ready");
    return false;
  }

  const int pool = chessPositionCount();
  if (pool <= 0) {
    errorLog("chess", "no levels");
    return false;
  }
  if (index < 0) index = 0;
  index %= pool;

  const int raw = chessRawAt(index);
  if (raw < 0) {
    errorLog("chess", "bad index");
    return false;
  }
  const Endgame &eg = kEndgames[raw];
  Sq board[8][8];
  bool white_tm = true;
  if (!parseFenBoard(eg.fen, board, white_tm)) {
    errorLog("chess", "bad FEN");
    return false;
  }

  epd.clear(Theme::Bg);

  // 棋盘靠右，单格 34；四周留白同 board_y；左侧竖排文案
  constexpr int kScreenW = EpdSsd2683::kWidth;
  constexpr int kScreenH = EpdSsd2683::kHeight;
  constexpr int kBoardTextGap = 8;
  constexpr int cell = 34;
  constexpr int kColW = 16;   // 竖排列宽（CJK）
  constexpr int kColGap = 10; // 列间距
  const int board_px = cell * 8;
  const int board_y = (kScreenH - board_px) / 2;  // 上下留白
  const int margin = board_y;                     // 左/右同上下
  const int sq_x = kScreenW - board_px - margin;
  const int text_y0 = board_y;                    // 标题顶对齐棋盘
  const int text_y1 = board_y + board_px;         // 与棋盘底齐
  const int idx_y = text_y1 - fontLineHeight();

  // 左侧竖排：标题 | 先行+行棋方 | 要求+提示；底部署名序号
  {
    int cx = margin;
    // 标题
    chessDrawVertical(epd, cx, text_y0, idx_y - 4, eg.name, Theme::Fg);
    cx += kColW + kColGap;

    // 先行 / 白方·黑方
    int y = chessDrawVertical(epd, cx, text_y0, idx_y - 4, "先行", Theme::Alert);
    chessDrawVertical(epd, cx, y + 4, idx_y - 4, white_tm ? "白方" : "黑方", Theme::Fg);
    cx += kColW + kColGap;

    // 要求 / goal
    if (eg.goal && eg.goal[0]) {
      y = chessDrawVertical(epd, cx, text_y0, idx_y - 4, "要求", Theme::Alert);
      chessDrawVertical(epd, cx, y + 4, idx_y - 4, eg.goal, Theme::Fg);
    }

    char idxbuf[12];
    snprintf(idxbuf, sizeof(idxbuf), "%d/%d", index + 1, pool);
    const int text_w = sq_x - kBoardTextGap - margin;
    const int idx_w = (int)strlen(idxbuf) * 8;
    int ix = margin + (text_w - idx_w) / 2;
    if (ix < margin) ix = margin;
    fontDrawTextWrapped(epd, ix, idx_y, text_w, kScreenH, idxbuf, Theme::Alert);
  }

  // 棋盘：白 / 黄格；黑子实心黑，白子纯红（无描边）
  for (int r = 0; r < 8; r++) {
    for (int f = 0; f < 8; f++) {
      const int x = sq_x + f * cell;
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
  epd.drawRect(sq_x - 1, board_y - 1, board_px + 2, board_px + 2, Theme::Fg);

  epd.display();
  return true;
}
