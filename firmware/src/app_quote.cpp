#include "app_quote.h"
#include "app_config.h"
#include "app_apps.h"
#include "app_font.h"
#include "app_theme.h"
#include "app_rtc.h"

#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <esp_system.h>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <new>

static constexpr uint32_t kRefreshMinSec = 60;
static constexpr int kQuoteMarginPref = 40;  // 优先留白
static constexpr int kQuoteMarginMin = 20;   // 最小留白
static constexpr int kAuthorDashW = 32;      // 作者前横线：2 字宽（16×2）
static constexpr int kAuthorDashGap = 10;    // 横线与作者名间距
static constexpr int kQuoteLineGap2x = 12;   // 名言 2× 额外行距
static constexpr int kQuoteLineGap1x = 6;    // 名言 1× 额外行距
static constexpr int kPoemLineGap2x = 14;     // 诗句 2× 行距
static constexpr int kPoemLineGap1x = 8;      // 诗句 1× 行距

// QuoteItem[64] 约数 KB，不可叠在 loopTask 栈上（曾触发 stack canary）
static QuoteItem *quoteAllocItems() {
  QuoteItem *p = new (std::nothrow) QuoteItem[QUOTE_ITEM_MAX];
  if (!p) Serial.println("[Quote] alloc items failed");
  return p;
}

static void quoteFreeItems(QuoteItem *p) { delete[] p; }

// 已确认种子达标后跳过重复 load；列表被清空后会再 seed
static int g_quote_seed_ok = -1;

// 1× 字宽估算：ASCII 8 / CJK 16；scale=2 时乘 2
static int quoteTextWidth(const char *utf8, int scale = 1) {
  if (!utf8) return 0;
  if (scale < 1) scale = 1;
  int w = 0;
  const uint8_t *p = (const uint8_t *)utf8;
  while (*p) {
    if (*p < 0x80) {
      w += 8 * scale;
      p++;
    } else if (*p < 0xE0) {
      w += 16 * scale;
      p += 2;
    } else if (*p < 0xF0) {
      w += 16 * scale;
      p += 3;
    } else {
      w += 16 * scale;
      p += 4;
    }
  }
  return w;
}

// 推进一个 UTF-8 码点，返回 1× 字宽
static int quoteUtf8Advance(const char *&p) {
  if (!p || !*p) return 0;
  const uint8_t c0 = (uint8_t)*p;
  if (c0 < 0x80) {
    p++;
    return 8;
  }
  if (c0 < 0xE0) {
    p += 2;
    return 16;
  }
  if (c0 < 0xF0) {
    p += 3;
    return 16;
  }
  p += 4;
  return 16;
}

static bool quoteIsAsciiWordChar(unsigned char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '\'' ||
         c == '-';
}

// 不应落在行首的 ASCII 标点（跟前一词）
static bool quoteIsAsciiTrailPunct(unsigned char c) {
  switch (c) {
    case ',':
    case '.':
    case ';':
    case ':':
    case '!':
    case '?':
    case ')':
    case ']':
    case '}':
    case '%':
    case '\'':
    case '"':
      return true;
    default:
      return false;
  }
}

// 中文行首禁则标点
static bool quoteIsCjkTrailPunct(uint32_t cp) {
  switch (cp) {
    case 0x3001:  // 、
    case 0x3002:  // 。
    case 0xFF0C:  // ，
    case 0xFF0E:  // ．
    case 0xFF01:  // ！
    case 0xFF1F:  // ？
    case 0xFF1B:  // ；
    case 0xFF1A:  // ：
    case 0x300B:  // 》
    case 0x300D:  // 」
    case 0x300F:  // 』
    case 0xFF09:  // ）
    case 0xFF3D:  // ］
    case 0xFF5D:  // ｝
      return true;
    default:
      return false;
  }
}

static uint32_t quoteUtf8Peek(const char *p, int &bytes) {
  bytes = 0;
  if (!p || !*p) return 0;
  const uint8_t c0 = (uint8_t)*p;
  if (c0 < 0x80) {
    bytes = 1;
    return c0;
  }
  if ((c0 & 0xE0) == 0xC0 && (uint8_t)p[1]) {
    bytes = 2;
    return ((uint32_t)(c0 & 0x1F) << 6) | ((uint8_t)p[1] & 0x3F);
  }
  if ((c0 & 0xF0) == 0xE0 && (uint8_t)p[1] && (uint8_t)p[2]) {
    bytes = 3;
    return ((uint32_t)(c0 & 0x0F) << 12) | ((uint32_t)((uint8_t)p[1] & 0x3F) << 6) |
           ((uint8_t)p[2] & 0x3F);
  }
  if ((c0 & 0xF8) == 0xF0 && (uint8_t)p[1] && (uint8_t)p[2] && (uint8_t)p[3]) {
    bytes = 4;
    return ((uint32_t)(c0 & 0x07) << 18) | ((uint32_t)((uint8_t)p[1] & 0x3F) << 12) |
           ((uint32_t)((uint8_t)p[2] & 0x3F) << 6) | ((uint8_t)p[3] & 0x3F);
  }
  bytes = 1;
  return c0;
}

