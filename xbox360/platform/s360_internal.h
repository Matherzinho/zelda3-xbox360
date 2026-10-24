// snes360 internals shared between its modules.
#ifndef S360_INTERNAL_H_
#define S360_INTERNAL_H_

#include "snes360.h"

// Video (s360_video.c)
bool s360_video_open(int max_w, int max_h);
uint32_t *s360_video_pixels(int *pitch);
// The display filters (kS360Filter_*) and scanline strengths the settings offer.
enum { kS360Filter_Nearest, kS360Filter_Bilinear, kS360Filter_SharpBilinear, kS360Filter_Hq2x, kS360Filter_Hq4x };
enum { kS360Scanlines_Off, kS360Scanlines_Light, kS360Scanlines_Medium, kS360Scanlines_Strong };
typedef struct S360Display { int aspect, filter, scanlines; } S360Display;
void s360_video_present(int w, int h, const S360Display *display);
// Microseconds the last present spent filtering and uploading the picture, before it waited for the display.
uint32_t s360_video_prepare_us(void);
void s360_video_show_color(uint32_t rgb, int frames, int w, int h);
void s360_video_close(void);

// Audio (s360_audio.c)
void s360_audio_start(const S360Game *game);

// Input (s360_input.c): the pad's Xbox buttons this frame, the ones newly pressed, and the SNES mapping.
typedef struct S360Pad {
  uint16_t held, pressed;   // INPUT_GAMEPAD_* bits; left stick folded into the D-pad bits
  uint16_t snes;            // kS360_* bits
} S360Pad;
void s360_input_poll(S360Pad *pad);

// Settings (s360_settings.c): <name>.ini beside the .xex, "key = choice index" lines.
void s360_settings_load(const S360Game *game, S360Setting *platform, int num_platform);
void s360_settings_save(const S360Game *game, const S360Setting *platform, int num_platform);

// Drawing over the frame (s360_draw.c): 8x8 Latin-1 text and boxes, in 0x00RRGGBB.
void s360_draw_rect(uint32_t *px, int pitch, int x, int y, int w, int h, uint32_t rgb);
void s360_draw_set_scale(int scale);  // later coordinates and sizes are in units of this many pixels
void s360_draw_dim(uint32_t *px, int pitch, int x, int y, int w, int h);  // darkens to 1/8
int s360_draw_text(uint32_t *px, int pitch, int clip_w, int x, int y, const char *utf8, uint32_t rgb);
int s360_text_width(const char *utf8);

// Menu (s360_menu.c)
typedef enum S360MenuResult { kS360Menu_Open, kS360Menu_Closed, kS360Menu_Changed, kS360Menu_Quit } S360MenuResult;
void s360_menu_init(const S360Game *game, S360Setting *platform, int num_platform);
S360MenuResult s360_menu_update(const S360Pad *pad);
void s360_menu_draw(uint32_t *px, int pitch, int w, int h);
bool s360_menu_is_open(void);

#endif  // S360_INTERNAL_H_
