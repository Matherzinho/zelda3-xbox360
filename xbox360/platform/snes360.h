// snes360: a small Xbox 360 platform layer for ports of SNES games, built on xsdk and SDL-xenon.
//
// A port describes its game with an S360Game (how to run a frame, draw it, render audio, and which settings it
// has) and calls s360_run(). The platform owns everything console-specific: the GPU presentation and display modes,
// audio output, the controller, the settings file, the in-game options menu (right stick click), the start-up
// colour codes and the frame-time log. Nothing here knows about a particular game.
#ifndef SNES360_H_
#define SNES360_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The controller as a SNES pad, in the bit order zelda3's frontends use (and the order SNES games read after
// reversing the joypad register): B, Y, Select, Start, Up, Down, Left, Right, A, X, L, R.
enum {
  kS360_B = 1 << 0, kS360_Y = 1 << 1, kS360_Select = 1 << 2, kS360_Start = 1 << 3,
  kS360_Up = 1 << 4, kS360_Down = 1 << 5, kS360_Left = 1 << 6, kS360_Right = 1 << 7,
  kS360_A = 1 << 8, kS360_X = 1 << 9, kS360_L = 1 << 10, kS360_R = 1 << 11,
};

// One option in the menu and the settings file. `value` is an index into `choices` and is what the game reads.
typedef struct S360Setting {
  const char *key;               // name in the settings file
  const char *label;             // shown in the menu (UTF-8; Latin-1 letters such as ç and ã are drawn)
  const char *const *choices;    // shown values; NULL-terminated
  int value;                     // current choice
  int def;                       // default choice (the original game where it applies)
  bool restart_needed;           // the change applies the next time the game starts
} S360Setting;

// How the picture is shaped on the 16:9 display.
typedef enum S360Aspect {
  kS360Aspect_TV = 0,        // as on a 4:3 television: SNES pixels 8:7 wide (default)
  kS360Aspect_Square = 1,    // square pixels, full height
  kS360Aspect_Integer = 2,   // square pixels at a whole scale (sharpest)
  kS360Aspect_Stretch = 3,   // fill the 16:9 screen
} S360Aspect;

typedef struct S360Game {
  const char *name;                 // short name: log prefix and settings file (<name>.ini beside the .xex)
  const char *title;                // shown at the top of the menu
  int max_width, max_height;        // largest frame the game draws, in pixels

  // Loads data and sets the game up. Returns false with a message on failure (the screen turns red).
  bool (*init)(char *error, size_t error_size);
  // Called after the settings are loaded and whenever the menu changes one. `aspect` is the display mode, so the
  // game can widen its picture to fill it. The audio lock is held.
  void (*apply_settings)(S360Aspect aspect);
  // Runs one 1/60 s frame with the pad's buttons (kS360_*). The audio lock is held.
  void (*run_frame)(uint16_t buttons);
  // Draws the frame as 0x00RRGGBB pixels, `pitch` pixels a row, and returns its size.
  void (*draw)(uint32_t *pixels, int pitch, int *width, int *height);
  // Fills `frames` stereo frames of 16-bit audio at s360_audio_rate(). Called from the audio thread with the
  // audio lock held. May be NULL for a silent game.
  void (*render_audio)(int16_t *samples, int frames);
  // Called when the player leaves the game (saves already happen in game; this is for anything else).
  void (*shutdown)(void);

  S360Setting *settings;
  int num_settings;
} S360Game;

// Runs the game until the player quits. Returns the process exit status.
int s360_run(const S360Game *game);

// The audio lock: held around run_frame/apply_settings and by the audio thread around render_audio.
void s360_audio_lock(void);
void s360_audio_unlock(void);
int s360_audio_rate(void);

// Writes a line to the title's log (game:\<xex name>.log).
void s360_log(const char *format, ...) __attribute__((format(printf, 1, 2)));

#ifdef __cplusplus
}
#endif
#endif  // SNES360_H_
