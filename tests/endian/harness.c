// Headless determinism harness: runs the core for N frames, prints hash of frame and RAM per frame.
// Build natively (reference) and for big-endian PowerPC under qemu-ppc; outputs must be identical.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "config.h"
#include "zelda_rtl.h"
#include "snes/ppu.h"
#include "util.h"

Config g_config;
void NORETURN Die(const char *error) { fprintf(stderr, "Die: %s\n", error); exit(1); }
void ZeldaApuLock(void) {}
void ZeldaApuUnlock(void) {}

static uint64_t fnv(const uint8 *p, size_t n, uint64_t h) {
  for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 1099511628211ull; }
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
  enum { PITCH = 1024 * 4 };
  uint8 *pix = calloc(1024, PITCH);
  extern uint8 g_ram[131072];
  for (int f = 0; f < frames; f++) {
    ZeldaGameUpdate(PadForFrame(f));
    int scale = PpuGetCurrentRenderScale(g_zenv.ppu, 0);
    int w = 256 * scale, h = 224 * scale;
    ZeldaGameRender(pix, PITCH, 0);
    uint64_t hp = 1469598103934665603ull;
    for (int y = 0; y < h; y++) hp = fnv(pix + (size_t)y * PITCH, (size_t)w * 4, hp);
    uint64_t hr = fnv(g_ram, 0x20000, 1469598103934665603ull);
    printf("frame %4d scale %d pix %016llx ram %016llx\n", f, scale, (unsigned long long)hp, (unsigned long long)hr);
    fflush(stdout);
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