// 取下一个折行单元（英文整词+尾随标点 / 单字+行首禁则标点），返回 1× 宽
static int quoteNextWrapUnit(const char *p, const char *&end) {
  end = p;
  if (!p || !*p || *p == '\n' || *p == '\r') return 0;

  const unsigned char c0 = (unsigned char)*p;
  // 空格单独成单元，行首可丢弃
  if (c0 == ' ' || c0 == '\t') {
    end = p + 1;
    return 8;
  }

  // 英文单词：字母数字串 + 尾随 , . 等
  if (c0 < 0x80 && quoteIsAsciiWordChar(c0)) {
    end = p;
    while (*end) {
      const unsigned char e = (unsigned char)*end;
      if (e >= 0x80 || !quoteIsAsciiWordChar(e)) break;
      end++;
    }
    while (*end) {
      const unsigned char e = (unsigned char)*end;
      if (e >= 0x80 || !quoteIsAsciiTrailPunct(e)) break;
      end++;
    }
    return (int)(end - p) * 8;
  }

  // 单码点（CJK / 其它 ASCII），并粘上行首禁则标点
  int bytes = 0;
  (void)quoteUtf8Peek(p, bytes);
  if (bytes <= 0) bytes = 1;
  end = p + bytes;
  int w = (c0 < 0x80) ? 8 : 16;
  for (;;) {
    int nb = 0;
    const uint32_t cp = quoteUtf8Peek(end, nb);
    if (nb <= 0) break;
    if (cp < 0x80) {
      if (!quoteIsAsciiTrailPunct((unsigned char)cp)) break;
      end += nb;
      w += 8;
    } else if (quoteIsCjkTrailPunct(cp)) {
      end += nb;
      w += 16;
    } else {
      break;
    }
  }
  return w;
}

// 取一行可显示内容：英文不切词、标点不落行首；超宽单词再逐字切
// 返回行宽（已 × scale）；line_begin/line_end 为正文区间；end 为下一行起点
static int quoteTakeWrappedLine(const char *start, int content_w, int scale, const char *&end,
                                const char *&line_begin, const char *&line_end) {
  end = start;
  line_begin = start;
  line_end = start;
  if (!start || !*start || content_w <= 0) return 0;
  if (scale < 1) scale = 1;

  // 行首空白丢掉（换行后不顶格空格）
  while (*end == ' ' || *end == '\t') end++;
  line_begin = end;
  line_end = end;
  if (!*end || *end == '\n' || *end == '\r') return 0;

  int line_w = 0;
  while (*end && *end != '\n' && *end != '\r') {
    const char *unit_end = end;
    int uw = quoteNextWrapUnit(end, unit_end) * scale;
    if (unit_end == end) {
      // 防御：无法前进则逐字节跳过
      end++;
      line_end = end;
      continue;
    }
    // 行内已有内容且本单元放不下 → 在此折行
    if (line_w > 0 && line_w + uw > content_w) break;

    // 单词本身超宽：逐字塞到行满
    if (line_w == 0 && uw > content_w) {
      const char *q = end;
      while (q < unit_end) {
        const char *before = q;
        const int gw = quoteUtf8Advance(q) * scale;
        if (line_w > 0 && line_w + gw > content_w) {
          end = before;
          line_end = before;
          return line_w;
        }
        line_w += gw;
        end = q;
        line_end = q;
      }
      return line_w;
    }

    line_w += uw;
    end = unit_end;
    line_end = unit_end;
  }
  // 行尾空白不计入宽度，也不画出来
  while (line_end > line_begin && (line_end[-1] == ' ' || line_end[-1] == '\t')) {
    line_w -= 8 * scale;
    line_end--;
  }
  if (line_w < 0) line_w = 0;
  return line_w;
}

