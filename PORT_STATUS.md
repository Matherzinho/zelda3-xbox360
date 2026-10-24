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
- Xbox frontend: controller mapping in the game's input format, 4:3 picture centred on 720p, pixel format converted
  to what Xenos displays, audio through SDL-xenon, in-game saves in `saves\sram.dat`.

## Not yet confirmed on hardware

- The frontend changes above were checked against xsdk's and SDL-xenon's headers and documentation, not yet on a
  console.
- MSU-1 audio, widescreen, enhanced Mode 7 and the PC's snapshot/replay hotkeys are not enabled on Xbox.

## Legal assets

Provide your own legally obtained compatible US Zelda 3 ROM and generate `zelda3_assets.dat` locally as described in
[BUILDING_XBOX360.md](BUILDING_XBOX360.md). Do not commit or distribute the ROM or extracted asset file.
