// The Legend of Zelda: A Link to the Past (zelda3) on the snes360 platform layer. Everything Xbox-specific lives in
// ../platform; this file only describes the game: its options and how to run, draw and play it.
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "snes360.h"
#include "assets.h"
#include "audio.h"
#include "features.h"
#include "game.h"
#include "snes/ppu.h"
#include "util.h"
#include "zelda_rtl.h"

// ---- Options -------------------------------------------------------------------------------------------------------
// Every default is the original game; each option is one of zelda3's own features (zelda3.ini on the PC) unless noted.
static const char *const kOffOn[] = { "N\xC3\xA3o", "Sim", NULL };
static const char *const kTextSpeeds[] = { "Normal", "R\xC3\xA1pido", "Instant\xC3\xA2neo", NULL };
static const char *kLanguageChoices[16] = { "English", NULL };   // filled from the assets at start
static char g_language_codes[16][8];

enum {
  kOptWidescreen, kOptLanguage, kOptTextSpeed, kOptHiresMap, kOptNoSpriteLimits, kOptItemSwitch, kOptTurnDash,
  kOptMirrorDark, kOptSwordItems, kOptSwordPots, kOptNoBeep, kOptSkipIntro, kOptMaxYellow, kOptMoreBombs,
  kOptMoreRupees, kOptBugFixes, kOptGameFixes, kOptCancelBird, kOptDimFlashes, kNumOpts
};
static S360Setting g_opts[kNumOpts] = {
  [kOptWidescreen] = { "widescreen", "Widescreen", kOffOn, 0, 0, false },
  [kOptLanguage] = { "language", "Idioma", kLanguageChoices, 0, 0, false },
  [kOptTextSpeed] = { "text_speed", "Velocidade do texto", kTextSpeeds, 0, 0, false },
  [kOptHiresMap] = { "hires_map", "Mapa em alta res.", kOffOn, 0, 0, false },
  [kOptNoSpriteLimits] = { "no_sprite_limits", "Sem limite sprites", kOffOn, 0, 0, false },
  [kOptItemSwitch] = { "item_switch", "Trocar item L/R", kOffOn, 0, 0, false },
  [kOptTurnDash] = { "turn_while_dashing", "Virar correndo", kOffOn, 0, 0, false },
  [kOptMirrorDark] = { "mirror_to_darkworld", "Espelho p/ Sombrio", kOffOn, 0, 0, false },
  [kOptSwordItems] = { "collect_with_sword", "Pegar itens c/ esp.", kOffOn, 0, 0, false },
  [kOptSwordPots] = { "break_pots_with_sword", "Potes c/ espada", kOffOn, 0, 0, false },
  [kOptNoBeep] = { "no_low_health_beep", "Sem bipe vida baixa", kOffOn, 0, 0, false },
  [kOptSkipIntro] = { "skip_intro", "Pular intro (bot\xC3\xA3o)", kOffOn, 0, 0, false },
  [kOptMaxYellow] = { "max_items_yellow", "M\xC3\xA1ximos amarelos", kOffOn, 0, 0, false },
  [kOptMoreBombs] = { "more_bombs", "4 bombas na tela", kOffOn, 0, 0, false },
  [kOptMoreRupees] = { "more_rupees", "R\xC3\xBApias at\xC3\xA9 9999", kOffOn, 0, 0, false },
  [kOptBugFixes] = { "bug_fixes", "Corrigir bugs", kOffOn, 0, 0, false },
  [kOptGameFixes] = { "game_changing_fixes", "Corrigir bugs (+)", kOffOn, 0, 0, false },
  [kOptCancelBird] = { "cancel_bird", "Cancelar p\xC3\xA1ssaro", kOffOn, 0, 0, false },
  [kOptDimFlashes] = { "dim_flashes", "Atenuar flashes", kOffOn, 0, 0, false },
};

static uint32 g_render_flags;

// Display names for the languages zelda3's asset tool can extract (assets/restool.py --languages).
static const char *LanguageName(const char *code) {
  static const char *const kNames[][2] = {
    { "de", "Deutsch" }, { "fr", "Fran\xC3\xA7" "ais" }, { "fr-c", "Fran\xC3\xA7" "ais CA" }, { "es", "Espa\xC3\xB1ol" },
    { "pl", "Polski" }, { "pt", "Portugu\xC3\xAAs" }, { "nl", "Nederlands" }, { "sv", "Svenska" },
    { "redux", "English Redux" }, { "en", "English" },
  };
  for (unsigned i = 0; i < sizeof kNames / sizeof kNames[0]; i++)
    if (strcmp(kNames[i][0], code) == 0) return kNames[i][1];
  return code;
}

// The assets hold the default language's text plus any extra languages they were built with.
static void FindLanguages(void) {
  int n = 1;
  for (int i = 0; n < 15; i++) {
    MemBlk mb = kDialogueMap(i);
    if (mb.ptr == 0) break;
    MemBlk name = FindIndexInMemblk(mb, 0);
    if (name.size == 0 || name.size >= sizeof g_language_codes[0]) continue;
    memcpy(g_language_codes[n], name.ptr, name.size);
    g_language_codes[n][name.size] = 0;
    kLanguageChoices[n] = LanguageName(g_language_codes[n]);
    n++;
  }
  kLanguageChoices[n] = NULL;
  s360_log("zelda3: %d language(s) in the assets", n);
}

