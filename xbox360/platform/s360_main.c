// The platform's main loop: start-up, the 60 Hz frame loop, the menu, the frame-time log and shutdown.
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include "s360_internal.h"
#include "s360_jobs.h"
#include "xsdk/kernel.h"
#include "xsdk/log.h"
#include "xsdk/splash.h"

// Start-up colour codes, so a failure shows without the log: blue = video up, green = game loaded, red = failed.
enum { kColorVideo = 0x0000FF, kColorLoaded = 0x00FF00, kColorFailed = 0xFF0000 };

void s360_log(const char *format, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, format);
  vsnprintf(buf, sizeof buf, format, ap);
  va_end(ap);
  xsdk_log_write("%s", buf);
}

static const char *const kAspectChoices[] = { "TV 4:3", "Quadrado", "Inteiro", "Esticar", NULL };
static const char *const kNoYes[] = { "N\xC3\xA3o", "Sim", NULL };
static S360Setting g_platform[] = {
  { "aspect", "Propor\xC3\xA7\xC3\xA3o", kAspectChoices, 0, kS360Aspect_TV, false },
  { "show_fps", "Mostrar FPS", kNoYes, 0, 0, false },
};
enum { kNumPlatform = sizeof g_platform / sizeof g_platform[0] };

static uint64_t NowUs(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
}
typedef struct { uint64_t sum, max; } Stat;
static void StatAdd(Stat *s, uint64_t v) { s->sum += v; if (v > s->max) s->max = v; }

static void Apply(const S360Game *game) {
  s360_audio_lock();
  game->apply_settings((S360Aspect)g_platform[0].value);
  s360_audio_unlock();
}

int s360_run(const S360Game *game) {
  s360_log("%s: start image=%s", game->name, ExLoadedImageName ? ExLoadedImageName : "(unknown)");
  xsdk_splash_show(1500);
  if (!s360_video_open(game->max_width, game->max_height)) {
    s360_log("%s: video failed", game->name);
    return 1;
  }
  // Workers for per-frame work, on the other two cores and the second thread of this one (audio is on 2).
  static const int kWorkerThreads[] = { 4, 5, 3 };
  s360_log("s360: %d worker threads", s360_jobs_start(3, kWorkerThreads));
  s360_video_show_color(kColorVideo, 20, 256, 224);
  char error[256] = "";
  if (!game->init(error, sizeof error)) {
    s360_log("%s: init failed: %s", game->name, error);
    s360_video_show_color(kColorFailed, 300, 256, 224);
    s360_video_close();
    return 2;
  }
  s360_video_show_color(kColorLoaded, 20, 256, 224);
  s360_settings_load(game, g_platform, kNumPlatform);
  s360_menu_init(game, g_platform, kNumPlatform);
  s360_audio_start(game);
  Apply(game);
  s360_log("%s: running (right stick: menu, hold Back+Start: quit)", game->name);

  int frame = 0, quit_hold = 0, fps_shown = 0;
  bool dirty = false;
  Stat st_run = { 0, 0 }, st_draw = { 0, 0 }, st_present = { 0, 0 }, st_frame = { 0, 0 };
  uint64_t t_last = NowUs(), t_fps = t_last;
  for (;;) {
    S360Pad pad;
    s360_input_poll(&pad);
    const uint16_t quit_mask = INPUT_GAMEPAD_BACK | INPUT_GAMEPAD_START;
    quit_hold = (pad.held & quit_mask) == quit_mask ? quit_hold + 1 : 0;
    if (quit_hold >= 60) {
      s360_log("%s: quit (Back+Start)", game->name);
      break;
    }
    S360MenuResult menu = s360_menu_update(&pad);
    if (menu == kS360Menu_Changed) { Apply(game); dirty = true; }
    if (menu == kS360Menu_Quit) { s360_log("%s: quit (menu)", game->name); break; }
    if (menu == kS360Menu_Closed && dirty) { s360_settings_save(game, g_platform, kNumPlatform); dirty = false; }

    uint64_t t0 = NowUs();
    if (!s360_menu_is_open()) {  // the game is paused while the menu is open
      s360_audio_lock();
      game->run_frame(pad.snes);
      s360_audio_unlock();
    }
    uint64_t t1 = NowUs();
    int pitch, w = 0, h = 0;
    uint32_t *px = s360_video_pixels(&pitch);
    game->draw(px, pitch, &w, &h);
    if (s360_menu_is_open()) s360_menu_draw(px, pitch, w, h);
    if (g_platform[1].value) {
      char fps[16];
      snprintf(fps, sizeof fps, "%d", fps_shown);
      int k = h >= 448 ? h / 224 : 1;
      s360_draw_set_scale(k);
      s360_draw_text(px, pitch, w, w / k - 4 - s360_text_width(fps), 4, fps, 0x40F040);
      s360_draw_set_scale(1);
    }
    uint64_t t2 = NowUs();
    s360_video_present(w, h, g_platform[0].value);
    uint64_t t3 = NowUs();
    StatAdd(&st_run, t1 - t0); StatAdd(&st_draw, t2 - t1); StatAdd(&st_present, t3 - t2); StatAdd(&st_frame, t3 - t_last);
    t_last = t3;
    if (++frame % 60 == 0) {  // frames a second over the last 60 frames, rounded
      fps_shown = (int)((60000000ull + (t3 - t_fps) / 2) / (t3 - t_fps));
      t_fps = t3;
    }
    if (frame % 600 == 0) {
      // Microseconds per frame, average/max; 16683 us is the SNES's 59.94 frames a second.
      s360_log("%s: perf frames=%d run=%u/%u draw=%u/%u present=%u/%u frame=%u/%u (us avg/max) fps=%u.%u picture=%dx%d",
               game->name, frame, (unsigned)(st_run.sum / 600), (unsigned)st_run.max, (unsigned)(st_draw.sum / 600),
               (unsigned)st_draw.max, (unsigned)(st_present.sum / 600), (unsigned)st_present.max,
               (unsigned)(st_frame.sum / 600), (unsigned)st_frame.max, (unsigned)(600000000ull / st_frame.sum),
               (unsigned)(6000000000ull / st_frame.sum % 10), w, h);
      st_run = st_draw = st_present = st_frame = (Stat){ 0, 0 };
    }
  }
  if (dirty) s360_settings_save(game, g_platform, kNumPlatform);
  if (game->shutdown) game->shutdown();
  s360_video_close();
  s360_log("%s: exit after %d frames", game->name, frame);
  return 0;
}
