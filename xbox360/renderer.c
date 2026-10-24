#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "renderer.h"
#include "zelda_xenon_renderer_shaders.h"
#include "xsdk/gpu.h"
#include "xsdk/kernel.h"
#include "xsdk/log.h"
#include "xsdk/present.h"
#include "xsdk/shader.h"
#include "xsdk/texture.h"

// Draws the game's 256x224 frame as a textured quad on the 720p display.
//
// Pixel format: the PPU writes one native-endian uint32 0x00RRGGBB per pixel, and an A8R8G8B8 texture is sampled as
// the CPU's own 0xAARRGGBB words (checked on the console: byte-swapping them rotated the channels, red showing as
// cyan). So each pixel only gets an opaque alpha.

enum { BUFFERS = 2, SWAP_INTERVAL = 1, BEGIN_TIMEOUT_MS = 5000, MAX_W = 1024, MAX_H = 1024, STRIDE = 16 };
static const XsdkVertexElement layout[] = {
  { .usage = XSDK_USAGE_POSITION, .format = XSDK_VERTEX_FLOAT2, .offset_bytes = 0 },
  { .usage = XSDK_USAGE_TEXCOORD, .format = XSDK_VERTEX_FLOAT2, .offset_bytes = 8 },
};
typedef struct { float x, y, u, v; } Vertex;
static XsdkShader vs, ps;
static XsdkTexture frame;
static XsdkSamplerState sampler;
static volatile uint32_t *vertices;
static uint32_t *staging;   // what the game draws into (native 0x00RRGGBB), uploaded as 0xFFRRGGBB
static uint32_t sw, sh, dw, dh;
static bool open;
static bool logged_first_present;

static void write_quad(float half_width, float half_height) {
  // Clip space y up; v = 0 is the texture's first row, at the top.
  const Vertex quad[4] = {
    { -half_width, -half_height, 0, 1 }, { half_width, -half_height, 1, 1 },
    { -half_width,  half_height, 0, 0 }, { half_width,  half_height, 1, 0 },
  };
  // GPU memory takes 32-bit volatile stores only (xsdk docs/PITFALLS.md, "Plain stores to GPU memory").
  for (unsigned i = 0; i < sizeof quad / 4; i++) {
    uint32_t w;
    memcpy(&w, (const uint8_t *)quad + i * 4, 4);
    vertices[i] = w;
  }
  xsdk_gpu_invalidate(xsdk_gpu_address(vertices), sizeof quad);
}

bool zelda_xenon_renderer_open(uint32_t width, uint32_t height) {
  xsdk_log_write("zelda3: renderer stage=open_begin size=%ux%u", (unsigned)width, (unsigned)height);
  // Xenos textures and resolves want multiples of 32 (256x224 is 8x7 tiles).
  if (!width || !height || width > MAX_W || height > MAX_H || (width & 31) || (height & 31)) {
    xsdk_log_write("zelda3: renderer invalid frame dimensions %ux%u", (unsigned)width, (unsigned)height);
    return false;
  }
  sw = width; sh = height;
  NTSTATUS s = xsdk_gpu_open();
  if (!NT_SUCCESS(s)) { xsdk_log_write("zelda3: renderer gpu_open failed status=0x%08X", (unsigned)s); return false; }
  xsdk_log_write("zelda3: renderer stage=gpu_open_ok");
  s = xsdk_present_open(XSDK_DISPLAY_720P, BUFFERS, SWAP_INTERVAL, &dw, &dh);
  if (!NT_SUCCESS(s)) { xsdk_log_write("zelda3: renderer present_open failed status=0x%08X", (unsigned)s); return false; }
  xsdk_log_write("zelda3: renderer stage=present_open_ok display=%ux%u", (unsigned)dw, (unsigned)dh);
  s = xsdk_texture_create(&frame, XSDK_FORMAT_A8R8G8B8, sw, sh, 1, true);
  if (!NT_SUCCESS(s)) { xsdk_log_write("zelda3: renderer texture_create failed status=0x%08X", (unsigned)s); return false; }
  xsdk_log_write("zelda3: renderer stage=texture_create_ok format=A8R8G8B8 size=%ux%u", (unsigned)sw, (unsigned)sh);
  s = xsdk_shader_load(&vs, blit_vert.container, blit_vert.container_bytes);
  if (!NT_SUCCESS(s)) { xsdk_log_write("zelda3: renderer vertex_shader_load failed status=0x%08X", (unsigned)s); return false; }
  s = xsdk_shader_load(&ps, blit_frag.container, blit_frag.container_bytes);
  if (!NT_SUCCESS(s)) { xsdk_log_write("zelda3: renderer pixel_shader_load failed status=0x%08X", (unsigned)s); return false; }
  xsdk_log_write("zelda3: renderer stage=shaders_ok");
  s = xsdk_shader_set_vertex_layout(&vs, layout, 2, (const uint32_t[]){ STRIDE });
  if (!NT_SUCCESS(s)) { xsdk_log_write("zelda3: renderer vertex_layout failed status=0x%08X", (unsigned)s); return false; }
  xsdk_log_write("zelda3: renderer stage=vertex_layout_ok");
  vertices = xsdk_gpu_alloc(4 * sizeof(Vertex));
  if (!vertices) { xsdk_log_write("zelda3: renderer gpu_alloc vertices failed"); return false; }
  // Keep the SNES picture 4:3 on the 16:9 display: 4/3 of the height, centred.
  float half_width = (float)dh * 4.0f / 3.0f / (float)dw;
  if (half_width > 1.0f) half_width = 1.0f;
  write_quad(half_width, 1.0f);
  xsdk_log_write("zelda3: renderer stage=vertex_buffer_ok half_width=%d/1000", (int)(half_width * 1000));
  const XsdkSampler sm = { .mag_filter = XSDK_FILTER_POINT, .min_filter = XSDK_FILTER_POINT,
                           .mip_filter = XSDK_FILTER_POINT, .address_u = XSDK_ADDRESS_CLAMP,
                           .address_v = XSDK_ADDRESS_CLAMP };
  xsdk_texture_sampler_state(&sampler, &sm);
  staging = calloc(sw * sh, 4);
  if (!staging) { xsdk_log_write("zelda3: renderer staging allocation failed"); return false; }
  open = true;
  logged_first_present = false;
  xsdk_log_write("zelda3: renderer stage=open_ok");
  return true;
}