// 统计换行后占用行数（含自动折行；英文按词）
static int quoteCountWrappedLines(const char *text, int content_w, int scale) {
  if (!text || !text[0] || content_w <= 0) return 0;
  if (scale < 1) scale = 1;
  int lines = 0;
  const char *p = text;
  while (*p) {
    while (*p == '\r') p++;
    if (*p == '\n') {
      p++;
      continue;
    }
    while (*p && *p != '\n' && *p != '\r') {
      const char *line_begin = p;
      const char *line_end = p;
      const char *next = p;
      const int lw = quoteTakeWrappedLine(p, content_w, scale, next, line_begin, line_end);
      if (lw <= 0) {
        // 仅空白：跳过到换行或结束
        p = next;
        while (*p == ' ' || *p == '\t') p++;
        break;
      }
      lines++;
      p = next;
    }
    if (*p == '\r') p++;
    if (*p == '\n') p++;
  }
  return lines;
}

static int quoteProseHeight(const char *text, int content_w, int scale, int line_gap) {
  const int line_h = 16 * scale;
  const int n = quoteCountWrappedLines(text, content_w, scale);
  if (n <= 0) return 0;
  return n * line_h + (n - 1) * line_gap;
}

static int quotePoemHeight(const char *text, int content_w, int scale, int line_gap) {
  // 与绘制一致：每句可再折行，句间用 line_gap，句内续折用较小间距
  if (!text || !text[0]) return 0;
  const int line_h = 16 * scale;
  const int wrap_gap = (scale >= 2) ? 4 : 2;
  int h = 0;
  int poem_lines = 0;
  const char *p = text;
  while (*p) {
    while (*p == '\r') p++;
    if (*p == '\n') {
      p++;
      continue;
    }
    const char *start = p;
    while (*p && *p != '\n' && *p != '\r') p++;
    String line;
    for (const char *q = start; q < p; q++) line += *q;
    line.trim();
    if (*p == '\r') p++;
    if (*p == '\n') p++;
    if (line.isEmpty()) continue;

    const int wraps = quoteCountWrappedLines(line.c_str(), content_w, scale);
    if (wraps <= 0) continue;
    if (poem_lines > 0) h += line_gap;
    h += wraps * line_h + (wraps - 1) * wrap_gap;
    poem_lines++;
  }
  return h;
}

static bool ensureQuoteDir() {
  if (LittleFS.exists("/quote")) return true;
  return LittleFS.mkdir("/quote");
}

static void clampQuoteFields(QuoteItem &it) {
  it.text.trim();
  it.author.trim();
  if (it.text.length() > QUOTE_TEXT_MAX) it.text.remove(QUOTE_TEXT_MAX);
  if (it.author.length() > QUOTE_AUTHOR_MAX) it.author.remove(QUOTE_AUTHOR_MAX);
}

bool quoteLoad(QuoteItem *out_items, int max_n, int &out_n, int &out_index, int *out_seed_ver) {
  out_n = 0;
  out_index = 0;
  if (out_seed_ver) *out_seed_ver = 0;
  if (!out_items || max_n <= 0) return false;
  if (!LittleFS.exists(QUOTE_PATH)) return true;
  File f = LittleFS.open(QUOTE_PATH, "r");
  if (!f) return false;
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;
  out_index = doc["idx"] | 0;
  if (out_seed_ver) *out_seed_ver = doc["seed"] | 0;
  if (!doc["items"].is<JsonArray>()) return true;
  for (JsonVariant v : doc["items"].as<JsonArray>()) {
    if (out_n >= max_n) break;
    QuoteItem it;
    if (v.is<const char *>()) {
      // 兼容旧版纯字符串条目
      const char *s = v.as<const char *>();
      if (!s || !s[0]) continue;
      it.text = String(s);
    } else if (v.is<JsonObject>()) {
      JsonObject o = v.as<JsonObject>();
      const char *t = o["t"] | o["text"] | "";
      const char *a = o["a"] | o["author"] | "";
      if (!t || !t[0]) continue;
      it.text = String(t);
      it.author = String(a);
    } else {
      continue;
    }
    clampQuoteFields(it);
    if (it.text.isEmpty()) continue;
    out_items[out_n++] = it;
  }
  if (out_n <= 0) {
    out_index = 0;
  } else if (out_index < 0 || out_index >= out_n) {
    out_index = 0;
  }
  return true;
}

bool quoteSave(const QuoteItem *items, int n, int index, int seed_ver) {
  if (!ensureQuoteDir()) return false;
  if (n < 0) n = 0;
  if (n > QUOTE_ITEM_MAX) n = QUOTE_ITEM_MAX;
  if (n <= 0) index = 0;
  else if (index < 0 || index >= n) index = 0;
  if (seed_ver < 0) seed_ver = 0;

  JsonDocument doc;
  doc["idx"] = index;
  doc["seed"] = seed_ver;
  JsonArray arr = doc["items"].to<JsonArray>();
  for (int i = 0; i < n; i++) {
    QuoteItem it = items[i];
    clampQuoteFields(it);
    if (it.text.isEmpty()) continue;
    JsonObject o = arr.add<JsonObject>();
    o["t"] = it.text;
    o["a"] = it.author;
  }
  File f = LittleFS.open(QUOTE_PATH, "w");
  if (!f) return false;
  const bool ok = serializeJson(doc, f) > 0;
  f.close();
  if (ok) {
    // 空列表下次仍需 ensureDefaults 回填内置诗词
    g_quote_seed_ok = (n > 0) ? seed_ver : -1;
  }
  return ok;
}