// ---- Game interface ------------------------------------------------------------------------------------------------
static bool Init(char *error, size_t error_size) {
  const char *path = getenv("ZELDA3_ASSETS");
  if (!ZeldaGameLoadAssets(path && *path ? path : "zelda3_assets.dat", error, error_size)) return false;
  ZeldaGameInitialize(0, 0, NULL);
  FindLanguages();
  // In-game saves go to saves\sram.dat (ZeldaWriteSram); the folder must exist.
  if (mkdir("saves", 0777) != 0 && errno != EEXIST) s360_log("zelda3: mkdir saves failed errno=%d", errno);
  ZeldaReadSram();
  return true;
}

static int ChoiceCount(const S360Setting *s) { int n = 0; while (s->choices[n]) n++; return n; }

static void ApplySettings(S360Aspect aspect) {
  const S360Setting *o = g_opts;
  // Widescreen: as many extra columns each side as fill 16:9 in this display mode (zelda3's ExtendedAspectRatio).
  int extra = 0;
  if (o[kOptWidescreen].value) {
    float par = aspect == kS360Aspect_TV ? 7.0f / 6.0f : 1.0f;  // TV mode shows 256 pixels 4:3 wide
    extra = (int)((224.0f * 16.0f / 9.0f / par - 256.0f) / 2.0f);
    if (extra > kPpuExtraLeftRight) extra = kPpuExtraLeftRight;
  }
  g_zenv.ppu->extraLeftRight = (uint8)extra;

  uint32 f = 0;
  if (extra) f |= kFeatures0_ExtendScreen64 | kFeatures0_WidescreenVisualFixes;
  if (o[kOptItemSwitch].value) f |= kFeatures0_SwitchLR;
  if (o[kOptTurnDash].value) f |= kFeatures0_TurnWhileDashing;
  if (o[kOptMirrorDark].value) f |= kFeatures0_MirrorToDarkworld;
  if (o[kOptSwordItems].value) f |= kFeatures0_CollectItemsWithSword;
  if (o[kOptSwordPots].value) f |= kFeatures0_BreakPotsWithSword;
  if (o[kOptNoBeep].value) f |= kFeatures0_DisableLowHealthBeep;
  if (o[kOptSkipIntro].value) f |= kFeatures0_SkipIntroOnKeypress;
  if (o[kOptMaxYellow].value) f |= kFeatures0_ShowMaxItemsInYellow;
  if (o[kOptMoreBombs].value) f |= kFeatures0_MoreActiveBombs;
  if (o[kOptMoreRupees].value) f |= kFeatures0_CarryMoreRupees;
  if (o[kOptBugFixes].value) f |= kFeatures0_MiscBugFixes;
  if (o[kOptGameFixes].value) f |= kFeatures0_GameChangingBugFixes;
  if (o[kOptCancelBird].value) f |= kFeatures0_CancelBirdTravel;
  if (o[kOptDimFlashes].value) f |= kFeatures0_DimFlashes;
  if (o[kOptTextSpeed].value == 1) f |= kFeatures0_TextFast;
  if (o[kOptTextSpeed].value == 2) f |= kFeatures0_TextInstant;
  ZeldaGameSetWantedFeatures(f);

  g_render_flags = kPpuRenderFlags_NewRenderer;
  if (o[kOptHiresMap].value) g_render_flags |= kPpuRenderFlags_4x4Mode7;
  if (o[kOptNoSpriteLimits].value) g_render_flags |= kPpuRenderFlags_NoSpriteLimits;

  int lang = o[kOptLanguage].value;
  if (lang >= ChoiceCount(&o[kOptLanguage])) lang = 0;
  ZeldaSetLanguage(lang ? g_language_codes[lang] : NULL);
  s360_log("zelda3: settings extra=%d features=0x%X render=0x%X language=%s", extra, (unsigned)f,
           (unsigned)g_render_flags, kLanguageChoices[lang]);
}

static void RunFrame(uint16_t buttons) {
  ZeldaGameUpdate(buttons);  // zelda3 reads the same 12-bit order as kS360_*
}

static void Draw(uint32_t *pixels, int pitch, int *width, int *height) {
  int scale = PpuGetCurrentRenderScale(g_zenv.ppu, g_render_flags);
  *width = (256 + 2 * g_zenv.ppu->extraLeftRight) * scale;
  *height = 224 * scale;
  ZeldaGameRender((uint8 *)pixels, (size_t)pitch * 4, g_render_flags);
}

static void RenderAudio(int16_t *samples, int frames) {
  ZeldaRenderAudio(samples, frames, 2);
  ZeldaDiscardUnusedAudioFrames();
}

// zelda3's core calls these; the platform's lock is the one shared with the audio thread.
void ZeldaApuLock(void) { s360_audio_lock(); }
void ZeldaApuUnlock(void) { s360_audio_unlock(); }
void NORETURN Die(const char *error) {
  s360_log("zelda3 fatal: %s", error);
  exit(1);
}

int main(void) {
  static const S360Game game = {
    .name = "zelda3",
    .title = "Zelda: A Link to the Past",
    // Widest picture: 96 extra columns each side, times 4 for the high-resolution world map.
    .max_width = (256 + 2 * kPpuExtraLeftRight) * 4,
    .max_height = 224 * 4,
    .init = Init,
    .apply_settings = ApplySettings,
    .run_frame = RunFrame,
    .draw = Draw,
    .render_audio = RenderAudio,
    .shutdown = NULL,
    .settings = g_opts,
    .num_settings = kNumOpts,
  };
  return s360_run(&game);
}
