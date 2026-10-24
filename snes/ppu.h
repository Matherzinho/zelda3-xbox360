
#ifndef ZELDA3_SNES_PPU_H_
#define ZELDA3_SNES_PPU_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "snes/saveload.h"
#include "types.h"
typedef struct Ppu Ppu;

#include "src/types.h"

typedef struct BgLayer {
  uint16_t hScroll;
  uint16_t vScroll;
  // -- snapshot starts here
  bool tilemapWider;
  bool tilemapHigher;
  uint16_t tilemapAdr;
  // -- snapshot ends here
  uint16_t tileAdr;
} BgLayer;

enum {
  kPpuXPixels = 256 + kPpuExtraLeftRight * 2,
};

typedef uint16_t PpuZbufType;

typedef struct PpuPixelPrioBufs {
  // This holds the prio in the upper 8 bits and the color in the lower 8 bits.
  PpuZbufType data[kPpuXPixels];
} PpuPixelPrioBufs;

enum {
  kPpuRenderFlags_NewRenderer = 1,
  // Render mode7 upsampled by 4x4
  kPpuRenderFlags_4x4Mode7 = 2,
  // Use 240 height instead of 224
  kPpuRenderFlags_Height240 = 4,
  // Disable sprite render limits
  kPpuRenderFlags_NoSpriteLimits = 8,
};


// One line of the 4x4 Mode 7 picture (kPpuRenderFlags_4x4Mode7), worked out while the frame's lines run and drawn
// then, or later on other threads when a frontend sets Ppu.mode7Parallel: it depends only on these values, the VRAM
// and colorMapRgb, none of which change during the rest of the frame.
typedef struct PpuMode7Line {
  uint8_t *row;             // the first of its four rows in the render buffer
  uint32_t xcur[4], ycur[4], m0[4];
  uint32_t m2;
  uint16_t left_clear, right_clear, draw_width;  // columns blank at each side, columns drawn
  bool half_color, has_sprites;
  uint8_t sprites[kPpuXPixels];  // sprite colour per drawn column, 0 for none
} PpuMode7Line;

// Runs draw(ppu, i) for i in [0, count), in any order and on any threads, and returns when all are done.
typedef void PpuParallelFunc(void *ctx, void (*draw)(struct Ppu *ppu, int i), struct Ppu *ppu, int count);

struct Ppu {
  bool lineHasSprites;
  uint8_t lastBrightnessMult;
  uint8_t lastMosaicModulo;
  uint8_t renderFlags;
  uint32_t renderPitch;
  uint8_t *renderBuffer;
  uint8_t extraLeftCur, extraRightCur, extraLeftRight, extraBottomCur;
  float mode7PerspectiveLow, mode7PerspectiveHigh;

  // TMW / TSW etc
  uint8 screenEnabled[2];
  uint8 screenWindowed[2];
  uint8 mosaicEnabled;
  uint8 mosaicSize;
  // object/sprites
  uint16_t objTileAdr1;
  uint16_t objTileAdr2;
  uint8_t objSize;
  // Window
  uint8_t window1left;
  uint8_t window1right;
  uint8_t window2left;
  uint8_t window2right;
  uint32_t windowsel;

  // color math
  uint8_t clipMode;
  uint8_t preventMathMode;
  bool addSubscreen;
  bool subtractColor;
  bool halfColor;
  uint8 mathEnabled;
  uint8_t fixedColorR, fixedColorG, fixedColorB;
  // settings
  bool forcedBlank;
  uint8_t brightness;
  uint8_t mode;

  // vram access
  uint16_t vramPointer;
  uint16_t vramIncrement;
  bool vramIncrementOnHigh;
  // cgram access
  uint8_t cgramPointer;
  bool cgramSecondWrite;
  uint8_t cgramBuffer;
  // oam access
  uint16_t oamAdr;
  bool oamSecondWrite;
  uint8_t oamBuffer;

  // background layers
  BgLayer bgLayer[4];
  uint8_t scrollPrev;
  uint8_t scrollPrev2;
  
  // mode 7
  int16_t m7matrix[8]; // a, b, c, d, x, y, h, v
  uint8_t m7prev;
  bool m7largeField;
  bool m7charFill;
  bool m7xFlip;
  bool m7yFlip;
  bool m7extBg_always_zero;
  // mode 7 internal
  int32_t m7startX;
  int32_t m7startY;

  zle16_t oam[0x110];  // little-endian, as SNES OAM
  
  // store 31 extra entries to remove the need for clamp
  uint8_t brightnessMult[32 + 31];
  uint8_t brightnessMultHalf[32 * 2];
  zle16_t cgram[0x100];  // little-endian, as SNES CGRAM
  uint8_t mosaicModulo[kPpuXPixels];
  uint32_t colorMapRgb[256];
  PpuPixelPrioBufs bgBuffers[2];
  PpuPixelPrioBufs objBuffer;
  zle16_t vram[0x8000];  // little-endian, as SNES VRAM

  // Deferred 4x4 Mode 7 lines (not saved in snapshots): with no mode7Parallel each line is drawn at once.
  PpuParallelFunc *mode7Parallel;
  void *mode7ParallelCtx;
  PpuMode7Line *mode7Lines;
  int mode7LineCount;
};

Ppu* ppu_init();
void ppu_free(Ppu* ppu);
void ppu_reset(Ppu* ppu);
void ppu_handleVblank(Ppu* ppu);
void ppu_runLine(Ppu* ppu, int line);
uint8_t ppu_read(Ppu* ppu, uint8_t adr);
void ppu_write(Ppu* ppu, uint8_t adr, uint8_t val);
void ppu_saveload(Ppu *ppu, SaveLoadFunc *func, void *ctx);
void PpuBeginDrawing(Ppu *ppu, uint8_t *buffer, size_t pitch, uint32_t render_flags);
// Finishes the lines PpuBeginDrawing started; the frame is complete in the buffer after it.
void PpuEndDrawing(Ppu *ppu);
// Lets the 4x4 Mode 7 lines of a frame be drawn by parallel(ctx, ...) at the end of the frame; NULL draws them in turn.
void PpuSetMode7Parallel(Ppu *ppu, PpuParallelFunc *parallel, void *ctx);

// Returns the current render scale, 1x = 256px, 2x=512px, 4x=1024px
int PpuGetCurrentRenderScale(Ppu *ppu, uint32_t render_flags);

void PpuSetMode7PerspectiveCorrection(Ppu *ppu, int low, int high);
void PpuSetExtraSideSpace(Ppu *ppu, int left, int right, int bottom);

#endif  // ZELDA3_SNES_PPU_H_
