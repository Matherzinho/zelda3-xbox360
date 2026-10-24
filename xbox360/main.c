// Xbox 360 frontend for zelda3, built with xsdk (github.com/xenosworks/xsdk) and SDL-xenon for audio.
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <SDL3/SDL.h>
#include "game.h"
#include "zelda_rtl.h"
#include "audio.h"
#include "snes/ppu.h"
#include "renderer.h"
#include "util.h"
#include "xsdk/kernel.h"
#include "xsdk/log.h"
#include "xsdk/splash.h"

// ---- Input ---------------------------------------------------------------------------------------------------------
// The game takes 12 bits: 0 B, 1 Y, 2 Select, 3 Start, 4 Up, 5 Down, 6 Left, 7 Right, 8 A, 9 X, 10 L, 11 R
// (NMI_ReadJoypads bit-reverses them into the SNES joypad registers; src/main.c's kKbdRemap uses the same order).
enum {
  kIn_B = 1 << 0, kIn_Y = 1 << 1, kIn_Select = 1 << 2, kIn_Start = 1 << 3,
  kIn_Up = 1 << 4, kIn_Down = 1 << 5, kIn_Left = 1 << 6, kIn_Right = 1 << 7,
  kIn_A = 1 << 8, kIn_X = 1 << 9, kIn_L = 1 << 10, kIn_R = 1 << 11,
};
enum { kStickDeadzone = 12000, kTriggerThreshold = 64 };

// Buttons by position, as on the SNES pad: Xbox A (bottom) = SNES B, Xbox B (right) = SNES A,
// Xbox X (left) = SNES Y, Xbox Y (top) = SNES X.
static uint16_t MapInput(const INPUT_GAMEPAD *g) {
  uint16_t b = g->buttons, out = 0;
  if (b & INPUT_GAMEPAD_A) out |= kIn_B;
  if (b & INPUT_GAMEPAD_B) out |= kIn_A;
  if (b & INPUT_GAMEPAD_X) out |= kIn_Y;
  if (b & INPUT_GAMEPAD_Y) out |= kIn_X;
  if (b & INPUT_GAMEPAD_LEFT_SHOULDER || g->left_trigger > kTriggerThreshold) out |= kIn_L;
  if (b & INPUT_GAMEPAD_RIGHT_SHOULDER || g->right_trigger > kTriggerThreshold) out |= kIn_R;
  if (b & INPUT_GAMEPAD_START) out |= kIn_Start;
  if (b & INPUT_GAMEPAD_BACK) out |= kIn_Select;
  if (b & INPUT_GAMEPAD_DPAD_UP || g->thumb_ly > kStickDeadzone) out |= kIn_Up;
  if (b & INPUT_GAMEPAD_DPAD_DOWN || g->thumb_ly < -kStickDeadzone) out |= kIn_Down;
  if (b & INPUT_GAMEPAD_DPAD_LEFT || g->thumb_lx < -kStickDeadzone) out |= kIn_Left;
  if (b & INPUT_GAMEPAD_DPAD_RIGHT || g->thumb_lx > kStickDeadzone) out |= kIn_Right;
  return out;
}

// ---- Audio ---------------------------------------------------------------------------------------------------------
// SDL-xenon plays through xsdk's audio API; the callback runs on SDL's device thread, so the game's APU state is
// guarded by g_audio_mutex exactly as the PC frontend does (src/main.c).
enum { kAudioFreq = 48000, kAudioChannels = 2 };
static SDL_Mutex *g_audio_mutex;
static int16_t *g_audiobuffer;
static int g_audiobuffer_pos, g_audiobuffer_len, g_frames_per_block;

void ZeldaApuLock(void) { if (g_audio_mutex) SDL_LockMutex(g_audio_mutex); }
void ZeldaApuUnlock(void) { if (g_audio_mutex) SDL_UnlockMutex(g_audio_mutex); }

