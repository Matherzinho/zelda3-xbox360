# Zelda3

## Xbox 360 port

This repository is a port of zelda3 to the Xbox 360, for consoles that run homebrew (RGH/JTAG), built with the
[xsdk](https://github.com/xenosworks/xsdk) homebrew SDK. It is based on `snesrev/zelda3` commit
`45a149d8b4bb8098f53d1009f76e76e7d8b79bbf`; the game code is unchanged apart from big-endian support, and plays the
same as the PC build frame for frame (checked over all 13 of the upstream reference playthroughs).

- 60 frames a second at 720p, with sound, saves and the Xbox controller.
- An options menu in the game (click the right stick): picture shape, widescreen, display filter (sharp bilinear by
  default, bilinear, sharp pixels, HQ2x, HQ4x), CRT scanlines, language, text speed, the high-resolution world map
  and zelda3's optional fixes and conveniences. Every default is the original game.
- Brazilian Portuguese from a translated ROM you own, interface included, alongside English; more languages can be
  added the same way.
- `xbox360/platform` is a small layer meant to be reused by other SNES ports (video and filters, sound, controller,
  settings, menu, worker threads).

How to build it and run it: [BUILDING_XBOX360.md](BUILDING_XBOX360.md). What is done and what is known:
[PORT_STATUS.md](PORT_STATUS.md). **Porting another SNES game with this platform layer:**
[xbox360/PORTING.md](xbox360/PORTING.md). This community port is not affiliated with Nintendo or Microsoft. Supply your own
legally obtained US ROM and build `zelda3_assets.dat` yourself; do not commit or share ROMs, extracted assets or the
files built from them. HQ2x and HQ4x are Maxim Stepin's hqx, under the GNU LGPL 2.1 (`xbox360/platform/hqx`).

A reimplementation of Zelda 3.

Our discord server is: https://discord.gg/AJJbJAzNNJ

## About

This is a reverse engineered clone of Zelda 3 - A Link to the Past.

It's around 70-80kLOC of C code, and reimplements all parts of the original game. The game is playable from start to end.

You need a copy of the ROM to extract game resources (levels, images). Then once that's done, the ROM is no longer needed.

It uses the PPU and DSP implementation from [LakeSnes](https://github.com/elzo-d/LakeSnes), but with lots of speed optimizations.
Additionally, it can be configured to also run the original machine code side by side. Then the RAM state is compared after each frame, to verify that the C implementation is correct.

I got much assistance from spannerism's Zelda 3 JP disassembly and the other ones that documented loads of function names and variables.

## Additional features

A bunch of features have been added that are not supported by the original game. Some of them are: basic widescreen, shaders, 240 vertical res, hi-res mode7 map, MSU-1 audio, item switching, and over 30 bugfixes.

## Building

You must self-build for now. Steps for 64-bit Windows:<br>
(0) Download [Python](https://www.python.org/ftp/python/3.11.4/python-3.11.4-amd64.exe) and install with "Add to PATH" checked<br>
(1) Click the green button "Code > Download ZIP" on the github page and extract the ZIP<br>
(2) Place your USA rom named zelda3.sfc in that folder<br>
(3) Download [TCC](https://github.com/FitzRoyX/tinycc/releases/download/tcc_20230519/tcc_20230519.zip) and [SDL2](https://github.com/libsdl-org/SDL/releases/download/release-2.28.2/SDL2-devel-2.28.2-VC.zip) and extract each ZIP into the "third-party" subfolder<br>
(4) Double-click "get_python_libs.bat" in the main dir.<br>
(5) Double-click "extract_assets.bat" in the main dir. This will create zelda3_assets.dat.<br>
(6) Double-click "run_with_tcc.bat" in the main dir. This will create zelda3.exe and run it.<br>
(7) Configure with zelda3.ini in a text editor like notepad++<br>

For other platforms and compilers, see: https://github.com/snesrev/zelda3/blob/main/BUILDING.md

## Usage and controls

The game supports snapshots. The joypad input history is also saved in the snapshot. It's thus possible to replay a playthrough in turbo mode to verify that the game behaves correctly.

The game is run with `./zelda3` and takes an optional path to the ROM-file, which will verify for each frame that the C code matches the original behavior.

| Button | Key         |
| ------ | ----------- |
| Up     | Up arrow    |
| Down   | Down arrow  |
| Left   | Left arrow  |
| Right  | Right arrow |
| Start  | Enter       |
| Select | Right shift |
| A      | X           |
| B      | Z           |
| X      | S           |
| Y      | A           |
| L      | C           |
| R      | V           |

The keys can be reconfigured in zelda3.ini

Additionally, the following commands are available:

| Key | Action                |
| --- | --------------------- |
| Tab | Turbo mode |
| W   | Fill health/magic     |
| Shift+W   | Fill rupees/bombs/arrows     |
| Ctrl+E | Reset            |
| P   | Pause (with dim)                |
| Shift+P   | Pause (without dim)                |
| Ctrl+Up   | Increase window size                |
| Ctrl+Down   | Decrease window size                |
| T   | Toggle replay turbo mode  |
| O   | Set dungeon key to 1  |
| K   | Clear all input history from the joypad log  |
| L   | Stop replaying a shapshot  |
| R   | Toggle between fast and slow renderer |
| F   | Display renderer performance |
| F1-F10 | Load snapshot      |
| Alt+Enter | Toggle Fullscreen     |
| Shift+F1-F10 | Save snapshot |
| Ctrl+F1-F10 | Replay the snapshot |
| 1-9 | Load a dungeons playthrough snapshot |
| Ctrl+1-9 | Run a dungeons playthrough in turbo mode |


## License

This project is licensed under the MIT license. See 'LICENSE.txt' for details.
