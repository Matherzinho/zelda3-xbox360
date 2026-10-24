// Localizable interface text: the words the game draws with its own tiles (the HUD, the inventory and the ability
// box), as opposed to the dialogue, which lives in the assets (kDialogue, one entry per language).
//
// The game's own language draws exactly the tiles it always did; a locale added here replaces those words. Two kinds
// of word are handled:
//   - item names, ability names and so on, drawn letter by letter with the game's 8x8 alphabet (A-Z, space, period);
//   - the fixed labels drawn as pre-made art, two letters to a tile ("LIFE", "EQUIPMENT", "DUNGEON ITEM", "DO"),
//     which a locale replaces by drawing its own text into those tiles with the small font in ui_text.c.
//
// Adding a language means adding one UiLocale below, with a translation for every id; nothing else changes.
#pragma once
#include "types.h"

enum {
  // Labels drawn as art, two letters to a tile (kUiLabel_*). Each has room for a set number of letters.
  kUiText_Life,         // over the hearts, 4 letters
  kUiText_Equipment,    // the equipment box, 12 letters
  kUiText_DungeonItem,  // the dungeon item box, 14 letters
  kUiText_Do,           // over the ability box, 4 letters
  kUiText_Pendants,     // over the pendants, 10 letters
  kUiText_Crystals,     // over the crystals, 10 letters
  kUiLabel_Count,

  // Words drawn with the game's alphabet. "\n" separates the two rows; each row holds at most 8 letters (5 for the
  // abilities). Spaces place the text as the original does.
  kUiText_Ability_Lift1 = kUiLabel_Count, kUiText_Ability_Read, kUiText_Ability_Talk,
  kUiText_Ability_Pull, kUiText_Ability_Run, kUiText_Ability_Swim,
  kUiText_Ability_Lift2, kUiText_Ability_Lift3,

  kUiText_NotAssigned,
  // Item names, in the order the game's own name table has them (slot 1 to 20 of the inventory).
  kUiText_Item_Bow, kUiText_Item_Boomerang, kUiText_Item_Hookshot, kUiText_Item_Bomb, kUiText_Item_Mushroom,
  kUiText_Item_FireRod, kUiText_Item_IceRod, kUiText_Item_Bombos, kUiText_Item_Ether, kUiText_Item_Quake,
  kUiText_Item_Lamp, kUiText_Item_Hammer, kUiText_Item_Shovel, kUiText_Item_BugNet, kUiText_Item_BookMudora,
  kUiText_Item_Mushroom2, kUiText_Item_CaneSomaria, kUiText_Item_CaneByrna, kUiText_Item_Cape, kUiText_Item_Empty,
  // The forms an item takes, named by their own branch in the inventory.
  kUiText_Item_BowArrows, kUiText_Item_BowSilver, kUiText_Item_Powder, kUiText_Item_Flute, kUiText_Item_Mirror,
  // What a bottle holds.
  kUiText_Bottle_Mushroom, kUiText_Bottle_Empty, kUiText_Bottle_LifeMedicine, kUiText_Bottle_MagicMedicine,
  kUiText_Bottle_CureAll, kUiText_Bottle_Faerie, kUiText_Bottle_Bee, kUiText_Bottle_GoodBee,
  kUiText_Count
};

// The text for an id in the language ZeldaSetLanguage chose, or NULL when the game's own tiles are to be used.
const char *UiText(int id);
// Picks the locale for a language code ("pt-br"), or the game's own text for NULL or a language with no locale.
void UiSetLocale(const char *language);
// True when a locale is in use, so the caller draws text instead of the game's own art.
bool UiHasLocale(void);

// Fills count tilemap entries with a word drawn in the game's alphabet: attr_base is the entry for tile 0 (0x2400 for
// the inventory, 0x2c00 for the ability box), and rows of the text are placed row_tiles entries apart.
void UiTextToTiles(zle16_t *dst, const char *text, int row_tiles, int rows, uint16 attr_base);
// Draws the locale's labels into the tiles the game's own labels use, in the 2bpp graphics at vram. Called whenever
// those graphics are loaded; it does nothing without a locale.
void UiDrawLabels(zle16_t *vram);
// The tilemap entries for a label that no longer fits its own tiles (the equipment box); fills count entries.
void UiLabelTiles(int id, zle16_t *dst, int count, uint16 attr_base);