struct QuoteDefaultEntry {
  const char *t;
  const char *a;
};

// 内置名言 + 诗词（诗词正文用 \n 分句；偏经典、忌过浅启蒙篇）
static const QuoteDefaultEntry kQuoteDefaults[] = {
    // —— 名言 ——
    {"天行健，君子以自强不息。", "《周易》"},
    {"路漫漫其修远兮，吾将上下而求索。", "屈原"},
    {"知之者不如好之者，好之者不如乐之者。", "孔子"},
    {"三人行，必有我师焉。", "孔子"},
    {"勿以恶小而为之，勿以善小而不为。", "刘备"},
    {"纸上得来终觉浅，绝知此事要躬行。", "陆游"},
    {"千磨万击还坚劲，任尔东西南北风。", "郑燮"},
    {"海内存知己，天涯若比邻。", "王勃"},
    {"人生自古谁无死，留取丹心照汗青。", "文天祥"},
    {"Stay hungry, stay foolish.", "Steve Jobs"},
    {"The only way to do great work is to love what you do.", "Steve Jobs"},
    // —— 唐诗 ——
    {"朝辞白帝彩云间\n千里江陵一日还\n两岸猿声啼不住\n轻舟已过万重山", "李白《早发白帝城》"},
    {"故人西辞黄鹤楼\n烟花三月下扬州\n孤帆远影碧空尽\n唯见长江天际流", "李白《黄鹤楼送孟浩然之广陵》"},
    {"日照香炉生紫烟\n遥看瀑布挂前川\n飞流直下三千尺\n疑是银河落九天", "李白《望庐山瀑布》"},
    {"青山横北郭\n白水绕东城\n此地一为别\n孤蓬万里征\n浮云游子意\n落日故人情\n挥手自兹去\n萧萧班马鸣",
     "李白《送友人》"},
    {"国破山河在\n城春草木深\n感时花溅泪\n恨别鸟惊心\n烽火连三月\n家书抵万金\n白头搔更短\n浑欲不胜簪",
     "杜甫《春望》"},
    {"风急天高猿啸哀\n渚清沙白鸟飞回\n无边落木萧萧下\n不尽长江滚滚来\n万里悲秋常作客\n百年多病独登台\n艰难苦恨繁霜鬓\n潦倒新停浊酒杯",
     "杜甫《登高》"},
    {"两个黄鹂鸣翠柳\n一行白鹭上青天\n窗含西岭千秋雪\n门泊东吴万里船", "杜甫《绝句》"},
    {"岐王宅里寻常见\n崔九堂前几度闻\n正是江南好风景\n落花时节又逢君", "杜甫《江南逢李龟年》"},
    {"空山不见人\n但闻人语响\n返景入深林\n复照青苔上", "王维《鹿柴》"},
    {"独坐幽篁里\n弹琴复长啸\n深林人不知\n明月来相照", "王维《竹里馆》"},
    {"人闲桂花落\n夜静春山空\n月出惊山鸟\n时鸣春涧中", "王维《鸟鸣涧》"},
    {"空山新雨后\n天气晚来秋\n明月松间照\n清泉石上流\n竹喧归浣女\n莲动下渔舟\n随意春芳歇\n王孙自可留",
     "王维《山居秋暝》"},
    {"千山鸟飞绝\n万径人踪灭\n孤舟蓑笠翁\n独钓寒江雪", "柳宗元《江雪》"},
    {"移舟泊烟渚\n日暮客愁新\n野旷天低树\n江清月近人", "孟浩然《宿建德江》"},
    {"秦时明月汉时关\n万里长征人未还\n但使龙城飞将在\n不教胡马度阴山", "王昌龄《出塞》"},
    {"寒雨连江夜入吴\n平明送客楚山孤\n洛阳亲友如相问\n一片冰心在玉壶", "王昌龄《芙蓉楼送辛渐》"},
    {"黄河远上白云间\n一片孤城万仞山\n羌笛何须怨杨柳\n春风不度玉门关", "王之涣《凉州词》"},
    {"月落乌啼霜满天\n江枫渔火对愁眠\n姑苏城外寒山寺\n夜半钟声到客船", "张继《枫桥夜泊》"},
    {"昔人已乘黄鹤去\n此地空余黄鹤楼\n黄鹤一去不复返\n白云千载空悠悠\n晴川历历汉阳树\n芳草萋萋鹦鹉洲\n日暮乡关何处是\n烟波江上使人愁",
     "崔颢《黄鹤楼》"},
    {"朱雀桥边野草花\n乌衣巷口夕阳斜\n旧时王谢堂前燕\n飞入寻常百姓家", "刘禹锡《乌衣巷》"},
    {"自古逢秋悲寂寥\n我言秋日胜春朝\n晴空一鹤排云上\n便引诗情到碧霄", "刘禹锡《秋词》"},
    {"烟笼寒水月笼沙\n夜泊秦淮近酒家\n商女不知亡国恨\n隔江犹唱后庭花", "杜牧《泊秦淮》"},
    {"远上寒山石径斜\n白云生处有人家\n停车坐爱枫林晚\n霜叶红于二月花", "杜牧《山行》"},
    {"君问归期未有期\n巴山夜雨涨秋池\n何当共剪西窗烛\n却话巴山夜雨时", "李商隐《夜雨寄北》"},
    {"向晚意不适\n驱车登古原\n夕阳无限好\n只是近黄昏", "李商隐《登乐游原》"},
    {"相见时难别亦难\n东风无力百花残\n春蚕到死丝方尽\n蜡炬成灰泪始干", "李商隐《无题》"},
    // —— 宋诗 / 词 ——
    {"水光潋滟晴方好\n山色空蒙雨亦奇\n欲把西湖比西子\n淡妆浓抹总相宜", "苏轼《饮湖上初晴后雨》"},
    {"横看成岭侧成峰\n远近高低各不同\n不识庐山真面目\n只缘身在此山中", "苏轼《题西林壁》"},
    {"莫听穿林打叶声\n何妨吟啸且徐行\n竹杖芒鞋轻胜马\n谁怕\n一蓑烟雨任平生\n料峭春风吹酒醒\n微冷\n山头斜照却相迎\n回首向来萧瑟处\n归去\n也无风雨也无晴",
     "苏轼《定风波》"},
    {"人生到处知何似\n应似飞鸿踏雪泥\n泥上偶然留指爪\n鸿飞那复计东西", "苏轼《和子由渑池怀旧》"},
    {"京口瓜洲一水间\n钟山只隔数重山\n春风又绿江南岸\n明月何时照我还", "王安石《泊船瓜洲》"},
    {"墙角数枝梅\n凌寒独自开\n遥知不是雪\n为有暗香来", "王安石《梅花》"},
    {"死去元知万事空\n但悲不见九州同\n王师北定中原日\n家祭无忘告乃翁", "陆游《示儿》"},
    {"莫笑农家腊酒浑\n丰年留客足鸡豚\n山重水复疑无路\n柳暗花明又一村", "陆游《游山西村》"},
    {"生当作人杰\n死亦为鬼雄\n至今思项羽\n不肯过江东", "李清照《夏日绝句》"},
    {"常记溪亭日暮\n沉醉不知归路\n兴尽晚回舟\n误入藕花深处\n争渡\n争渡\n惊起一滩鸥鹭", "李清照《如梦令》"},
    {"寻寻觅觅\n冷冷清清\n凄凄惨惨戚戚\n乍暖还寒时候\n最难将息", "李清照《声声慢》"},
    {"东风夜放花千树\n更吹落\n星如雨\n宝马雕车香满路\n凤箫声动\n玉壶光转\n一夜鱼龙舞\n蛾儿雪柳黄金缕\n笑语盈盈暗香去\n众里寻他千百度\n蓦然回首\n那人却在\n灯火阑珊处",
     "辛弃疾《青玉案·元夕》"},
    {"醉里挑灯看剑\n梦回吹角连营\n八百里分麾下炙\n五十弦翻塞外声\n沙场秋点兵\n马作的卢飞快\n弓如霹雳弦惊\n了却君王天下事\n赢得生前身后名\n可怜白发生",
     "辛弃疾《破阵子》"},
    {"人生若只如初见\n何事秋风悲画扇\n等闲变却故人心\n却道故人心易变", "纳兰性德《木兰花》"},
};

