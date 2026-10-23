#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "zelda_rtl.h"
#include "snes/ppu.h"
#include "renderer.h"
#include "util.h"
#include "xsdk/kernel.h"
#include "xsdk/log.h"
#include "xsdk/splash.h"

static uint16_t map_input(uint16_t buttons) {
 uint16_t out=0;
 if(buttons&INPUT_GAMEPAD_A) out|=kJoypadL_A;
 if(buttons&INPUT_GAMEPAD_B) out|=kJoypadH_B<<8;
 if(buttons&INPUT_GAMEPAD_X) out|=kJoypadL_X;
 if(buttons&INPUT_GAMEPAD_Y) out|=kJoypadH_Y<<8;
 if(buttons&INPUT_GAMEPAD_LEFT_SHOULDER) out|=kJoypadL_L;
 if(buttons&INPUT_GAMEPAD_RIGHT_SHOULDER) out|=kJoypadL_R;
 if(buttons&INPUT_GAMEPAD_START) out|=kJoypadH_Start<<8;
 if(buttons&INPUT_GAMEPAD_BACK) out|=kJoypadH_Select<<8;
 if(buttons&INPUT_GAMEPAD_DPAD_UP) out|=kJoypadH_Up<<8;
 if(buttons&INPUT_GAMEPAD_DPAD_DOWN) out|=kJoypadH_Down<<8;
 if(buttons&INPUT_GAMEPAD_DPAD_LEFT) out|=kJoypadH_Left<<8;
 if(buttons&INPUT_GAMEPAD_DPAD_RIGHT) out|=kJoypadH_Right<<8;
 return out;
}


/* Colour codes shown on screen so a failure can be identified without reading the XSDK log.
   (Channel order may differ on hardware; what matters is that each stage looks different.) */
enum { COLOR_RENDERER_OK=0xFF0000FF, COLOR_ASSETS_FAIL=0xFFFF0000, COLOR_ASSETS_OK=0xFF00FF00 };

static bool try_load_assets(const char *path,char *err,size_t err_size) {
 xsdk_log_write("zelda3: assets try path=%s",path);
 FILE *f=fopen(path,"rb");
 if(!f) { xsdk_log_write("zelda3: assets fopen failed errno=%d path=%s",errno,path); return false; }
 fclose(f);
 return ZeldaGameLoadAssets(path,err,err_size);
}

/* There is no reliable working directory on Xbox 360, so try several locations:
   ZELDA3_ASSETS, the directory of the running XEX, then common drive aliases. */
static bool load_assets_any(const char *env_path,char *err,size_t err_size) {
 char path[512];
 if(env_path && *env_path && try_load_assets(env_path,err,err_size)) return true;
 const char *img=ExLoadedImageName;
 if(img) {
  const char *cut=img;
  for(const char *p=img;*p;p++) if(*p=='\\'||*p=='/'||*p==':') cut=p+1;
  size_t len=(size_t)(cut-img);
  if(len+sizeof "zelda3_assets.dat"<sizeof path) {
   memcpy(path,img,len); strcpy(path+len,"zelda3_assets.dat");
   if(try_load_assets(path,err,err_size)) return true;
  }
 }
 static const char *const fallbacks[]={"game:\\zelda3_assets.dat","d:\\zelda3_assets.dat","zelda3_assets.dat"};
 for(unsigned i=0;i<sizeof fallbacks/sizeof *fallbacks;i++)
  if(try_load_assets(fallbacks[i],err,err_size)) return true;
 snprintf(err,err_size,"zelda3_assets.dat not found in any candidate location (see log)");
 return false;
}