void zelda_xenon_renderer_begin(int width, int height, uint8_t **pixels, int *out_pitch) {
  *pixels = NULL; *out_pitch = 0;
  if (!open || width <= 0 || height <= 0 || width > (int)sw || height > (int)sh) {
    xsdk_log_write("zelda3: renderer begin_draw rejected open=%d size=%dx%d capacity=%ux%u",
                   open, width, height, (unsigned)sw, (unsigned)sh);
    return;
  }
  *pixels = (uint8_t *)staging;
  *out_pitch = (int)sw * 4;
}

void zelda_xenon_renderer_end(void) {
  if (!open) return;
  for (uint32_t i = 0; i < sw * sh; i++)
    staging[i] |= 0xFF000000u;
  xsdk_texture_upload(&frame, 0, staging, sw * 4);
  xsdk_gpu_invalidate(frame.address, frame.size_bytes);
  uint32_t address;
  NTSTATUS status = xsdk_present_begin(&address, BEGIN_TIMEOUT_MS);
  if (!NT_SUCCESS(status)) {
    xsdk_log_write("zelda3: renderer present_begin failed status=0x%08X", (unsigned)status);
    return;
  }
  xsdk_gpu_set_target(dw, dh);
  xsdk_gpu_set_shaders(&vs, &ps);
  xsdk_built_shader_set_literals(&blit_vert);
  xsdk_built_shader_set_literals(&blit_frag);
  xsdk_gpu_set_texture(BLIT_FRAG_SAMPLER_0_0, &frame, &sampler);
  xsdk_gpu_set_vertices(0, xsdk_gpu_address(vertices), 4 * sizeof(Vertex));
  xsdk_gpu_draw(XSDK_PRIMITIVE_TRIANGLE_STRIP, 4);
  xsdk_gpu_resolve(address, dw, dh, 0xFF000000);  // black borders
  xsdk_present_end();
  if (!logged_first_present) {
    xsdk_log_write("zelda3: renderer stage=first_present_queued");
    logged_first_present = true;
  }
}

void zelda_xenon_renderer_show_color(uint32_t rgb, int frames) {
  if (!open || !staging) return;
  for (uint32_t i = 0; i < sw * sh; i++) staging[i] = rgb & 0xFFFFFF;
  for (int i = 0; i < frames; i++) zelda_xenon_renderer_end();
}

void zelda_xenon_renderer_close(void) {
  free(staging); staging = NULL;
  if (vertices) xsdk_gpu_free((void *)vertices);
  vertices = NULL;
  if (frame.texels) xsdk_texture_free(&frame);
  xsdk_present_close();
  xsdk_gpu_close();
  xsdk_shader_free(&ps);
  xsdk_shader_free(&vs);
  open = false;
}

static bool renderer_initialize(SDL_Window *window) { (void)window; return zelda_xenon_renderer_open(256, 224); }
static void renderer_destroy(void) { zelda_xenon_renderer_close(); }
static void renderer_begin_adapter(int w, int h, uint8 **p, int *pitch_out) { zelda_xenon_renderer_begin(w, h, p, pitch_out); }
static void renderer_end_adapter(void) { zelda_xenon_renderer_end(); }
void zelda_xenon_renderer_bind(struct RendererFuncs *funcs) {
  funcs->Initialize = renderer_initialize; funcs->Destroy = renderer_destroy;
  funcs->BeginDraw = renderer_begin_adapter; funcs->EndDraw = renderer_end_adapter;
}
