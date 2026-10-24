# Building the Xbox 360 Xenon target

This target uses [xsdk](https://github.com/xenosworks/xsdk), a homebrew SDK for the retail kernel of an RGH/JTAG
console, and [SDL-xenon](https://github.com/xenosworks/SDL-xenon) (SDL 3 on xsdk) for audio. Neither is copied into
this repository. The PC build is unchanged and still follows the upstream `Makefile` and `BUILDING.md`.

## Requirements

Follow xsdk's `docs/GETTING_STARTED.md`, sections 1 to 5, in WSL Debian:

```sh
sudo apt install build-essential bison flex texinfo libgmp-dev libmpfr-dev libmpc-dev curl xz-utils patch python3 cmake ninja-build git
git clone https://github.com/xenosworks/xsdk.git ~/xsdk
cd ~/xsdk && bash tools/toolchain/build.sh          # GCC 15 for powerpc-xsdk-elf, into ~/xsdk-gcc
# glslang-xenon, copied to ~/xsdk-gcc/bin/glslang (GETTING_STARTED.md, section 4)
cd ~/xsdk && make -j$(nproc)                         # build/libxsdk.a
git clone --branch xenon https://github.com/xenosworks/SDL-xenon.git ~/SDL-xenon
cd ~/SDL-xenon && cmake -S. -Bbuild-xsdk -GNinja -DCMAKE_TOOLCHAIN_FILE=$HOME/xsdk/cmake/xsdk.cmake \
  -DXSDK_TOOLCHAIN=$HOME/xsdk-gcc -DCMAKE_BUILD_TYPE=Release && cmake --build build-xsdk
```

(`tools/toolchain/build.sh` is not marked executable in xsdk's repository, hence `bash` in front of it.)

## Configure and build

From the repository root:

```sh
cmake -S xbox360 -B xbox360/build -G Ninja --fresh \
  -DCMAKE_TOOLCHAIN_FILE=$HOME/xsdk/cmake/xsdk.cmake -DXSDK_TOOLCHAIN=$HOME/xsdk-gcc
cmake --build xbox360/build --target zelda3_xbox360
```

The build type defaults to Release (`-O2`). SDL-xenon is looked for in `~/SDL-xenon`; pass
`-DZELDA_SDL_XENON_DIR=<path>` if it is elsewhere. The output is `xbox360/build/zelda3_xbox360.xex`.

## Game assets

Provide your own legally obtained US Zelda 3 ROM and generate the assets locally:

```sh
python3 assets/restool.py --rom zelda3.sfc --extract-from-rom
```

This writes `zelda3_assets.dat`. Do not commit the ROM or the extracted assets.

## Running on the console

Copy `zelda3_xbox360.xex` and `zelda3_assets.dat` into one folder on the console (for example over FTP) and launch
the `.xex` from the dashboard. The title writes `zelda3_xbox360.log` beside the `.xex`; `exit: status <n>` is its last
line when it ends normally. In-game saves are written to `saves\sram.dat` in the same folder.

Controls follow the SNES pad by position: Xbox A/B/X/Y = SNES B/A/Y/X, LB/LT = L, RB/RT = R, Start = Start,
Back = Select, D-pad or left stick = D-pad. Hold Back+Start for one second to quit.

Click the right stick for the options menu (the game pauses): D-pad up/down chooses, left/right or A changes a value
and applies it at once, B or Start returns to the game. Choices are saved to `zelda3.ini` beside the `.xex`; delete
that file to return to the defaults, which are always the original game.

- Picture: TV 4:3 (as on a CRT), square pixels, whole-number scale, or stretched to 16:9.
- Widescreen: shows more of the world each side (zelda3's extended view); it fills 16:9 in the TV and square modes.
- Text speed: normal, fast (no delay between letters) or instant (whole box at once; pauses skipped). A still
  advances a box as on the SNES.
- Language: the languages present in `zelda3_assets.dat` (English first). A change waits for an open text box to
  close. See "Adding a language" below.
- The rest are zelda3's optional fixes and conveniences (switch items with L/R, turn while dashing, collect items
  with the sword, more bombs and rupees, bug fixes, dimmer flashes, ...).

## Adding a language

Extract the text and font from a translated ROM you own, then add them to the `zelda3_assets.dat` you already built
(no US ROM needed for this step; the file keeps working with the same `.xex`):

```sh
python3 assets/restool.py --extract-dialogue -r <translated ROM>    # writes assets/dialogue_<code>.txt, font_<code>.png
python3 assets/add_language.py zelda3_assets.dat <code>             # adds it, or replaces it if already there
```

`restool.py` recognises the translations it supports by checksum (`assets/util.py`): `pt-br` is "A LENDA DE ZELDA
(BR)" on the US ROM, header or not; the others are listed in the upstream README. Building everything at once also
works: `python3 assets/restool.py --languages=pt-br`. Do not commit or share the extracted text, font or assets.

While starting, the screen briefly turns blue (renderer up), then green (assets loaded); a red screen means
`zelda3_assets.dat` was not found or is invalid, and the log says which.

## Big-endian support

The Xbox 360 CPU is big-endian; the game keeps the SNES's little-endian layout for its RAM, save RAM, PPU memories
and asset tables, and reads and writes multi-byte values through little-endian types (`zle16_t` and friends in
`src/types.h`, which compile to plain loads on little-endian hosts). See `tests/endian/` for the test that compares a
big-endian PowerPC build, run under QEMU, with the PC build frame by frame.
