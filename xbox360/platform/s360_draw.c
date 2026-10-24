// Text and boxes drawn into the frame before it is presented, for the menu and the FPS counter.
#include "s360_internal.h"
#include "s360_font.h"

// Coordinates are in units of g_scale pixels, so overlays keep their size on the 4x world map.
static int g_scale = 1;
void s360_draw_set_scale(int scale) { g_scale = scale < 1 ? 1 : scale; }

void s360_draw_rect(uint32_t *px, int pitch, int x, int y, int w, int h, uint32_t rgb) {
  x *= g_scale, y *= g_scale, w *= g_scale, h *= g_scale;
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++) px[(size_t)(y + j) * pitch + x + i] = rgb;
}

void s360_draw_dim(uint32_t *px, int pitch, int x, int y, int w, int h) {
  x *= g_scale, y *= g_scale, w *= g_scale, h *= g_scale;
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++) {
      uint32_t *p = &px[(size_t)(y + j) * pitch + x + i];
      *p = (*p >> 3) & 0x1F1F1F;
    }
}

// Next code point of a UTF-8 string; anything outside Latin-1 becomes '?'.
static unsigned NextChar(const char **s) {
  const unsigned char *p = (const unsigned char *)*s;
  unsigned c = *p++;
  if (c >= 0xC0 && c < 0xE0 && (*p & 0xC0) == 0x80) c = (c & 0x1F) << 6 | (*p++ & 0x3F);
  else if (c >= 0x80) { while ((*p & 0xC0) == 0x80) p++; c = '?'; }
  *s = (const char *)p;
  return c > 0xFF ? '?' : c;
}

int s360_text_width(const char *s) {
  int n = 0;
  while (*s) { NextChar(&s); n++; }
  return n * 8;
}

// Draws with a one-pixel shadow; returns the x after the text.
int s360_draw_text(uint32_t *px, int pitch, int clip_w, int x, int y, const char *s, uint32_t rgb) {
  const int k = g_scale;
  while (*s) {
    const uint8_t *g = kS360Font[NextChar(&s)];
    for (int row = 0; row < 8; row++)
      for (int col = 0; col < 8; col++) {
        if (!(g[row] >> col & 1)) continue;
        int x0 = (x + col) * k, y0 = (y + row) * k;
        for (int j = 0; j < k; j++)
          for (int i = 0; i < k; i++) {
            if (x0 + i + k < clip_w) px[(size_t)(y0 + j + k) * pitch + x0 + i + k] = 0;  // shadow
            if (x0 + i < clip_w) px[(size_t)(y0 + j) * pitch + x0 + i] = rgb;
          }
      }
    x += 8;
  }
  return x;
}
