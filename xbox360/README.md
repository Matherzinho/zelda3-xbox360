# Zelda 3 for Xbox 360 (xsdk)

The port has two parts:

- `platform/` is **snes360**, a small Xbox 360 platform layer meant to be reused for other SNES-derived ports. It owns
  video (a texture drawn as one quad, with the TV 4:3, square-pixel, whole-scale and stretched modes), audio through
  SDL-xenon, the controller (mapped to the SNES pad by position), settings saved to `<name>.ini` beside the `.xex`,
  an 8x8 text overlay and the in-game options menu. A game describes itself with one `S360Game` (`snes360.h`): its
  frame size, its options, and functions to start, apply settings, run a frame, draw and render sound.
- `zelda3/zelda3_port.c` is that description for zelda3: its options map onto zelda3's own features (the PC build's
  `zelda3.ini`) and render flags.

Building, controls and how to run it on the console: [../BUILDING_XBOX360.md](../BUILDING_XBOX360.md).
`prepare-usb-package.ps1` copies the `.xex` and `zelda3_assets.dat` into `build/usb-package` for a USB drive.