static void SDLCALL AudioCallback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount) {
  (void)userdata; (void)total_amount;
  SDL_LockMutex(g_audio_mutex);
  while (additional_amount > 0) {
    if (g_audiobuffer_pos == g_audiobuffer_len) {
      ZeldaRenderAudio(g_audiobuffer, g_frames_per_block, kAudioChannels);
      g_audiobuffer_pos = 0;
      g_audiobuffer_len = g_frames_per_block * kAudioChannels * (int)sizeof(int16_t);
    }
    int n = SDL_min(additional_amount, g_audiobuffer_len - g_audiobuffer_pos);
    SDL_PutAudioStreamData(stream, (uint8_t *)g_audiobuffer + g_audiobuffer_pos, n);
    g_audiobuffer_pos += n;
    additional_amount -= n;
  }
  ZeldaDiscardUnusedAudioFrames();
  SDL_UnlockMutex(g_audio_mutex);
}

static void StartAudio(void) {
  if (!SDL_Init(SDL_INIT_AUDIO)) {
    xsdk_log_write("zelda3: audio SDL_Init failed: %s (continuing without sound)", SDL_GetError());
    return;
  }
  g_audio_mutex = SDL_CreateMutex();
  // The SPC produces 534 samples a frame at 32 kHz; ZeldaRenderAudio resamples to what we ask for.
  g_frames_per_block = (534 * kAudioFreq) / 32000;
  g_audiobuffer = calloc((size_t)g_frames_per_block * kAudioChannels, sizeof(int16_t));
  if (!g_audio_mutex || !g_audiobuffer) {
    xsdk_log_write("zelda3: audio allocation failed (continuing without sound)");
    return;
  }
  // Keep SDL's device thread off hardware thread 0, where the game runs.
  SDL_SetHint(SDL_HINT_AUDIO_XSDK_HARDWARE_THREAD, "2");
  const SDL_AudioSpec spec = { SDL_AUDIO_S16, kAudioChannels, kAudioFreq };
  SDL_AudioStream *stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, AudioCallback, NULL);
  if (!stream) {
    xsdk_log_write("zelda3: audio stream open failed: %s (continuing without sound)", SDL_GetError());
    return;
  }
  SDL_ResumeAudioStreamDevice(stream);
  xsdk_log_write("zelda3: stage=audio_ok freq=%d frames_per_block=%d", kAudioFreq, g_frames_per_block);
}

// ---- Assets --------------------------------------------------------------------------------------------------------
// Colour codes shown while starting, so a failure is visible without reading the log.
enum { COLOR_RENDERER_OK = 0x0000FF, COLOR_ASSETS_FAIL = 0xFF0000, COLOR_ASSETS_OK = 0x00FF00 };

static bool TryLoadAssets(const char *path, char *err, size_t err_size) {
  xsdk_log_write("zelda3: assets try path=%s", path);
  FILE *f = fopen(path, "rb");
  if (!f) { xsdk_log_write("zelda3: assets fopen failed errno=%d path=%s", errno, path); return false; }
  fclose(f);
  return ZeldaGameLoadAssets(path, err, err_size);
}

// Relative paths start at game:\, the title's folder (xsdk docs/GUIDE.md, "Files").
static bool LoadAssetsAny(const char *env_path, char *err, size_t err_size) {
  if (env_path && *env_path && TryLoadAssets(env_path, err, err_size)) return true;
  static const char *const kCandidates[] = { "zelda3_assets.dat", "game:\\zelda3_assets.dat" };
  for (unsigned i = 0; i < sizeof kCandidates / sizeof *kCandidates; i++)
    if (TryLoadAssets(kCandidates[i], err, err_size)) return true;
  if (!err[0]) snprintf(err, err_size, "zelda3_assets.dat not found next to the .xex");
  return false;
}

