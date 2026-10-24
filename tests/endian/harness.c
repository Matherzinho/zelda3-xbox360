// Headless determinism harness: runs the core for N frames, prints hash of frame and RAM per frame.
// Build natively (reference) and for big-endian PowerPC under qemu-ppc; outputs must be identical.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "config.h"
#include "zelda_rtl.h"
#include "audio.h"
#include "snes/ppu.h"
#include "util.h"
#include "zelda_rtl.h"

Config g_config;
void NORETURN Die(const char *error) { fprintf(stderr, "Die: %s\n", error); exit(1); }
void ZeldaApuLock(void) {}
void ZeldaApuUnlock(void) {}

// Hash of a byte region, 8 bytes at a time, read as little-endian words so every host gets the same value.
static uint64_t fnv(const uint8 *p, size_t n, uint64_t h) {
  size_t i = 0;
  for (; i + 8 <= n; i += 8) {
    uint64_t w; memcpy(&w, p + i, 8);
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    w = __builtin_bswap64(w);
#endif
    h = (h ^ w) * 0x100000001b3ull; h ^= h >> 29;
  }
  for (; i < n; i++) { h ^= p[i]; h *= 1099511628211ull; }
  return h;
}
static uint16 PadForFrame(int f) {
  int ph = f % 90; uint16 in = 0;
  if (ph >= 10 && ph < 14) in |= (uint16)(kJoypadH_Start << 8);
  if (ph >= 50 && ph < 54) in |= kJoypadL_A;
  return in;
}
int main(int argc, char **argv) {
  const char *assets = argc > 1 ? argv[1] : "zelda3_assets.dat";
  int frames = argc > 2 ? atoi(argv[2]) : 60;
  const char *dump = argc > 3 ? argv[3] : NULL;
  char err[256];
  if (!ZeldaGameLoadAssets(assets, err, sizeof err)) { fprintf(stderr, "assets: %s\n", err); return 2; }
  ZeldaGameInitialize(0, 0, "us");
  ZeldaGameSetWantedFeatures(0);
  int replay = getenv("ZREPLAY") ? atoi(getenv("ZREPLAY")) : -1;  // replay saves/ref chapter N (0-12)
  if (replay >= 0) SaveLoadSlot(kSaveLoad_Replay, 256 + replay);
  enum { PITCH = 1024 * 4 };
  uint8 *pix = calloc(1024, PITCH);
  extern uint8 g_ram[131072];
  bool audio = getenv("ZAUDIO") != NULL;  // also generate 48 kHz stereo audio each frame, as the Xbox frontend does
  int16 audio_buf[801 * 2];
  FILE *st = getenv("ZSTATE") ? fopen(getenv("ZSTATE"), "wb") : NULL;
  for (int f = 0; f < frames; f++) {
    ZeldaGameUpdate(PadForFrame(f));
    int scale = PpuGetCurrentRenderScale(g_zenv.ppu, 0);
    int w = 256 * scale, h = 224 * scale;
    ZeldaGameRender(pix, PITCH, 0);
    uint64_t hp = 1469598103934665603ull;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x += 2) {  // hash pixel values, not host byte order
      uint32 p0, p1; memcpy(&p0, pix + (size_t)y * PITCH + x * 4, 4); memcpy(&p1, pix + (size_t)y * PITCH + x * 4 + 4, 4);
      hp = (hp ^ (p0 | (uint64_t)p1 << 32)) * 0x100000001b3ull; hp ^= hp >> 29;
    }
    uint64_t hr = fnv(g_ram, 0x20000, 1469598103934665603ull);
    Ppu *pp = g_zenv.ppu;
    uint64_t hv = fnv((uint8 *)pp->vram, 0x10000, 1469598103934665603ull);
    uint64_t hc = fnv((uint8 *)pp->cgram, 0x200, fnv((uint8 *)pp->oam, 0x220, 1469598103934665603ull));
    uint64_t hs = fnv(g_zenv.sram, 0x2000, 1469598103934665603ull);
    uint64_t ha = 0;
    if (audio) {
      ZeldaRenderAudio(audio_buf, 801, 2);
      ZeldaDiscardUnusedAudioFrames();
      for (int i = 0; i < 801 * 2; i++) { ha = (ha ^ (uint16)audio_buf[i]) * 0x100000001b3ull; ha ^= ha >> 29; }
    }
    printf("frame %5d scale %d pix %016llx ram %016llx vram %016llx cgoam %016llx sram %016llx audio %016llx\n", f, scale,
           (unsigned long long)hp, (unsigned long long)hr, (unsigned long long)hv, (unsigned long long)hc, (unsigned long long)hs,
           (unsigned long long)ha);
    fflush(stdout);
    int st_from = getenv("ZSTATE_FROM") ? atoi(getenv("ZSTATE_FROM")) : 0;
    if (st && f >= st_from) {  // raw state, written in little-endian order on every host so files are comparable
      fwrite(g_ram, 1, 0x20000, st);
      Ppu *ppu = g_zenv.ppu;
      // PPU memories hold little-endian words on every build, so raw bytes compare directly
      fwrite(ppu->vram, 1, 0x10000, st);
      fwrite(ppu->cgram, 1, 0x200, st);
      fwrite(ppu->oam, 1, 0x220, st);
      fwrite(g_zenv.sram, 1, 0x2000, st);
      for (int y = 0; y < 224; y++) for (int x = 0; x < 256; x++) {
        uint32 p; memcpy(&p, pix + (size_t)y * PITCH + x * 4, 4);
        uint8 b[4] = { p, p >> 8, p >> 16, p >> 24 }; fwrite(b, 1, 4, st);
      }
    }
  }
  if (dump) {
    FILE *o = fopen(dump, "wb");
    int scale = PpuGetCurrentRenderScale(g_zenv.ppu, 0), w = 256 * scale, h = 224 * scale;
    fprintf(o, "P6\n%d %d\n255\n", w, h);
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
      uint32 p; memcpy(&p, pix + (size_t)y * PITCH + x * 4, 4);
      fputc((p >> 16) & 255, o); fputc((p >> 8) & 255, o); fputc(p & 255, o);
    }
    fclose(o);
  }
  return 0;
}
