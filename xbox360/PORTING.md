# Porting another SNES game to the Xbox 360 with snes360

`xbox360/platform` ("snes360") is the console-specific half of this port, written so that another SNES
reimplementation or emulator can reuse it. It owns the GPU presentation, the display filters, sound, the controller,
the settings file and the in-game menu; it knows nothing about Zelda. zelda3's own half is one file,
[`zelda3/zelda3_port.c`](zelda3/zelda3_port.c), about 200 lines — that file is the example to copy.

This guide is what we learned doing it: the interface, the build, the big-endian work (the hard part) and the console
pitfalls that cost us the most time.

## 1. What you need

- An RGH/JTAG Xbox 360 and a way to copy files to it (FTP).
- [xsdk](https://github.com/xenosworks/xsdk) and its GCC toolchain for `powerpc-xsdk-elf`.
- [SDL-xenon](https://github.com/xenosworks/SDL-xenon), built for xsdk (snes360 uses it for audio only).
- A game that already runs on a PC as portable C: a reimplementation (zelda3, Cannonball, …) or an emulator core.

## 2. Describe your game

Fill in one `S360Game` ([`platform/snes360.h`](platform/snes360.h)) and call `s360_run`:

```c
#include "snes360.h"

static bool Init(char *error, size_t error_size) {
  if (!MyGameLoadData()) { snprintf(error, error_size, "data file missing"); return false; }
  return true;
}
static void ApplySettings(S360Aspect aspect) { /* read your S360Setting values */ }
static void RunFrame(uint16_t buttons) { MyGameRunFrame(buttons); }      // kS360_* bits
static void Draw(uint32_t *pixels, int pitch, int *w, int *h) {
  *w = 256, *h = 224;
  MyGameRender(pixels, pitch * 4);                                       // 0x00RRGGBB
}
static void RenderAudio(int16_t *samples, int frames) { MyGameAudio(samples, frames); }

int main(void) {
  static const S360Game game = {
    .name = "mygame", .title = "My Game",
    .max_width = 256, .max_height = 224,
    .init = Init, .apply_settings = ApplySettings, .run_frame = RunFrame,
    .draw = Draw, .render_audio = RenderAudio,
    .settings = g_options, .num_settings = kNumOptions,
  };
  return s360_run(&game);
}
```

What the platform does for you from there:

- **Picture.** `draw` fills a buffer the platform owns; it uploads it, shapes it for the 16:9 display (TV 4:3, square
  pixels, whole scale, stretched) and applies the display filter and scanlines. Return a different `width`/`height`
  any frame you like — a high-resolution mode, a wider picture — as long as it fits `max_width`/`max_height`.
- **Sound.** SDL-xenon pulls `render_audio` from its own thread, 16-bit stereo at `s360_audio_rate()`. The platform
  takes `s360_audio_lock()` around `run_frame`, so your emulated sound chip is never touched by two threads at once.
- **Controller.** Mapped by position (Xbox A/B/X/Y to SNES B/A/Y/X, both shoulder buttons and triggers to L/R, Back
  to Select, left stick as a D-pad).
- **Settings and menu.** Each `S360Setting` is one line in `<name>.ini` beside the `.xex` and one row in the menu the
  right stick opens. `apply_settings` is called when one changes, so most options can take effect at once; set
  `restart_needed` for those that cannot and the menu marks them.
- **Threads.** `s360_jobs_run()` ([`platform/s360_jobs.h`](platform/s360_jobs.h)) shares a per-frame job across the
  console's other cores. We use it for the high-resolution renderer and the HQ filters; a per-scanline renderer or a
  per-band blitter fits the same shape.
- **Start-up colours.** Blue when the GPU is up, green when `init` returned, red when it failed, so a console with no
  log still tells you where it stopped.

## 3. Build it

Copy `xbox360/CMakeLists.txt` and change the last block to name your sources. The platform is a library you link:

```cmake
add_executable(mygame_xbox360 mygame/mygame_port.c)
target_link_libraries(mygame_xbox360 PRIVATE snes360 mygame_core SDL3::SDL3-static m)
xsdk_add_xex(mygame_xbox360 NAME "My Game")
```

```sh
cmake -S xbox360 -B xbox360/build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$HOME/xsdk/cmake/xsdk.cmake \
  -DXSDK_TOOLCHAIN=$HOME/xsdk-gcc -DZELDA_SDL_XENON_DIR=$HOME/SDL-xenon
cmake --build xbox360/build
```

## 4. The hard part: the Xbox 360 is big-endian

A SNES reimplementation is full of code that writes a byte and reads it back as part of a 16-bit word, because that
is what the original 65816 code did. On a little-endian PC that works by accident. On the console's PowerPC it does
not, and the game crashes or draws nonsense within a frame or two.

What worked here, in `src/types.h`:

- Keep the SNES's own memories (RAM, save RAM, VRAM, CGRAM, OAM) and the asset tables byte for byte as they are, and
  read and write multi-byte values through **little-endian types**: `scalar_storage_order("little-endian")` packed
  structs (`zle16_t`, `zles16_t`, `zle32_t`). GCC then emits byte-swapping loads and stores on PowerPC and plain ones
  on x86, so the PC build is untouched.
- Wrap those accesses in macros (`RAM16`, `RAM32`, `WORD`, `HIBYTE`, …) rather than changing call sites by hand, so
  the two builds stay one body of code.
- Snapshots and save states need the same treatment, or a file written on one machine will not load on the other.
- Watch for byte-level tricks that no type can fix: a table read as bytes in one place and as words in another, a
  structure whose size differs between ABIs, a `memcpy` of a struct that holds words.

**Test it against the PC build, frame by frame.** [`tests/endian/`](../tests/endian) builds the same game core for
x86 and for PowerPC, runs both under QEMU over the reference playthroughs, and compares a hash of the RAM, the PPU
memories, the save RAM, every pixel and the audio each frame. That test is what turned a hundred silent byte bugs
into a list of exact frame numbers. It is worth writing before you write any console code.

## 5. Console pitfalls we hit

- **Texture colours.** A Xenos `A8R8G8B8` texture samples the CPU's own `0xAARRGGBB` words. Byte-swapping the pixels
  "to be safe" rotates the channels and tints everything blue.
- **The picture's alpha.** Rather than walking the frame to set the top byte, let the texture swizzle return 1 for
  alpha (`xsdk_texture_swizzle`). It is free.
- **Vertex inputs go by location,** not by name: 0 is POSITION, 1 COLOR, 2 TEXCOORD0. A texture coordinate at
  location 1 fails the draw with `0xC000000D`.
- **GPU memory takes volatile 32-bit stores.** GCC merges plain stores into 64-bit ones, and an unaligned 64-bit
  store raises an alignment exception on the console.
- **The shader compiler takes a subset of GLSL.** No function calls (write the maths out), no `clamp` with bounds
  that are not constants (use `min`/`max`), and a local may be assigned only once. The build tells you which.
- **Keep one frame in flight.** We use two textures and a fence each, so the CPU never writes what the GPU is still
  reading; with one, the picture tears or stalls.
- **Upload only the picture.** Sizing the texture to the frame (rounded up to 32) instead of the largest possible
  frame cut the per-frame upload about five times.
- **Watch the clock in the log.** `s360_log` lines with the time each stage takes (`run`, `draw`, `filter`,
  `present`) are how we found what was slow without a profiler on the console.

## 6. Where to look in this repository

| What | Where |
|---|---|
| The platform's interface | [`platform/snes360.h`](platform/snes360.h) |
| A complete game description | [`zelda3/zelda3_port.c`](zelda3/zelda3_port.c) |
| Presentation, filters, scanlines | [`platform/s360_video.c`](platform/s360_video.c), [`platform/blit.frag`](platform/blit.frag) |
| Worker threads | [`platform/s360_jobs.c`](platform/s360_jobs.c) |
| Options menu and settings file | [`platform/s360_menu.c`](platform/s360_menu.c), [`platform/s360_settings.c`](platform/s360_settings.c) |
| Big-endian types and macros | [`../src/types.h`](../src/types.h) |
| Frame-by-frame parity test | [`../tests/endian/`](../tests/endian) |
| Interface text per language | [`../src/ui_text.c`](../src/ui_text.c) |

The platform layer is MIT, like the rest of this repository, except `platform/hqx` (Maxim Stepin's hqx, LGPL 2.1).
Reuse it, and if you port something with it, say so in an issue — we would like to know.