static constexpr int kQuoteDefaultsN = (int)(sizeof(kQuoteDefaults) / sizeof(kQuoteDefaults[0]));

// 旧版过浅篇目：种子升级时从列表剔除（精确匹配正文）
static bool quoteIsRetiredDefault(const String &text) {
  static const char *kRetired[] = {
      "床前明月光\n疑是地上霜\n举头望明月\n低头思故乡",
      "白日依山尽\n黄河入海流\n欲穷千里目\n更上一层楼",
      "春眠不觉晓\n处处闻啼鸟\n夜来风雨声\n花落知多少",
      "锄禾日当午\n汗滴禾下土\n谁知盘中餐\n粒粒皆辛苦",
      "宝剑锋从磨砺出，梅花香自苦寒来。",
  };
  for (size_t i = 0; i < sizeof(kRetired) / sizeof(kRetired[0]); i++) {
    if (text == kRetired[i]) return true;
  }
  return false;
}

static bool quoteTextExists(const QuoteItem *items, int n, const char *text) {
  if (!text) return false;
  for (int i = 0; i < n; i++) {
    if (items[i].text == text) return true;
  }
  return false;
}

static int quoteFillFromDefaults(QuoteItem *items, int max_n) {
  int out = 0;
  for (int i = 0; i < kQuoteDefaultsN && out < max_n; i++) {
    items[out].text = String(kQuoteDefaults[i].t);
    items[out].author = String(kQuoteDefaults[i].a);
    clampQuoteFields(items[out]);
    if (!items[out].text.isEmpty()) out++;
  }
  return out;
}

