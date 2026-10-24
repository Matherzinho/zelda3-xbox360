// Interface text per language; see ui_text.h. Without a locale every function here is a no-op and the game draws
// the tiles it always did, so the original language is untouched.
#include <string.h>
#include "ui_text.h"
#include "zelda_rtl.h"

typedef struct UiLocale {
  const char *code;
  const char *text[kUiText_Count];
} UiLocale;

// Brazilian Portuguese. The game's alphabet has no accented letters, so these words are written without accents.
static const UiLocale kLocale_PtBr = {
  "pt-br",
  {
    [kUiText_Life] = "VIDA",
    [kUiText_Equipment] = "EQUIPAMENTO",
    [kUiText_DungeonItem] = "MASMORRA",
    [kUiText_Do] = "FACA",
    [kUiText_Pendants] = "PINGENTES",
    [kUiText_Crystals] = "CRISTAIS",

    [kUiText_Ability_Lift1] = "\nPEGA1",
    [kUiText_Ability_Read] = "\n LEIA",
    [kUiText_Ability_Talk] = "\n FALE",
    [kUiText_Ability_Pull] = "\nPUXE ",
    [kUiText_Ability_Run] = "\nCORRA",
    [kUiText_Ability_Swim] = "\n NADE",
    [kUiText_Ability_Lift2] = "\nPEGA2",
    [kUiText_Ability_Lift3] = "\nPEGA3",

    [kUiText_NotAssigned] = "   SEM\n    ITEM",
    [kUiText_Item_Bow] = "\n    ARCO",
    [kUiText_Item_BowArrows] = "  ARCO E\n FLECHAS",
    [kUiText_Item_BowSilver] = " FLECHAS\nDE PRATA",
    [kUiText_Item_Mushroom2] = "\nCOGUMELO",
    [kUiText_Item_Empty] = "\n",
    [kUiText_Item_Boomerang] = "\nBUMERANG",
    [kUiText_Item_Hookshot] = "\n  GANCHO",
    [kUiText_Item_Bomb] = "\n   BOMBA",
    [kUiText_Item_Mushroom] = "\nCOGUMELO",
    [kUiText_Item_Powder] = "      PO\n  MAGICO",
    [kUiText_Item_FireRod] = " VARA DE\n    FOGO",
    [kUiText_Item_IceRod] = " VARA DE\n    GELO",
    [kUiText_Item_Bombos] = "\n  BOMBOS",
    [kUiText_Item_Ether] = "\n   ETHER",
    [kUiText_Item_Quake] = "\n   QUAKE",
    [kUiText_Item_Lamp] = "\nLANTERNA",
    [kUiText_Item_Hammer] = " MARTELO\n  MAGICO",
    [kUiText_Item_Flute] = "\n  FLAUTA",
    [kUiText_Item_BugNet] = " REDE DE\n INSETOS",
    [kUiText_Item_BookMudora] = "LIVRO DE\n  MUDORA",
    [kUiText_Item_CaneSomaria] = "  CAJADO\n SOMARIA",
    [kUiText_Item_CaneByrna] = "  CAJADO\n   BYRNA",
    [kUiText_Item_Cape] = "    CAPA\n  MAGICA",
    [kUiText_Item_Mirror] = " ESPELHO\n  MAGICO",
    [kUiText_Item_Shovel] = "\n      PA",

    [kUiText_Bottle_Mushroom] = "\nCOGUMELO",
    [kUiText_Bottle_Empty] = "\n GARRAFA",
    [kUiText_Bottle_LifeMedicine] = "POCAO DE\n    VIDA",
    [kUiText_Bottle_MagicMedicine] = "POCAO DE\n   MAGIA",
    [kUiText_Bottle_CureAll] = "   POCAO\nCOMPLETA",
    [kUiText_Bottle_Faerie] = "\n    FADA",
    [kUiText_Bottle_Bee] = "\n  ABELHA",
    [kUiText_Bottle_GoodBee] = "  ABELHA\n     BOA",
  },
};

static const UiLocale *const kLocales[] = { &kLocale_PtBr };
static const UiLocale *g_locale;

void UiSetLocale(const char *language) {
  g_locale = NULL;
  if (!language)
    return;
  for (size_t i = 0; i != sizeof(kLocales) / sizeof(kLocales[0]); i++)
    if (strcmp(kLocales[i]->code, language) == 0)
      g_locale = kLocales[i];
}

bool UiHasLocale(void) { return g_locale != NULL; }

const char *UiText(int id) {
  return g_locale && (unsigned)id < kUiText_Count ? g_locale->text[id] : NULL;
}

// ---- Text in the game's own 8x8 alphabet -----------------------------------------------------------------------
enum { kTile_Space = 0xf5, kTile_A = 0x150, kTile_Period = 0x16a, kTile_Digit1 = 0x127 };

static uint16 TileForChar(uint8 c) {
  if (c >= 'A' && c <= 'Z') return kTile_A + c - 'A';
  if (c >= '1' && c <= '3') return kTile_Digit1 + c - '1';
  if (c == '.') return kTile_Period;
  return kTile_Space;
}

void UiTextToTiles(zle16_t *dst, const char *text, int row_tiles, int rows, uint16 attr_base) {
  for (int row = 0; row < rows; row++) {
    int i = 0;
    for (; i < row_tiles && *text && *text != '\n'; i++)
      dst[row * row_tiles + i].v = attr_base + TileForChar((uint8)*text++);
    for (; i < row_tiles; i++)
      dst[row * row_tiles + i].v = attr_base + kTile_Space;
    while (*text && *text != '\n')  // a row longer than the box: the rest is dropped
      text++;
    if (*text == '\n')
      text++;
  }
}

