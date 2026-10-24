// hqx by Maxim Stepin, from Cameron Zemek's hqx (https://github.com/grom358/hqx), under the GNU LGPL 2.1 (COPYING).
// Changed for snes360: each scaler draws rows first_row to end_row - 1 of the source, so threads can share a frame.
#ifndef SNES360_HQX_H
#define SNES360_HQX_H
#include <stdint.h>

#define HQX_API
#define HQX_CALLCONV

// Fills the colour table; once, before the scalers run.
void hqxInit(void);
// srb and drb are the source's and destination's bytes a row; the destination is 2 or 4 times the source each way.
void hq2x_32_rows(const uint32_t *src, uint32_t srb, uint32_t *dst, uint32_t drb, int width, int height, int first_row,
                  int end_row);
void hq4x_32_rows(const uint32_t *src, uint32_t srb, uint32_t *dst, uint32_t drb, int width, int height, int first_row,
                  int end_row);

#endif
