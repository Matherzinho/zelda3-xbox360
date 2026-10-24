// Presents the game's frame on the 720p display with xsdk's GPU layer: the frame is uploaded into a texture and drawn
// as one quad shaped by the display mode. Pixels are 0x00RRGGBB; an A8R8G8B8 texture is sampled as the CPU's own
// 0xAARRGGBB words (checked on the console: byte-swapping them rotated the channels), and its alpha reads as 1.
// Each texture is the picture's own size rounded up to 32, so an upload moves only the picture: uploads go a whole
// texture row at a time, and are shared out between the worker threads (s360_jobs.c) in bands of tiles.
//
// Filters: the pixel shader (blit.frag) draws sharp bilinear, plain bilinear or sharp pixels, and the scanlines; HQ2x
// and HQ4x first enlarge the picture on the worker threads (hqx/), and the shader then draws that sharp bilinear.
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "s360_internal.h"
#include "s360_jobs.h"
#include "hqx/hqx.h"
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
  XsdkTexture texture;      // tex_w x tex_h, made for the picture size when it changes
  uint32_t tex_w, tex_h;
  float screen_w, screen_h;  // the quad's size in screen pixels
  volatile uint32_t *vertices;
  uint32_t fence;
  int quad_w, quad_h, quad_div, quad_aspect;  // what the vertices were written for
} Slot;

static XsdkShader g_vs, g_ps;
static XsdkSamplerState g_point, g_linear;
static uint32_t *g_scaled;          // HQ2x and HQ4x output, g_pitch pixels a row, made the first time either is used
static bool g_hqx_ready;
enum { kScaledRows = 960 };         // 240 lines times 4
static Slot g_slots[kSlots];
static int g_slot;
static uint32_t *g_pixels;        // the game draws here, g_pitch pixels a row
static uint32_t g_pitch, g_max_h; // the game's largest frame, rounded up to 32
static uint32_t g_dw, g_dh;       // display size
static bool g_open;

static uint32_t RoundUp32(int v) { return (uint32_t)(v + 31) & ~31u; }

bool s360_video_open(int max_w, int max_h) {
  g_pitch = RoundUp32(max_w);
  g_max_h = RoundUp32(max_h);
  s360_log("s360: video open max=%dx%d", max_w, max_h);
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
    sl->tex_w = sl->tex_h = 0;  // made by the first present
    sl->vertices = xsdk_gpu_alloc(4 * sizeof(Vertex));
    if (!sl->vertices) { s360_log("s360: gpu_alloc failed"); return false; }
    sl->fence = 0;
    sl->quad_w = -1;
  }
  const XsdkSampler point = { .mag_filter = XSDK_FILTER_POINT, .min_filter = XSDK_FILTER_POINT,
                              .mip_filter = XSDK_FILTER_POINT, .address_u = XSDK_ADDRESS_CLAMP,
                              .address_v = XSDK_ADDRESS_CLAMP };
  XsdkSampler linear = point;
  linear.mag_filter = linear.min_filter = XSDK_FILTER_LINEAR;
  xsdk_texture_sampler_state(&g_point, &point);
  xsdk_texture_sampler_state(&g_linear, &linear);
  g_pixels = calloc((size_t)g_pitch * g_max_h, 4);
  if (!g_pixels) { s360_log("s360: frame buffer allocation failed"); return false; }
  g_open = true;
  return true;
}