// ---- The labels drawn as art ------------------------------------------------------------------------------------
// A 3x5 capital font, drawn like the game's own small lettering: the letter in colour 1 with colour 2 a pixel down
// and to the right. Each letter takes a 4 pixel cell, so a tile holds two of them.
// Five rows of three pixels, the top row in bits 14..12 and the leftmost pixel of a row in its high bit: a space,
// then A to Z.
static const uint16 kSmallFont[] = {
  0x0000, 0x2BED, 0x6BAE, 0x3923, 0x6B6E, 0x79A7, 0x79A4, 0x396B,
  0x5BED, 0x7497, 0x126A, 0x5BAD, 0x4927, 0x5FED, 0x5FFD, 0x2B6A,
  0x6BA4, 0x2B73, 0x6BAD, 0x388E, 0x7492, 0x5B6B, 0x5B6A, 0x5BFD,
  0x5AAD, 0x5A92, 0x72A7,
};

// Returns the 5x3 bitmap rows of a character, top row first, bit 2 leftmost.
static void SmallGlyph(uint8 c, uint8 rows[5]) {
  unsigned index = (c >= 'A' && c <= 'Z') ? 1 + c - 'A' : 0;
  uint16 bits = kSmallFont[index];
  for (int y = 0; y < 5; y++)
    rows[y] = (bits >> (12 - y * 3)) & 7;
}

// Where each label lives: the tiles the game's own art uses, and how many letters fit (two to a tile).
// The colours are the palette's, as the game's own labels use them: white letters (2) over the picture, with the red
// (1) the game's lettering has beneath them, and white on the boxes' black (3) inside the menu.
typedef struct UiLabelPlace {
  uint16 tiles[8];
  uint8 num_tiles, background, ink, shadow;
} UiLabelPlace;
// The equipment and dungeon labels are drawn into tiles the game uses for the bug net's name, which a locale writes
// with the alphabet instead, because their own tiles are shared with the "ITEM" label or too few.
static const UiLabelPlace kLabelPlaces[kUiLabel_Count] = {
  [kUiText_Life] = { { 0xab, 0xac }, 2, 0, 2, 1 },                              // over the hearts, on the picture
  [kUiText_Equipment] = { { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05 }, 6, 3, 2, 3 },
  [kUiText_DungeonItem] = { { 0x06, 0x07, 0x08, 0x09 }, 4, 3, 2, 3 },
  [kUiText_Do] = { { 0x82, 0x83 }, 2, 3, 2, 3 },
  [kUiText_Pendants] = { { 0x121, 0x122, 0x123, 0x124, 0x13f }, 5, 3, 2, 3 },
  [kUiText_Crystals] = { { 0x12f, 0x134, 0x135, 0x136, 0x137 }, 5, 3, 2, 3 },
};

// Writes one 2bpp tile: pixels[y][x] holds the colour of each pixel.
static void WriteTile(zle16_t *vram, int tile, const uint8 pixels[8][8]) {
  zle16_t *dst = vram + 0x7000 + tile * 8;
  for (int y = 0; y < 8; y++) {
    uint32 plane0 = 0, plane1 = 0;
    for (int x = 0; x < 8; x++) {
      plane0 |= (uint32)(pixels[y][x] & 1) << (7 - x);
      plane1 |= (uint32)((pixels[y][x] >> 1) & 1) << (7 - x);
    }
    dst[y].v = (uint16)(plane0 | plane1 << 8);
  }
}

static void DrawLabel(zle16_t *vram, int id) {
  const char *text = UiText(id);
  const UiLabelPlace *place = &kLabelPlaces[id];
  if (!text)
    return;
  int len = (int)strlen(text), cells = place->num_tiles * 2;
  int start = (cells - (len < cells ? len : cells)) / 2;  // centred in the room the label has
  for (int t = 0; t < place->num_tiles; t++) {
    uint8 pixels[8][8];
    memset(pixels, place->background, sizeof pixels);
    for (int half = 0; half < 2; half++) {
      int cell = t * 2 + half, i = cell - start;
      if (i < 0 || i >= len)
        continue;
      uint8 rows[5];
      SmallGlyph((uint8)text[i], rows);
      for (int y = 0; y < 5; y++)
        for (int x = 0; x < 3; x++)
          if (rows[y] >> (2 - x) & 1) {
            if (pixels[y + 2][half * 4 + x + 1] != place->ink)
              pixels[y + 2][half * 4 + x + 1] = place->shadow;  // the shadow the game's own lettering has
            pixels[y + 1][half * 4 + x] = place->ink;
          }
    }
    WriteTile(vram, place->tiles[t], pixels);
  }
}

void UiDrawLabels(zle16_t *vram) {
  if (!g_locale)
    return;
  for (int id = 0; id < kUiLabel_Count; id++)
    DrawLabel(vram, id);
}

void UiLabelTiles(int id, zle16_t *dst, int count, uint16 attr_base) {
  const UiLabelPlace *place = &kLabelPlaces[id];
  int n = place->num_tiles < count ? place->num_tiles : count;
  int start = (count - n) / 2;
  for (int i = 0; i < count; i++)
    dst[i].v = (i >= start && i - start < n) ? attr_base + place->tiles[i - start] : attr_base + kTile_Space;
}
