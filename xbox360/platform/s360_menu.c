// The options menu, opened with the right stick click. It lists the platform's settings, then the game's, then
// "Back to game" and "Quit"; D-pad up/down moves, left/right or A changes, B / Start / right stick closes.
// It is drawn over the game's frame in the game's own resolution (8x8 text, so 256 pixels hold 32 letters).
#include <stdio.h>
#include <string.h>
#include "s360_internal.h"
#include "xsdk/kernel.h"

enum { kRepeatDelay = 18, kRepeatRate = 5, kRowH = 10, kMaxItems = 64 };
enum { kItemSetting, kItemBack, kItemQuit };
typedef struct { int kind; S360Setting *setting; } Item;

static Item g_items[kMaxItems];
static int g_num_items, g_cursor, g_scroll, g_repeat;
static const S360Game *g_game;
static bool g_open, g_changed;

void s360_menu_init(const S360Game *game, S360Setting *platform, int num_platform) {
  g_game = game;
  g_num_items = 0;
  for (int i = 0; i < num_platform && g_num_items < kMaxItems - 2; i++)
    g_items[g_num_items++] = (Item){ kItemSetting, &platform[i] };
  for (int i = 0; i < game->num_settings && g_num_items < kMaxItems - 2; i++)
    g_items[g_num_items++] = (Item){ kItemSetting, &game->settings[i] };
  g_items[g_num_items++] = (Item){ kItemBack, NULL };
  g_items[g_num_items++] = (Item){ kItemQuit, NULL };
}

static int NumChoices(const S360Setting *s) {
  int n = 0;
  while (s->choices[n]) n++;
  return n;
}

// D-pad with auto-repeat while held.
static bool Repeat(const S360Pad *pad, uint16_t bit) {
  if (pad->pressed & bit) { g_repeat = kRepeatDelay; return true; }
  if (pad->held & bit) {
    if (--g_repeat <= 0) { g_repeat = kRepeatRate; return true; }
  }
  return false;
}

S360MenuResult s360_menu_update(const S360Pad *pad) {
  if (!g_open) {
    if (pad->pressed & INPUT_GAMEPAD_RIGHT_THUMB) {
      g_open = true;
      g_changed = false;
      return kS360Menu_Open;
    }
    return kS360Menu_Closed;
  }
  if (pad->pressed & (INPUT_GAMEPAD_RIGHT_THUMB | INPUT_GAMEPAD_B | INPUT_GAMEPAD_START)) {
    g_open = false;
    return kS360Menu_Closed;
  }
  if (Repeat(pad, INPUT_GAMEPAD_DPAD_UP)) g_cursor = (g_cursor + g_num_items - 1) % g_num_items;
  if (Repeat(pad, INPUT_GAMEPAD_DPAD_DOWN)) g_cursor = (g_cursor + 1) % g_num_items;
  Item *it = &g_items[g_cursor];
  int dir = 0;
  if (Repeat(pad, INPUT_GAMEPAD_DPAD_LEFT)) dir = -1;
  if (Repeat(pad, INPUT_GAMEPAD_DPAD_RIGHT)) dir = 1;
  if (pad->pressed & INPUT_GAMEPAD_A) dir = 1;
  if (it->kind == kItemSetting && dir) {
    int n = NumChoices(it->setting);
    it->setting->value = (it->setting->value + dir + n) % n;
    g_changed = true;
    return kS360Menu_Changed;
  }
  if (pad->pressed & INPUT_GAMEPAD_A) {
    if (it->kind == kItemBack) { g_open = false; return kS360Menu_Closed; }
    if (it->kind == kItemQuit) { g_open = false; return kS360Menu_Quit; }
  }
  return kS360Menu_Open;
}

bool s360_menu_is_open(void) { return g_open; }

void s360_menu_draw(uint32_t *px, int pitch, int w, int h) {
  const uint32_t kText = 0xE8E8E8, kValue = 0xF8D040, kCursor = 0x3060C0, kTitle = 0xF8F8F8, kHint = 0x98A8B8;
  const int scale = h >= 448 ? h / 224 : 1;  // the 4x world map: draw at the same size on screen
  s360_draw_set_scale(scale);
  const int clip_w = w;
  w /= scale, h /= scale;
  s360_draw_dim(px, pitch, 0, 0, w, h);
  int panel_w = w < 256 ? w : 256, x0 = (w - panel_w) / 2;
  int rows = (h - 40) / kRowH;
  if (g_cursor < g_scroll) g_scroll = g_cursor;
  if (g_cursor >= g_scroll + rows) g_scroll = g_cursor - rows + 1;
  s360_draw_text(px, pitch, clip_w, x0 + (panel_w - s360_text_width(g_game->title)) / 2, 6, g_game->title, kTitle);
  for (int r = 0; r < rows && g_scroll + r < g_num_items; r++) {
    int i = g_scroll + r, y = 22 + r * kRowH;
    Item *it = &g_items[i];
    if (i == g_cursor) s360_draw_rect(px, pitch, x0 + 2, y - 1, panel_w - 4, kRowH, kCursor);
    if (it->kind == kItemSetting) {
      S360Setting *s = it->setting;
      s360_draw_text(px, pitch, clip_w, x0 + 6, y, s->label, kText);
      char value[48];
      snprintf(value, sizeof value, "%s%s", s->choices[s->value], s->restart_needed ? "*" : "");
      s360_draw_text(px, pitch, clip_w, x0 + panel_w - 6 - s360_text_width(value), y, value, kValue);
    } else {
      s360_draw_text(px, pitch, clip_w, x0 + 6, y, it->kind == kItemBack ? "Voltar ao jogo" : "Sair do jogo", kText);
    }
  }
  if (g_scroll > 0) s360_draw_text(px, pitch, clip_w, x0 + panel_w - 12, 12, "^", kHint);
  if (g_scroll + rows < g_num_items) s360_draw_text(px, pitch, clip_w, x0 + panel_w - 12, 22 + rows * kRowH - 2, "v", kHint);
  bool restart = false;
  for (int i = 0; i < g_num_items; i++) restart |= g_items[i].kind == kItemSetting && g_items[i].setting->restart_needed;
  const char *hint = restart ? "<> muda  B volta  *=reiniciar" : "<> ou A muda   B volta";
  s360_draw_text(px, pitch, clip_w, x0 + (panel_w - s360_text_width(hint)) / 2, h - 12, hint, kHint);
  s360_draw_set_scale(1);
}
