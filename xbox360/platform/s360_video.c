// Presents the game's frame on the 720p display with xsdk's GPU layer: the frame is uploaded into a texture and drawn
// as one quad shaped by the display mode. Pixels are 0x00RRGGBB; an A8R8G8B8 texture is sampled as the CPU's own
// 0xAARRGGBB words (checked on the console: byte-swapping them rotated the channels), so each gets an opaque alpha.
#include <stdlib.h>
#include <string.h>
#include "s360_internal.h"
#include "snes360_shaders.h"  // generated from blit.vert and blit.frag (xsdk_add_shaders)
#include "xsdk/gpu.h"
#include "xsdk/log.h"
#include "xsdk/present.h"
#include "xsdk/shader.h"
#include "xsdk/texture.h"

enum { kBuffers = 2, kSwapInterval = 1, kBeginTimeoutMs = 5000, kSlots = 2, kStride = 16 };
static const XsdkVertexElement kLayout[] = {
  { .usage = XSDK_USAGE_POSITION, .format = XSDK_VERTEX_FLOAT2, .offset_bytes = 0 },
  // xsdk maps vertex inputs by location: 2 is TEXCOORD0 (blit.vert).
  { .usage = XSDK_USAGE_TEXCOORD, .format = XSDK_VERTEX_FLOAT2, .offset_bytes = 8 },
};
typedef struct { float x, y, u, v; } Vertex;

// Each slot is a texture and its quad, used every other frame, so the CPU never writes what the GPU may still read.
typedef struct Slot {
  XsdkTexture texture;
  volatile uint32_t *vertices;
  uint32_t fence;
  int quad_w, quad_h, quad_aspect;  // what the vertices were written for
} Slot;

static XsdkShader g_vs, g_ps;
static XsdkSamplerState g_sampler;
static Slot g_slots[kSlots];
static int g_slot;
static uint32_t *g_pixels;        // the game draws here
static uint32_t g_tex_w, g_tex_h; // texture size: the game's largest frame rounded up to 32
static uint32_t g_dw, g_dh;       // display size
static bool g_open;

static uint32_t RoundUp32(int v) { return (uint32_t)(v + 31) & ~31u; }

bool s360_video_open(int max_w, int max_h) {
  g_tex_w = RoundUp32(max_w);
  g_tex_h = RoundUp32(max_h);
  s360_log("s360: video open max=%dx%d texture=%ux%u", max_w, max_h, (unsigned)g_tex_w, (unsigned)g_tex_h);
  NTSTATUS s = xsdk_gpu_open();
  if (!NT_SUCCESS(s)) { s360_log("s360: gpu_open failed status=0x%08X", (unsigned)s); return false; }
  s = xsdk_present_open(XSDK_DISPLAY_720P, kBuffers, kSwapInterval, &g_dw, &g_dh);
  if (!NT_SUCCESS(s)) { s360_log("s360: present_open failed status=0x%08X", (unsigned)s); return false; }
  s360_log("s360: display %ux%u", (unsigned)g_dw, (unsigned)g_dh);
  s = xsdk_shader_load(&g_vs, blit_vert.container, blit_vert.container_bytes);
  if (NT_SUCCESS(s)) s = xsdk_shader_load(&g_ps, blit_frag.container, blit_frag.container_bytes);
  if (NT_SUCCESS(s)) s = xsdk_shader_set_vertex_layout(&g_vs, kLayout, 2, (const uint32_t[]){ kStride });
  if (!NT_SUCCESS(s)) { s360_log("s360: shader setup failed status=0x%08X", (unsigned)s); return false; }
  for (int i = 0; i < kSlots; i++) {
    Slot *sl = &g_slots[i];
    s = xsdk_texture_create(&sl->texture, XSDK_FORMAT_A8R8G8B8, g_tex_w, g_tex_h, 1, true);
    if (!NT_SUCCESS(s)) { s360_log("s360: texture_create failed status=0x%08X", (unsigned)s); return false; }
    sl->vertices = xsdk_gpu_alloc(4 * sizeof(Vertex));
    if (!sl->vertices) { s360_log("s360: gpu_alloc failed"); return false; }
    sl->fence = 0;
    sl->quad_w = -1;
  }
  const XsdkSampler sm = { .mag_filter = XSDK_FILTER_POINT, .min_filter = XSDK_FILTER_POINT,
                           .mip_filter = XSDK_FILTER_POINT, .address_u = XSDK_ADDRESS_CLAMP,
                           .address_v = XSDK_ADDRESS_CLAMP };
  xsdk_texture_sampler_state(&g_sampler, &sm);
  g_pixels = calloc((size_t)g_tex_w * g_tex_h, 4);
  if (!g_pixels) { s360_log("s360: frame buffer allocation failed"); return false; }
  g_open = true;
  return true;
}

uint32_t *s360_video_pixels(int *pitch) {
  *pitch = (int)g_tex_w;
  return g_pixels;
}