int main(void) {
  xsdk_log_write("zelda3: stage=program_start image=%s log=%s",
                 ExLoadedImageName ? ExLoadedImageName : "(unknown)",
                 xsdk_log_path() ? xsdk_log_path() : "(log file unavailable)");
  NTSTATUS splash_status = xsdk_splash_show(1500);
  xsdk_log_write("zelda3: stage=splash result=0x%08X", (unsigned)splash_status);

  struct RendererFuncs renderer;
  zelda_xenon_renderer_bind(&renderer);
  if (!zelda_xenon_renderer_open(256, 224)) {
    xsdk_log_write("zelda3: stage=renderer_open_failed");
    return 1;
  }
  xsdk_log_write("zelda3: stage=renderer_open_ok");
  zelda_xenon_renderer_show_color(COLOR_RENDERER_OK, 20);

  char asset_error[192] = "";
  if (!LoadAssetsAny(getenv("ZELDA3_ASSETS"), asset_error, sizeof asset_error)) {
    xsdk_log_write("zelda3: stage=assets_failed error=%s", asset_error);
    zelda_xenon_renderer_show_color(COLOR_ASSETS_FAIL, 300);
    zelda_xenon_renderer_close();
    return 2;
  }
  xsdk_log_write("zelda3: stage=assets_ok");
  zelda_xenon_renderer_show_color(COLOR_ASSETS_OK, 20);

  xsdk_log_write("zelda3: stage=game_initialize_begin");
  ZeldaGameInitialize(0, 0, NULL);  // no widescreen, no MSU, default (English) text
  ZeldaGameSetWantedFeatures(0);
  // In-game saves go to game:\saves\sram.dat (ZeldaWriteSram); the folder must exist.
  if (mkdir("saves", 0777) != 0 && errno != EEXIST)
    xsdk_log_write("zelda3: mkdir saves failed errno=%d (saving may not persist)", errno);
  ZeldaReadSram();
  xsdk_log_write("zelda3: stage=game_initialize_ok");

  StartAudio();

  xsdk_log_write("zelda3: stage=main_loop_enter (hold Back+Start to quit)");
  int frame = 0, quit_hold = 0;
  for (;;) {
    INPUT_STATE state = { 0 };
    uint32_t input_result = XamInputGetState(0, 0, &state);
    if (input_result != ERROR_SUCCESS) {
      if (frame == 0) xsdk_log_write("zelda3: first input poll failed result=0x%08X", (unsigned)input_result);
      memset(&state, 0, sizeof state);
    }
    // Back is the SNES Select button, so quitting needs Back+Start held for one second.
    uint16_t quit_mask = INPUT_GAMEPAD_BACK | INPUT_GAMEPAD_START;
    quit_hold = (state.gamepad.buttons & quit_mask) == quit_mask ? quit_hold + 1 : 0;
    if (quit_hold >= 60) {
      xsdk_log_write("zelda3: exit requested (Back+Start)");
      break;
    }
    if (frame == 0) xsdk_log_write("zelda3: stage=first_update_begin");
    ZeldaApuLock();
    ZeldaGameUpdate(MapInput(&state.gamepad));
    ZeldaApuUnlock();
    if (frame == 0) xsdk_log_write("zelda3: stage=first_update_ok");

    uint8_t *pixels = NULL; int pitch = 0;
    renderer.BeginDraw(256, 224, &pixels, &pitch);
    if (!pixels) { xsdk_log_write("zelda3: frame buffer unavailable"); break; }
    ZeldaGameRender(pixels, (size_t)pitch, 0);
    if (frame == 0) xsdk_log_write("zelda3: stage=first_frame_render_ok");
    renderer.EndDraw();
    if (frame == 0) xsdk_log_write("zelda3: stage=first_frame_present_returned");
    if (frame == 600) xsdk_log_write("zelda3: 600 frames ran");
    frame++;
  }
  xsdk_log_write("zelda3: stage=renderer_close frames=%d", frame);
  zelda_xenon_renderer_close();
  xsdk_log_write("zelda3: stage=program_exit");
  return 0;
}

void NORETURN Die(const char *error) {
  xsdk_log_write("zelda3 fatal: %s", error);
  exit(1);
}