uint32_t *s360_video_pixels(int *pitch) {
  *pitch = (int)g_pitch;
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

// The quad for a w x h texture holding the picture at div times the console's resolution (the 4x map, the HQ filters):
// its size on screen is the console resolution's, so every filter shows the picture at the same size.
static void WriteQuad(Slot *sl, int w, int h, int div, int aspect) {
  float hw, hh;
  QuadSize(w / div, h / div, aspect, &hw, &hh);
  float u = (float)w / (float)sl->tex_w, v = (float)h / (float)sl->tex_h;
  // Clip space y up; v = 0 is the texture's first row, at the top.
  const Vertex quad[4] = { { -hw, -hh, 0, v }, { hw, -hh, u, v }, { -hw, hh, 0, 0 }, { hw, hh, u, 0 } };
  // GPU memory takes 32-bit volatile stores only (xsdk docs/PITFALLS.md, "Plain stores to GPU memory").
  for (unsigned i = 0; i < sizeof quad / 4; i++) {
    uint32_t word;
    memcpy(&word, (const uint8_t *)quad + i * 4, 4);
    sl->vertices[i] = word;
  }
  xsdk_gpu_invalidate(xsdk_gpu_address(sl->vertices), sizeof quad);
  sl->quad_w = w, sl->quad_h = h, sl->quad_div = div, sl->quad_aspect = aspect;
  sl->screen_w = hw * (float)g_dw, sl->screen_h = hh * (float)g_dh;
  s360_log("s360: picture %dx%d mode %d -> %dx%d on screen", w, h, aspect, (int)(hw * g_dw + 0.5f), (int)(hh * g_dh + 0.5f));
}

// A texture for a w x h picture; the slot's old one goes once the GPU is done with it.
static bool SizeTexture(Slot *sl, int w, int h) {
  uint32_t tw = RoundUp32(w), th = RoundUp32(h);
  if (sl->tex_w == tw && sl->tex_h == th) return true;
  if (sl->fence) xsdk_gpu_wait(sl->fence, kBeginTimeoutMs), sl->fence = 0;
  if (sl->texture.texels) xsdk_texture_free(&sl->texture);
  sl->tex_w = sl->tex_h = 0;
  NTSTATUS s = xsdk_texture_create(&sl->texture, XSDK_FORMAT_A8R8G8B8, tw, th, 1, true);
  if (!NT_SUCCESS(s)) {
    s360_log("s360: texture_create %ux%u failed status=0x%08X", (unsigned)tw, (unsigned)th, (unsigned)s);
    return false;
  }
  static const uint32_t kOpaque[4] = { XSDK_CHANNEL_RED, XSDK_CHANNEL_GREEN, XSDK_CHANNEL_BLUE, XSDK_CHANNEL_ONE };
  xsdk_texture_swizzle(&sl->texture, kOpaque);  // the frame's top byte is not alpha
  sl->tex_w = tw, sl->tex_h = th;
  sl->quad_w = -1;
  return true;
}

enum { kBandRows = 32 };  // a row of tiles
typedef struct { XsdkTexture *texture; uint32_t rows; const uint32_t *pixels; } Upload;
static void UploadBand(void *arg, int band) {
  const Upload *u = arg;
  uint32_t first = (uint32_t)band * kBandRows, n = u->rows - first < kBandRows ? u->rows - first : kBandRows;
  xsdk_texture_upload_rows(u->texture, 0, first, n, u->pixels + (size_t)first * g_pitch, g_pitch * 4);
}

// HQ2x or HQ4x of the w x h picture into g_scaled, in bands of rows on the worker threads.
enum { kHqBandRows = 16 };
typedef struct { int factor, w, h; } Scale;
static void ScaleBand(void *arg, int band) {
  const Scale *sc = arg;
  int first = band * kHqBandRows, end = first + kHqBandRows < sc->h ? first + kHqBandRows : sc->h;
  if (sc->factor == 2)
    hq2x_32_rows(g_pixels, g_pitch * 4, g_scaled, g_pitch * 4, sc->w, sc->h, first, end);
  else
    hq4x_32_rows(g_pixels, g_pitch * 4, g_scaled, g_pitch * 4, sc->w, sc->h, first, end);
}

// Enlarges the picture for the HQ filters when it is at the console's own resolution and fits; returns the factor.
static int Prescale(int filter, int w, int h) {
  int factor = filter == kS360Filter_Hq4x ? 4 : filter == kS360Filter_Hq2x ? 2 : 1;
  if (factor == 1 || h > 240 || w * factor > (int)g_pitch || h * factor > kScaledRows)
    return 1;  // the high-resolution world map is drawn at 4x already
  if (!g_scaled) {
    g_scaled = malloc((size_t)g_pitch * kScaledRows * 4);
    if (!g_scaled) { s360_log("s360: no memory for the HQ filters"); return 1; }
  }
  if (!g_hqx_ready) hqxInit(), g_hqx_ready = true;
  Scale sc = { factor, w, h };
  s360_jobs_run(ScaleBand, &sc, (h + kHqBandRows - 1) / kHqBandRows);
  return factor;
}

static uint32_t g_prepare_us;  // the last present's work before it waits for the display: filters and upload
uint32_t s360_video_prepare_us(void) { return g_prepare_us; }

static uint64_t ClockUs(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
}

void s360_video_present(int w, int h, const S360Display *display) {
  if (!g_open || w <= 0 || h <= 0) return;
  uint64_t t_start = ClockUs();
  Slot *sl = &g_slots[g_slot];
  g_slot = (g_slot + 1) % kSlots;
  if (sl->fence) xsdk_gpu_wait(sl->fence, kBeginTimeoutMs), sl->fence = 0;  // the GPU is done with this slot
  int lines = (h + 112) / 224;  // texture rows to a line of the console's picture: 4 on the high-resolution map
  int factor = Prescale(display->filter, w, h);
  int tw = w * factor, th = h * factor;
  if (!SizeTexture(sl, tw, th)) return;
  // Rows below the picture are whatever the buffer held; the quad never samples them.
  Upload up = { &sl->texture, (uint32_t)th, factor > 1 ? g_scaled : g_pixels };
  s360_jobs_run(UploadBand, &up, (int)((th + kBandRows - 1) / kBandRows));
  xsdk_gpu_invalidate(sl->texture.address, sl->texture.size_bytes);
  int div = lines * factor;
  if (sl->quad_w != tw || sl->quad_h != th || sl->quad_div != div || sl->quad_aspect != display->aspect)
    WriteQuad(sl, tw, th, div, display->aspect);

  // The shader's numbers (blit.frag, Params).
  static const float kScanlines[] = { 0.0f, 0.35f, 0.6f, 0.85f };
  float px_x = sl->screen_w / (float)tw, px_y = sl->screen_h / (float)th;  // screen pixels a texel covers
  bool sharp = display->filter != kS360Filter_Bilinear;
  float shape_x = sharp && px_x > 1.0f ? px_x : 1.0f, shape_y = sharp && px_y > 1.0f ? px_y : 1.0f;
  float texels_per_line = (float)(lines * factor);
  int scan = display->scanlines >= 0 && display->scanlines <= 3 ? display->scanlines : 0;
  const float params[12] = {
    (float)sl->tex_w, (float)sl->tex_h, 1.0f / (float)sl->tex_w, 1.0f / (float)sl->tex_h,
    shape_x, shape_y, kScanlines[scan], texels_per_line,
    1.0f / (3.0f * px_y * texels_per_line), 1.6f, 0.0f, 0.0f,
  };

  g_prepare_us = (uint32_t)(ClockUs() - t_start);
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
  xsdk_gpu_set_pixel_constants(BLIT_FRAG_PUSH, params, BLIT_FRAG_PUSH_REGISTERS);
  xsdk_gpu_set_texture(BLIT_FRAG_SAMPLER_0_0, &sl->texture,
                       display->filter == kS360Filter_Nearest ? &g_point : &g_linear);
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
      for (int x = 0; x < w; x++) g_pixels[(size_t)y * g_pitch + x] = rgb & 0xFFFFFF;
    static const S360Display kPlain = { kS360Aspect_TV, kS360Filter_Nearest, kS360Scanlines_Off };
    s360_video_present(w, h, &kPlain);
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
  free(g_scaled);
  g_pixels = g_scaled = NULL;
  xsdk_present_close();
  xsdk_gpu_close();
  xsdk_shader_free(&g_ps);
  xsdk_shader_free(&g_vs);
  g_open = false;
}