// 内置名言 + 诗词；空列表写入，旧种子合并升级
void quoteEnsureDefaults() {
  if (g_quote_seed_ok >= QUOTE_SEED_VER) return;

  QuoteItem *cur = quoteAllocItems();
  if (!cur) return;
  int n = 0, idx = 0, seed = 0;
  if (!quoteLoad(cur, QUOTE_ITEM_MAX, n, idx, &seed)) {
    quoteFreeItems(cur);
    return;
  }

  if (n <= 0) {
    const int out = quoteFillFromDefaults(cur, QUOTE_ITEM_MAX);
    if (out > 0) {
      quoteSave(cur, out, 0, QUOTE_SEED_VER);
      Serial.printf("[Quote] seeded %d defaults (v%d)\n", out, QUOTE_SEED_VER);
    }
    quoteFreeItems(cur);
    return;
  }

  if (seed >= QUOTE_SEED_VER) {
    g_quote_seed_ok = seed;
    quoteFreeItems(cur);
    return;
  }

  // 升级：去掉过浅旧篇，补上缺失的新内置条（第二块缓冲也走堆）
  QuoteItem *next = quoteAllocItems();
  if (!next) {
    quoteFreeItems(cur);
    return;
  }
  int out = 0;
  for (int i = 0; i < n && out < QUOTE_ITEM_MAX; i++) {
    if (quoteIsRetiredDefault(cur[i].text)) continue;
    next[out++] = cur[i];
  }
  int added = 0;
  for (int i = 0; i < kQuoteDefaultsN && out < QUOTE_ITEM_MAX; i++) {
    if (quoteTextExists(next, out, kQuoteDefaults[i].t)) continue;
    next[out].text = String(kQuoteDefaults[i].t);
    next[out].author = String(kQuoteDefaults[i].a);
    clampQuoteFields(next[out]);
    if (next[out].text.isEmpty()) continue;
    out++;
    added++;
  }
  if (idx >= out) idx = 0;
  quoteSave(next, out, idx, QUOTE_SEED_VER);
  Serial.printf("[Quote] upgraded seed %d→%d, items=%d (+%d)\n", seed, QUOTE_SEED_VER, out, added);
  quoteFreeItems(next);
  quoteFreeItems(cur);
}

bool quoteResetToDefaults() {
  QuoteItem *items = quoteAllocItems();
  if (!items) return false;
  const int out = quoteFillFromDefaults(items, QUOTE_ITEM_MAX);
  const bool ok = (out > 0) && quoteSave(items, out, 0, QUOTE_SEED_VER);
  quoteFreeItems(items);
  return ok;
}

int quoteCount() {
  quoteEnsureDefaults();
  QuoteItem *items = quoteAllocItems();
  if (!items) return 0;
  int n = 0, idx = 0;
  if (!quoteLoad(items, QUOTE_ITEM_MAX, n, idx)) n = 0;
  quoteFreeItems(items);
  return n;
}

