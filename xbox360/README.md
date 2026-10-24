# Zelda 3 for Xbox 360 (xsdk)

`main.c` is the console frontend (controller, audio, saves, main loop) and `renderer.c` draws the game's 256x224
frame on the 720p display with xsdk's GPU layer; `blit.vert`/`blit.frag` are its shaders.

Building, controls and how to run it on the console: [../BUILDING_XBOX360.md](../BUILDING_XBOX360.md).
`prepare-usb-package.ps1` copies the `.xex` and `zelda3_assets.dat` into `build/usb-package` for a USB drive.
