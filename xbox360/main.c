#include <stdint.h>
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

int main(void) {
 xsdk_log_write("zelda3: stage=program_start image=%s log=%s",
                ExLoadedImageName ? ExLoadedImageName : "(unknown)",
                xsdk_log_path() ? xsdk_log_path() : "(log file unavailable)");
 NTSTATUS splash_status=xsdk_splash_show(1500);
 xsdk_log_write("zelda3: stage=splash result=0x%08X", (unsigned)splash_status);
 char asset_error[192];
 const char *asset_path=getenv("ZELDA3_ASSETS");
 xsdk_log_write("zelda3: stage=assets_begin path=%s base=game:\\",
                asset_path && *asset_path ? asset_path : "zelda3_assets.dat (XSDK title directory)");
 if(!ZeldaGameLoadAssets(asset_path,asset_error,sizeof asset_error)) {
  xsdk_log_write("zelda3: stage=assets_failed error=%s",asset_error);
  return 2;
 }
 xsdk_log_write("zelda3: stage=assets_ok");
 xsdk_log_write("zelda3: stage=game_initialize_begin");
 ZeldaGameInitialize(0,0,"us");
 ZeldaGameSetWantedFeatures(0);
 xsdk_log_write("zelda3: stage=game_initialize_ok");
 struct RendererFuncs renderer;
 zelda_xenon_renderer_bind(&renderer);
 xsdk_log_write("zelda3: stage=renderer_open_begin");
 if(!zelda_xenon_renderer_open(1024,1024)) {
  xsdk_log_write("zelda3: stage=renderer_open_failed"); return 1;
 }
 xsdk_log_write("zelda3: stage=renderer_open_ok");
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
