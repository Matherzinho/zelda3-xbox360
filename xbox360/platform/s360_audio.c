// Audio through SDL-xenon (which plays through xsdk's audio API). SDL's device thread pulls samples; the game's
// sound state is shared with the main thread, so both take the audio lock, as zelda3's PC frontend does.
#include <SDL3/SDL.h>
#include <stdlib.h>
#include "s360_internal.h"

// A block is one SNES frame of sound: 534 samples at 32 kHz make 801 at 48 kHz, as zelda3's PC frontend asks for.
enum { kRate = 48000, kChannels = 2, kBlockFrames = 534 * kRate / 32000 };
static SDL_Mutex *g_mutex;
static const S360Game *g_game;
static int16_t g_block[kBlockFrames * kChannels];
static int g_pos = kBlockFrames * kChannels * 2, g_len = kBlockFrames * kChannels * 2;  // bytes

void s360_audio_lock(void) { if (g_mutex) SDL_LockMutex(g_mutex); }
void s360_audio_unlock(void) { if (g_mutex) SDL_UnlockMutex(g_mutex); }
int s360_audio_rate(void) { return kRate; }

static void SDLCALL Callback(void *userdata, SDL_AudioStream *stream, int additional, int total) {
  (void)userdata; (void)total;
  SDL_LockMutex(g_mutex);
  while (additional > 0) {
    if (g_pos == g_len) {
      g_game->render_audio(g_block, kBlockFrames);
      g_pos = 0;
    }
    int n = SDL_min(additional, g_len - g_pos);
    SDL_PutAudioStreamData(stream, (uint8_t *)g_block + g_pos, n);
    g_pos += n;
    additional -= n;
  }
  SDL_UnlockMutex(g_mutex);
}

void s360_audio_start(const S360Game *game) {
  g_game = game;
  g_mutex = SDL_CreateMutex();  // the lock exists even when there is no sound
  if (!game->render_audio) return;
  if (!SDL_Init(SDL_INIT_AUDIO)) {
    s360_log("s360: audio SDL_Init failed: %s (continuing without sound)", SDL_GetError());
    return;
  }
  if (!g_mutex) {
    s360_log("s360: audio mutex failed (continuing without sound)");
    return;
  }
  SDL_SetHint(SDL_HINT_AUDIO_XSDK_HARDWARE_THREAD, "2");  // off hardware thread 0, where the game runs
  const SDL_AudioSpec spec = { SDL_AUDIO_S16, kChannels, kRate };
  SDL_AudioStream *stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, Callback, NULL);
  if (!stream) {
    s360_log("s360: audio stream failed: %s (continuing without sound)", SDL_GetError());
    return;
  }
  SDL_ResumeAudioStreamDevice(stream);
  s360_log("s360: audio %d Hz stereo", kRate);
}