int main(void) {
 xsdk_log_write("zelda3: stage=program_start image=%s log=%s",
                ExLoadedImageName ? ExLoadedImageName : "(unknown)",
                xsdk_log_path() ? xsdk_log_path() : "(log file unavailable)");
 NTSTATUS splash_status=xsdk_splash_show(1500);
 xsdk_log_write("zelda3: stage=splash result=0x%08X", (unsigned)splash_status);
 struct RendererFuncs renderer;
 zelda_xenon_renderer_bind(&renderer);
 xsdk_log_write("zelda3: stage=renderer_open_begin");
 if(!zelda_xenon_renderer_open(1024,1024)) {
  xsdk_log_write("zelda3: stage=renderer_open_failed"); return 1;
 }
 xsdk_log_write("zelda3: stage=renderer_open_ok");
 zelda_xenon_renderer_show_color(COLOR_RENDERER_OK,30);
 char asset_error[192];
 const char *asset_path=getenv("ZELDA3_ASSETS");
 xsdk_log_write("zelda3: stage=assets_begin env=%s",asset_path && *asset_path ? asset_path : "(unset)");
 if(!load_assets_any(asset_path,asset_error,sizeof asset_error)) {
  xsdk_log_write("zelda3: stage=assets_failed error=%s",asset_error);
  zelda_xenon_renderer_show_color(COLOR_ASSETS_FAIL,300);
  zelda_xenon_renderer_close();
  return 2;
 }
 xsdk_log_write("zelda3: stage=assets_ok");
 zelda_xenon_renderer_show_color(COLOR_ASSETS_OK,30);
 xsdk_log_write("zelda3: stage=game_initialize_begin");
 ZeldaGameInitialize(0,0,"us");
 ZeldaGameSetWantedFeatures(0);
 xsdk_log_write("zelda3: stage=game_initialize_ok");
 xsdk_log_write("zelda3: stage=main_loop_enter");
 bool first_frame=true;
 for(;;) {
  INPUT_STATE state={0};
  uint32_t input_result=XamInputGetState(0,0,&state);
  if(input_result!=ERROR_SUCCESS) {
   if(first_frame) xsdk_log_write("zelda3: first input poll failed result=0x%08X",(unsigned)input_result);
   state.gamepad.buttons=0;
  }
  if(state.gamepad.buttons&INPUT_GAMEPAD_BACK) {
   xsdk_log_write("zelda3: exit requested by Xbox Back button");
   break;
  }
  if(first_frame) xsdk_log_write("zelda3: stage=first_update_begin");
  ZeldaGameUpdate(map_input((uint16_t)state.gamepad.buttons));
  if(first_frame) xsdk_log_write("zelda3: stage=first_update_ok");
  const int scale=PpuGetCurrentRenderScale(g_zenv.ppu,0);
  uint8_t *pixels=NULL; int pitch=0;
  renderer.BeginDraw(256*scale,224*scale,&pixels,&pitch);
  if(!pixels || pitch <= 0) {
   xsdk_log_write("zelda3: frame buffer unavailable width=%d height=%d pixels=%p pitch=%d",
                  256*scale,224*scale,(void*)pixels,pitch);
   break;
  }
  if(first_frame) xsdk_log_write("zelda3: stage=first_frame_buffer_ok width=%d height=%d pitch=%d",
                                 256*scale,224*scale,pitch);
  ZeldaGameRender(pixels,(size_t)pitch,0);
  if(first_frame) xsdk_log_write("zelda3: stage=first_frame_render_ok");
  renderer.EndDraw();
  if(first_frame) {
   xsdk_log_write("zelda3: stage=first_frame_present_returned");
   first_frame=false;
  }
 }
 xsdk_log_write("zelda3: stage=renderer_close");
 zelda_xenon_renderer_close();
 xsdk_log_write("zelda3: stage=program_exit");
 return 0;
}


#include <stdio.h>
#include <stdlib.h>
#include "util.h"
#include "audio.h"
void NORETURN Die(const char *error) { xsdk_log_write("zelda3 fatal: %s", error); exit(1); }
/* Audio output is not started by this first video/input frontend, so no other
   thread accesses the APU mixer and these synchronization hooks are unused. */
void ZeldaApuLock(void) {}
void ZeldaApuUnlock(void) {}
