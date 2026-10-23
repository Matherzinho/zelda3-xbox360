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

enum { BUFFERS=2, SWAP_INTERVAL=1, BEGIN_TIMEOUT_MS=5000, MAX_W=1024, MAX_H=1024, STRIDE=16 };
static const XsdkVertexElement layout[] = {
 { .usage=XSDK_USAGE_POSITION, .format=XSDK_VERTEX_FLOAT2, .offset_bytes=0 },
 { .usage=XSDK_USAGE_TEXCOORD, .format=XSDK_VERTEX_FLOAT2, .offset_bytes=8 },
};
typedef struct { float x,y,u,v; } Vertex;
static const Vertex quad[4]={{-1,-1,0,1},{1,-1,1,1},{-1,1,0,0},{1,1,1,0}};
static XsdkShader vs,ps;
static XsdkTexture frame;
static XsdkSamplerState sampler;
static volatile uint32_t *vertices;
static uint8_t *staging;
static uint32_t sw,sh,dw,dh;
static int pitch;
static bool open;
static bool logged_first_present;

bool zelda_xenon_renderer_open(uint32_t width,uint32_t height) {
 xsdk_log_write("zelda3: renderer stage=open_begin size=%ux%u",(unsigned)width,(unsigned)height);
 if(!width||!height||width>MAX_W||height>MAX_H) {
  xsdk_log_write("zelda3: renderer invalid frame dimensions %ux%u",(unsigned)width,(unsigned)height);
  return false;
 }
 sw=width;sh=height;
 NTSTATUS s=xsdk_gpu_open();
 if(!NT_SUCCESS(s)) { xsdk_log_write("zelda3: renderer gpu_open failed status=0x%08X",(unsigned)s); return false; }
 xsdk_log_write("zelda3: renderer stage=gpu_open_ok");
 s=xsdk_present_open(XSDK_DISPLAY_720P,BUFFERS,SWAP_INTERVAL,&dw,&dh);
 if(!NT_SUCCESS(s)) { xsdk_log_write("zelda3: renderer present_open failed status=0x%08X",(unsigned)s); return false; }
 xsdk_log_write("zelda3: renderer stage=present_open_ok display=%ux%u",(unsigned)dw,(unsigned)dh);
 s=xsdk_texture_create(&frame,XSDK_FORMAT_A8R8G8B8,sw,sh,1,true);
 if(!NT_SUCCESS(s)) { xsdk_log_write("zelda3: renderer texture_create failed status=0x%08X",(unsigned)s); return false; }
 xsdk_log_write("zelda3: renderer stage=texture_create_ok format=A8R8G8B8 size=%ux%u",(unsigned)sw,(unsigned)sh);
 s=xsdk_shader_load(&vs,blit_vert.container,blit_vert.container_bytes);
 if(!NT_SUCCESS(s)) { xsdk_log_write("zelda3: renderer vertex_shader_load failed status=0x%08X",(unsigned)s); return false; }
 xsdk_log_write("zelda3: renderer stage=vertex_shader_ok");
 s=xsdk_shader_load(&ps,blit_frag.container,blit_frag.container_bytes);
 if(!NT_SUCCESS(s)) { xsdk_log_write("zelda3: renderer pixel_shader_load failed status=0x%08X",(unsigned)s); return false; }
 xsdk_log_write("zelda3: renderer stage=pixel_shader_ok");
 s=xsdk_shader_set_vertex_layout(&vs,layout,2,(const uint32_t[]){STRIDE});
 if(!NT_SUCCESS(s)) { xsdk_log_write("zelda3: renderer vertex_layout failed status=0x%08X",(unsigned)s); return false; }
 xsdk_log_write("zelda3: renderer stage=vertex_layout_ok");
 vertices=xsdk_gpu_alloc(sizeof quad);
 if(!vertices) { xsdk_log_write("zelda3: renderer gpu_alloc vertices failed bytes=%u",(unsigned)sizeof quad); return false; }
 xsdk_log_write("zelda3: renderer stage=vertex_buffer_ok");
 for(unsigned i=0;i<sizeof quad/4;i++) { uint32_t w; memcpy(&w,(const uint8_t*)quad+i*4,4); vertices[i]=w; }
 xsdk_gpu_invalidate(xsdk_gpu_address(vertices),sizeof quad);
 const XsdkSampler sm={.mag_filter=XSDK_FILTER_POINT,.min_filter=XSDK_FILTER_POINT,.mip_filter=XSDK_FILTER_POINT,.address_u=XSDK_ADDRESS_CLAMP,.address_v=XSDK_ADDRESS_CLAMP};
 xsdk_texture_sampler_state(&sampler,&sm);
 staging=malloc(sw*sh*4);
 if(!staging) { xsdk_log_write("zelda3: renderer staging allocation failed bytes=%u",(unsigned)(sw*sh*4)); return false; }
 open=true;
 logged_first_present=false;
 xsdk_log_write("zelda3: renderer stage=open_ok staging_bytes=%u",(unsigned)(sw*sh*4));
 return true;
}
void zelda_xenon_renderer_begin(int width,int height,uint8_t **pixels,int *out_pitch) {
 *pixels=NULL;*out_pitch=0;
 if(!open||width<=0||height<=0||width>(int)sw||height>(int)sh) {
  xsdk_log_write("zelda3: renderer begin_draw rejected open=%d size=%dx%d capacity=%ux%u",
                 open,width,height,(unsigned)sw,(unsigned)sh);
  return;
 }
 pitch=(int)sw*4;*pixels=staging;*out_pitch=pitch;
}
void zelda_xenon_renderer_end(void) {
 if(!open)return;
 xsdk_texture_upload(&frame,0,staging,(uint32_t)pitch);
 xsdk_gpu_invalidate(frame.address,frame.size_bytes);
 uint32_t address;
 NTSTATUS status=xsdk_present_begin(&address,BEGIN_TIMEOUT_MS);
 if(!NT_SUCCESS(status)) {
  xsdk_log_write("zelda3: renderer present_begin failed status=0x%08X",(unsigned)status);
  return;
 }
 xsdk_gpu_set_target(dw,dh);
 xsdk_gpu_set_shaders(&vs,&ps);
 xsdk_built_shader_set_literals(&blit_vert);
 xsdk_built_shader_set_literals(&blit_frag);
 xsdk_gpu_set_texture(BLIT_FRAG_SAMPLER_0_0,&frame,&sampler);
 xsdk_gpu_set_vertices(0,xsdk_gpu_address(vertices),sizeof quad);
 xsdk_gpu_draw(XSDK_PRIMITIVE_TRIANGLE_STRIP,4);
 xsdk_gpu_resolve(address,dw,dh,0xFF000000);
 xsdk_present_end();
 if(!logged_first_present) {
  xsdk_log_write("zelda3: renderer stage=first_present_queued");
  logged_first_present=true;
 }
}
void zelda_xenon_renderer_show_color(uint32_t argb,int frames) {
 if(!open||!staging) return;
 uint32_t *px=(uint32_t*)staging;
 for(uint32_t i=0;i<sw*sh;i++) px[i]=argb;
 pitch=(int)sw*4;
 for(int i=0;i<frames;i++) zelda_xenon_renderer_end();
}
void zelda_xenon_renderer_close(void) {
 if (staging) free(staging);
 staging = NULL;
 if (vertices) xsdk_gpu_free(vertices);
 vertices = NULL;
 if(frame.texels)xsdk_texture_free(&frame);
 xsdk_present_close();
 xsdk_gpu_close();
 xsdk_shader_free(&ps);xsdk_shader_free(&vs);open=false;
}






static bool renderer_initialize(SDL_Window *window) { (void)window; return zelda_xenon_renderer_open(1024, 1024); }
static void renderer_destroy(void) { zelda_xenon_renderer_close(); }
static void renderer_begin_adapter(int w,int h,uint8 **p,int *pitch_out) { zelda_xenon_renderer_begin(w,h,p,pitch_out); }
static void renderer_end_adapter(void) { zelda_xenon_renderer_end(); }
void zelda_xenon_renderer_bind(struct RendererFuncs *funcs) {
 funcs->Initialize=renderer_initialize; funcs->Destroy=renderer_destroy;
 funcs->BeginDraw=renderer_begin_adapter; funcs->EndDraw=renderer_end_adapter;
}