void quoteAdvanceIndex(int &index, int steps) {
  quoteEnsureDefaults();
  QuoteItem *items = quoteAllocItems();
  if (!items) {
    index = 0;
    return;
  }
  int n = 0, stored = 0;
  if (!quoteLoad(items, QUOTE_ITEM_MAX, n, stored) || n <= 0) {
    index = 0;
    quoteFreeItems(items);
    return;
  }
  if (index < 0) index = stored;
  index %= n;
  if (getAppConfig().quote_random) {
    if (n == 1) {
      index = 0;
    } else {
      int next = index;
      for (int tries = 0; tries < 8 && next == index; tries++) {
        next = (int)(esp_random() % (uint32_t)n);
      }
      if (next == index) next = (index + 1) % n;
      index = next;
    }
  } else {
    if (steps < 0) steps = 0;
    index = (index + steps) % n;
  }
  quoteSave(items, n, index);
  quoteFreeItems(items);
}

// 正文含换行 → 按诗词处理
static bool quoteIsPoem(const String &text) { return text.indexOf('\n') >= 0; }

// 诗词：逐行居中；scale=1/2；单行过宽则行内再折
static void quoteDrawPoem(EpdSsd2683 &epd, const char *text, int margin, int content_w, int top_y,
                          int max_y, int scale) {
  if (scale < 1) scale = 1;
  const int line_h = 16 * scale;
  const int line_gap = (scale >= 2) ? kPoemLineGap2x : kPoemLineGap1x;
  const int wrap_gap = (scale >= 2) ? 4 : 2;
  const int step = line_h + line_gap;

  const int block_h = quotePoemHeight(text, content_w, scale, line_gap);
  int y = top_y;
  if (block_h > 0 && block_h < (max_y - top_y)) {
    y = top_y + ((max_y - top_y) - block_h) / 2;
  }

  const char *p = text;
  while (*p && y + line_h <= max_y) {
    while (*p == '\r') p++;
    if (*p == '\n') {
      p++;
      continue;
    }
    const char *start = p;
    while (*p && *p != '\n' && *p != '\r') p++;
    String line;
    line.reserve((size_t)(p - start) + 1);
    for (const char *q = start; q < p; q++) line += *q;
    line.trim();
    if (*p == '\r') p++;
    if (*p == '\n') p++;
    if (line.isEmpty()) continue;

    const char *s = line.c_str();
    while (*s && y + line_h <= max_y) {
      const char *line_begin = s;
      const char *line_end = s;
      const char *next = s;
      const int chunk_w = quoteTakeWrappedLine(s, content_w, scale, next, line_begin, line_end);
      if (chunk_w <= 0) {
        s = next;
        while (*s == ' ' || *s == '\t') s++;
        break;
      }
      String part;
      part.reserve((size_t)(line_end - line_begin) + 1);
      for (const char *q = line_begin; q < line_end; q++) part += *q;
      s = next;

      int x = margin + (content_w - chunk_w) / 2;
      if (x < margin) x = margin;
      if (scale >= 2) {
        fontDrawTextWrappedDouble(epd, x, y, content_w, y + line_h, part.c_str(), Theme::Fg);
      } else {
        fontDrawTextWrapped(epd, x, y, content_w, y + line_h, part.c_str(), Theme::Fg);
      }
      y += (*s) ? (line_h + wrap_gap) : step;
    }
  }
}

// 名言正文：左起折行（英文整词、标点不落行首）
static void quoteDrawProse(EpdSsd2683 &epd, const char *text, int margin, int content_w, int top_y,
                           int max_y, int scale) {
  if (scale < 1) scale = 1;
  const int line_h = 16 * scale;
  const int line_gap = (scale >= 2) ? kQuoteLineGap2x : kQuoteLineGap1x;
  const int step = line_h + line_gap;
  const char *p = text;
  int y = top_y;
  while (*p && y + line_h <= max_y) {
    while (*p == '\r') p++;
    if (*p == '\n') {
      p++;
      y += step;
      continue;
    }
    const char *line_begin = p;
    const char *line_end = p;
    const char *next = p;
    const int chunk_w = quoteTakeWrappedLine(p, content_w, scale, next, line_begin, line_end);
    if (chunk_w <= 0) {
      p = next;
      while (*p == ' ' || *p == '\t') p++;
      if (*p == '\r') p++;
      if (*p == '\n') p++;
      continue;
    }
    String part;
    part.reserve((size_t)(line_end - line_begin) + 1);
    for (const char *q = line_begin; q < line_end; q++) part += *q;
    p = next;
    if (scale >= 2) {
      fontDrawTextWrappedDouble(epd, margin, y, content_w, y + line_h, part.c_str(), Theme::Fg);
    } else {
      fontDrawTextWrapped(epd, margin, y, content_w, y + line_h, part.c_str(), Theme::Fg);
    }
    if (*p == '\r') p++;
    if (*p == '\n') p++;
    y += step;
  }
}

