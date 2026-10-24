// hqx's colour table (see hqx.h), for the 32768 colours of 5 bits a channel, each read at the middle of its range.
#include "hqx.h"
#include "common.h"

uint32_t RGBtoYUV[32768];

void hqxInit(void) {
  for (uint32_t c = 0; c < 32768; c++) {
    uint32_t r = (c >> 10 & 31) << 3 | 4, g = (c >> 5 & 31) << 3 | 4, b = (c & 31) << 3 | 4;
    uint32_t y = (uint32_t)(0.299 * r + 0.587 * g + 0.114 * b);
    // Through int: hqx casts the negative values straight to unsigned, which C leaves undefined.
    uint32_t u = (uint32_t)((int)(-0.169 * r - 0.331 * g + 0.5 * b) + 128);
    uint32_t v = (uint32_t)((int)(0.5 * r - 0.419 * g - 0.081 * b) + 128);
    RGBtoYUV[c] = (y << 16) + (u << 8) + v;
  }
}
