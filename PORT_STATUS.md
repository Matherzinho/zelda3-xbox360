# Xbox 360 port status

Community Xenon port based on `snesrev/zelda3` baseline `45a149d8b4bb8098f53d1009f76e76e7d8b79bbf`. The original MIT
license and upstream credits remain in `LICENSE.txt` and the upstream README. This project is not affiliated with
Nintendo or Microsoft.

## Done

- xsdk CMake target and XEX packaging (`xbox360/`), Release build by default.
- Big-endian correctness of the game core: RAM, SRAM, VRAM, CGRAM, OAM and asset tables are kept little-endian and
  accessed through little-endian types; snapshots load the same on every host and ABI.
- Verified under QEMU: a big-endian PowerPC build matches the original x86 build byte for byte (RAM, VRAM, CGRAM,
  OAM, SRAM and every frame's pixels) while replaying the upstream reference playthroughs in `saves/ref`
  (`tests/endian/`). The PC build's output is unchanged.
- Xbox frontend (`xbox360/platform`, reusable as snes360): controller mapping in the game's input format, picture
  modes for 720p, audio through SDL-xenon, in-game saves in `saves\sram.dat`. Runs at full speed on a console with
  zelda3's faster renderer.
- In-game options menu (click the right stick), saved to `zelda3.ini`: picture mode, widescreen, language, text speed,
  high-resolution world map, no sprite limit and zelda3's optional fixes and quality-of-life features. Every option
  defaults to the original game.
- The high-resolution (4x) Mode 7 renderer also matches between the big-endian and x86 builds, drawn on one thread
  or shared between worker threads (the Xbox draws its lines on three worker threads on the other cores).
- Brazilian Portuguese (`pt-br`) from "A LENDA DE ZELDA (BR)": text and font extracted from the translated ROM and
  added to the assets with `assets/add_language.py`; switchable in the menu.

## Not yet confirmed on hardware

- The options menu, widescreen and text speeds were tested in a simulator of the frontend, not yet on a console.
- MSU-1 audio and the PC's snapshot/replay hotkeys are not enabled on Xbox.
- Worker threads, picture-sized textures and the Portuguese text were tested in QEMU and a simulator of the frontend,
  not yet on a console.
- Only the dialogue font and text change with the language: graphics with words in them (title screen, item names
  on the menu screen, the name entry screen) stay as in the US assets.

## Legal assets

Provide your own legally obtained compatible US Zelda 3 ROM and generate `zelda3_assets.dat` locally as described in
[BUILDING_XBOX360.md](BUILDING_XBOX360.md). Do not commit or distribute the ROM or extracted asset file.
