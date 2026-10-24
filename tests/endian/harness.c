#include <time.h>
// Headless determinism harness: runs the core for N frames, prints hash of frame and RAM per frame.
// Build natively (reference) and for big-endian PowerPC under qemu-ppc; outputs must be identical.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "config.h"
#include "zelda_rtl.h"
#include "audio.h"
#include "spc_player.h"
#include "snes/dsp.h"
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
#include "../../xbox360/platform/s360_jobs.h"
// ZTHREADS=n draws 4x4 Mode 7 lines on n worker threads, as the Xbox frontend does (the picture must not change).
typedef struct { void (*draw)(Ppu *, int); Ppu *ppu; } Mode7Job;
static void Mode7Item(void *arg, int i) { Mode7Job *j = arg; j->draw(j->ppu, i); }
static void Mode7Parallel(void *ctx, void (*draw)(Ppu *, int), Ppu *ppu, int count) {
  (void)ctx; Mode7Job j = { draw, ppu }; s360_jobs_run(Mode7Item, &j, count);
}
int main(int argc, char **argv) {
  const char *assets = argc > 1 ? argv[1] : "zelda3_assets.dat";
  int frames = argc > 2 ? atoi(argv[2]) : 60;
  const char *dump = argc > 3 ? argv[3] : NULL;
  char err[256];
  if (!ZeldaGameLoadAssets(assets, err, sizeof err)) { fprintf(stderr, "assets: %s\n", err); return 2; }
  ZeldaGameInitialize(0, 0, "us");
  ZeldaGameSetWantedFeatures(getenv("ZFEATURES") ? (uint32)strtoul(getenv("ZFEATURES"), NULL, 0) : 0);
  if (getenv("ZLANG")) ZeldaSetLanguage(getenv("ZLANG"));  // a language in the assets, e.g. pt-br
  int replay = getenv("ZREPLAY") ? atoi(getenv("ZREPLAY")) : -1;  // replay saves/ref chapter N (0-12)
  if (replay >= 0) SaveLoadSlot(kSaveLoad_Replay, 256 + replay);
  int load = getenv("ZLOAD") ? atoi(getenv("ZLOAD")) : -1;  // start from a chapter's state and play it live (features apply)
  if (load >= 0) SaveLoadSlot(kSaveLoad_Load, 256 + load);
  if (getenv("ZDSP_DUMP")) {
    Dsp *d = g_zenv.player->dsp;
    for (int c = 0; c < 8; c++) {
      DspChannel *h = &d->channel[c];
      printf("ch%d pitch %u pc %u pm %d srcn %u doff %u pf %u old %d older %d noise %d rates %u %u %u %u rc %u st %u sus %u ug %d gm %u dg %d gv %u g %u kon %d koff %d so %d vl %d vr %d ee %d dec",
        c, h->pitch, h->pitchCounter, h->pitchModulation, h->srcn, h->decodeOffset, h->previousFlags, h->old, h->older, h->useNoise,
        h->adsrRates[0], h->adsrRates[1], h->adsrRates[2], h->adsrRates[3], h->rateCounter, h->adsrState, h->sustainLevel, h->useGain,
        h->gainMode, h->directGain, h->gainValue, h->gain, h->keyOn, h->keyOff, h->sampleOut, h->volumeL, h->volumeR, h->echoEnable);
      for (int j = 0; j < 19; j++) printf(" %d", h->decodeBuffer[j]);
      printf("\n");
    }
    printf("dsp dir %u ev %d mute %d reset %d mvl %d mvr %d ns %d nr %u nc %u ew %d evl %d evr %d fb %d eba %u ed %u er %u ebi %u fbi %u so %u\n",
      d->dirPage, d->evenCycle, d->mute, d->reset, d->masterVolumeL, d->masterVolumeR, d->noiseSample, d->noiseRate, d->noiseCounter,
      d->echoWrites, d->echoVolumeL, d->echoVolumeR, d->feedbackVolume, d->echoBufferAdr, d->echoDelay, d->echoRemain, d->echoBufferIndex,
      d->firBufferIndex, d->sampleOffset);
    printf("fir"); for (int j = 0; j < 8; j++) printf(" %d %d %d", d->firValues[j], d->firBufferL[j], d->firBufferR[j]); printf("\n");
    printf("ram"); for (int j = 0; j < 0x80; j++) printf(" %02x", d->ram[j]); printf("\n");
    unsigned long long hs = 0; for (int j = 0; j < 534 * 2; j++) hs = hs * 31 + (uint16)d->sampleBuffer[j]; printf("samplebuf %llx\n", hs);
    unsigned long long ha = 0; for (int j = 0; j < 0x10000; j++) ha = ha * 31 + g_zenv.player->ram[j]; printf("aram %llx\n", ha);
  }
  enum { PITCH = 1792 * 4 };
  uint8 *pix = calloc(1024, PITCH);
  if (getenv("ZTHREADS") && s360_jobs_start(atoi(getenv("ZTHREADS")), NULL) > 0) PpuSetMode7Parallel(g_zenv.ppu, Mode7Parallel, NULL);
  if (getenv("ZEXTRA")) g_zenv.ppu->extraLeftRight = (uint8)atoi(getenv("ZEXTRA"));  // widescreen columns each side
  extern uint8 g_ram[131072];
  uint32 render_flags = getenv("ZRENDER_FLAGS") ? (uint32)atoi(getenv("ZRENDER_FLAGS")) : 0;  // 1 = new renderer
  bool audio = getenv("ZAUDIO") != NULL;  // also generate 48 kHz stereo audio each frame, as the Xbox frontend does
  int16 audio_buf[801 * 2];
  FILE *st = getenv("ZSTATE") ? fopen(getenv("ZSTATE"), "wb") : NULL;
  for (int f = 0; f < frames; f++) {
    static int stop_at = -2;
    if (stop_at == -2) stop_at = getenv("ZSTOP") ? atoi(getenv("ZSTOP")) : -1;  // leave the replay here and play live
    if (f == stop_at) PatchCommand('l');
    if (getenv("ZFORCE_FEATURES")) {  // set the features inside a replay too (its inputs may then desync); bytes in LE order
      uint32 v = (uint32)strtoul(getenv("ZFORCE_FEATURES"), NULL, 0);
      for (int i = 0; i < 4; i++) g_ram[0x64c + i] = (uint8)(v >> (8 * i));
    }
    ZeldaGameUpdate(stop_at >= 0 && f >= stop_at ? (getenv("ZSTOP_A") && f % 40 < 3 ? kJoypadL_A : 0) : PadForFrame(f));
    int scale = PpuGetCurrentRenderScale(g_zenv.ppu, render_flags);
    int w = 256 * scale, h = 224 * scale;
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    ZeldaGameRender(pix, PITCH, render_flags);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    static double render_us[5]; static int render_n[5];
    render_us[scale] += (t1.tv_sec - t0.tv_sec) * 1e6 + (t1.tv_nsec - t0.tv_nsec) / 1e3; render_n[scale]++;
    if (getenv("ZTIME") && f == frames - 1)
      for (int k = 1; k <= 4; k *= 4) if (render_n[k]) fprintf(stderr, "render scale %d: %d frames, %.0f us avg\n", k, render_n[k], render_us[k] / render_n[k]);
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
      static FILE *adump; if (!adump && getenv("ZAUDIO_DUMP")) adump = fopen(getenv("ZAUDIO_DUMP"), "wb");
      if (adump) for (int i = 0; i < 801 * 2; i++) { uint16 v = audio_buf[i]; fputc(v & 255, adump); fputc(v >> 8, adump); }
      for (int i = 0; i < 801 * 2; i++) { ha = (ha ^ (uint16)audio_buf[i]) * 0x100000001b3ull; ha ^= ha >> 29; }
    }
    printf("frame %5d scale %d pix %016llx ram %016llx vram %016llx cgoam %016llx sram %016llx audio %016llx\n", f, scale,
           (unsigned long long)hp, (unsigned long long)hr, (unsigned long long)hv, (unsigned long long)hc, (unsigned long long)hs,
           (unsigned long long)ha);
    fflush(stdout);
    int dump_every = getenv("ZDUMP_EVERY") ? atoi(getenv("ZDUMP_EVERY")) : 0;  // also write frame%05d.ppm files
    if (dump_every && f % dump_every == 0) {
      char name[64]; snprintf(name, sizeof name, "frame%05d.ppm", f);
      FILE *o = fopen(name, "wb");
      fprintf(o, "P6\n%d %d\n255\n", w, h);
      for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        uint32 p; memcpy(&p, pix + (size_t)y * PITCH + x * 4, 4);
        fputc((p >> 16) & 255, o); fputc((p >> 8) & 255, o); fputc(p & 255, o);
      }
      fclose(o);
    }
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