// 选布局：优先大字+大边距 → 大字+小边距 → 小字+大边距 → 小字+小边距
static void quotePickLayout(const char *text, bool poem, bool has_author, int sw, int sh, int &out_margin,
                            int &out_scale) {
  out_margin = kQuoteMarginPref;
  out_scale = 2;
  const int author_h = fontLineHeight();
  const int author_reserve = has_author ? (author_h + 12) : 0;

  const int margins[2] = {kQuoteMarginPref, kQuoteMarginMin};
  const int scales[2] = {2, 1};
  for (int si = 0; si < 2; si++) {
    for (int mi = 0; mi < 2; mi++) {
      const int margin = margins[mi];
      const int scale = scales[si];
      const int content_w = sw - margin * 2;
      const int body_h = sh - margin * 2 - author_reserve;
      if (content_w <= 0 || body_h <= 0) continue;
      const int line_gap =
          poem ? ((scale >= 2) ? kPoemLineGap2x : kPoemLineGap1x)
               : ((scale >= 2) ? kQuoteLineGap2x : kQuoteLineGap1x);
      const int need =
          poem ? quotePoemHeight(text, content_w, scale, line_gap)
               : quoteProseHeight(text, content_w, scale, line_gap);
      if (need > 0 && need <= body_h) {
        out_margin = margin;
        out_scale = scale;
        return;
      }
    }
  }
  // 都放不下：用最小边距 + 小字硬画（可能裁切）
  out_margin = kQuoteMarginMin;
  out_scale = 1;
}

bool quoteShow(EpdSsd2683 &epd, int &index) {
  if (!epd.ok()) return false;
  quoteEnsureDefaults();
  QuoteItem *items = quoteAllocItems();
  if (!items) return false;
  int n = 0, stored = 0;
  quoteLoad(items, QUOTE_ITEM_MAX, n, stored);

  const int sw = EpdSsd2683::kWidth;
  const int sh = EpdSsd2683::kHeight;

  epd.clear(Theme::Bg);

  if (n <= 0) {
    fontDrawTextWrapped(epd, kQuoteMarginMin, kQuoteMarginMin, sw - kQuoteMarginMin * 2,
                        sh - kQuoteMarginMin, "还没有名言。请在配置网页 /quote 添加。", Theme::Fg);
    epd.display();
    quoteFreeItems(items);
    return true;
  }
  if (index < 0 || index >= n) index = stored % n;

  // 拷出当前条，再释放整表，降低后续排版期间的峰值内存
  const String text = items[index].text;
  const String author = items[index].author;
  quoteFreeItems(items);

  const bool has_author = author.length() > 0;
  const bool poem = quoteIsPoem(text);

  int margin = kQuoteMarginPref;
  int scale = 2;
  quotePickLayout(text.c_str(), poem, has_author, sw, sh, margin, scale);

  const int content_w = sw - margin * 2;
  const int author_h = fontLineHeight();
  const int body_max_y = has_author ? (sh - margin - author_h - 12) : (sh - margin);

  if (poem) {
    quoteDrawPoem(epd, text.c_str(), margin, content_w, margin, body_max_y, scale);
  } else {
    quoteDrawProse(epd, text.c_str(), margin, content_w, margin, body_max_y, scale);
  }

  // 右下角：2 字宽小横线 + 10px + 作者名（边距不低于当前 layout）
  if (has_author) {
    const int aw = quoteTextWidth(author.c_str(), 1);
    int author_x = sw - margin - aw;
    const int author_y = sh - margin - author_h;
    int dash_x = author_x - kAuthorDashGap - kAuthorDashW;
    if (dash_x < margin) {
      dash_x = margin;
      author_x = dash_x + kAuthorDashW + kAuthorDashGap;
    }
    epd.drawHLine(dash_x, author_y + author_h / 2, kAuthorDashW, Theme::Fg);
    fontDrawTextWrapped(epd, author_x, author_y, aw + 2, author_y + author_h, author.c_str(),
                        Theme::Fg);
  }

  epd.display();

  const time_t now = time(nullptr);
  if (now >= 1700000000) saveQuoteLastUnix((uint32_t)now);
  return true;
}

bool quoteAutoRefreshDue() {
  const AppConfig &cfg = getAppConfig();
  if (!cfg.quote_refresh_en) return false;
  const uint32_t iv = cfg.quote_refresh_sec;
  if (iv < kRefreshMinSec) return false;
  if (!appIdEnabled(AppId::Quote)) return false;
  const time_t now = time(nullptr);
  if (now < 1700000000) return false;
  const uint32_t last = loadQuoteLastUnix();
  if (last == 0) return false;
  return (uint32_t)now >= last + iv;
}