// The quad, in clip space, for a w x h frame in the given display mode.
static void QuadSize(int w, int h, int aspect, float *half_w, float *half_h) {
  float dw = (float)g_dw, dh = (float)g_dh, out_w, out_h;
  switch (aspect) {
  case kS360Aspect_Stretch:
    out_w = dw, out_h = dh;
    break;
  case kS360Aspect_Integer: {
    int scale = (int)(dh / (float)h);
    if (scale < 1) scale = 1;
    while (scale > 1 && w * scale > (int)g_dw) scale--;
    out_w = (float)(w * scale), out_h = (float)(h * scale);
    break;
  }
  case kS360Aspect_Square:
    out_h = dh, out_w = dh * (float)w / (float)h;
    break;
  default:  // kS360Aspect_TV: 256 SNES pixels span a 4:3 picture's width
    out_h = dh, out_w = dh * 4.0f / 3.0f * (float)w / 256.0f * 224.0f / (float)h;
    break;
  }
  if (out_w > dw) { out_h *= dw / out_w; out_w = dw; }
  if (out_h > dh) { out_w *= dh / out_h; out_h = dh; }  // e.g. a 4x world map in whole-scale mode
  *half_w = out_w / dw;
  *half_h = out_h / dh;
}

static void WriteQuad(Slot *sl, int w, int h, int aspect) {
  float hw, hh;
  QuadSize(w, h, aspect, &hw, &hh);
  float u = (float)w / (float)g_tex_w, v = (float)h / (float)g_tex_h;
  // Clip space y up; v = 0 is the texture's first row, at the top.
  const Vertex quad[4] = { { -hw, -hh, 0, v }, { hw, -hh, u, v }, { -hw, hh, 0, 0 }, { hw, hh, u, 0 } };
  // GPU memory takes 32-bit volatile stores only (xsdk docs/PITFALLS.md, "Plain stores to GPU memory").
  for (unsigned i = 0; i < sizeof quad / 4; i++) {
    uint32_t word;
    memcpy(&word, (const uint8_t *)quad + i * 4, 4);
    sl->vertices[i] = word;
  }
  xsdk_gpu_invalidate(xsdk_gpu_address(sl->vertices), sizeof quad);
  sl->quad_w = w, sl->quad_h = h, sl->quad_aspect = aspect;
  s360_log("s360: picture %dx%d mode %d -> %dx%d on screen", w, h, aspect, (int)(hw * g_dw + 0.5f), (int)(hh * g_dh + 0.5f));
}

void s360_video_present(int w, int h, int aspect) {
  if (!g_open) return;
  Slot *sl = &g_slots[g_slot];
  g_slot = (g_slot + 1) % kSlots;
  if (sl->fence) xsdk_gpu_wait(sl->fence, kBeginTimeoutMs);  // the GPU is done with this slot's texture and quad
  for (int y = 0; y < h; y++) {
    uint32_t *row = g_pixels + (size_t)y * g_tex_w;
    for (int x = 0; x < w; x++) row[x] |= 0xFF000000u;
  }
  xsdk_texture_upload_rows(&sl->texture, 0, 0, (uint32_t)h, g_pixels, g_tex_w * 4);
  xsdk_gpu_invalidate(sl->texture.address, sl->texture.size_bytes);
  if (sl->quad_w != w || sl->quad_h != h || sl->quad_aspect != aspect) WriteQuad(sl, w, h, aspect);

  uint32_t address;
  NTSTATUS status = xsdk_present_begin(&address, kBeginTimeoutMs);
  if (!NT_SUCCESS(status)) {
    s360_log("s360: present_begin failed status=0x%08X", (unsigned)status);
    return;
  }
  xsdk_gpu_set_target(g_dw, g_dh);
  xsdk_gpu_set_shaders(&g_vs, &g_ps);
  xsdk_built_shader_set_literals(&blit_vert);
  xsdk_built_shader_set_literals(&blit_frag);
  xsdk_gpu_set_texture(BLIT_FRAG_SAMPLER_0_0, &sl->texture, &g_sampler);
  xsdk_gpu_set_vertices(0, xsdk_gpu_address(sl->vertices), 4 * sizeof(Vertex));
  xsdk_gpu_draw(XSDK_PRIMITIVE_TRIANGLE_STRIP, 4);
  xsdk_gpu_resolve(address, g_dw, g_dh, 0xFF000000);  // black borders
  xsdk_present_end();
  sl->fence = xsdk_gpu_fence();
}

// A full frame of one colour, for the start-up colour codes.
void s360_video_show_color(uint32_t rgb, int frames, int w, int h) {
  for (int i = 0; i < frames; i++) {
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++) g_pixels[(size_t)y * g_tex_w + x] = rgb & 0xFFFFFF;
    s360_video_present(w, h, kS360Aspect_TV);
  }
}

void s360_video_close(void) {
  if (!g_open) return;
  for (int i = 0; i < kSlots; i++) {
    Slot *sl = &g_slots[i];
    if (sl->fence) xsdk_gpu_wait(sl->fence, kBeginTimeoutMs);
    if (sl->vertices) xsdk_gpu_free(sl->vertices);
    if (sl->texture.texels) xsdk_texture_free(&sl->texture);
  }
  free(g_pixels);
  g_pixels = NULL;
  xsdk_present_close();
  xsdk_gpu_close();
  xsdk_shader_free(&g_ps);
  xsdk_shader_free(&g_vs);
  g_open = false;
}
