# Building the Xbox 360 Xenon target

This experimental target uses the XSDK toolchain and the Xenos renderer in `xbox360/`. It does not replace the original PC build. The XSDK and SDL-xenon are external dependencies and must not be copied into this repository.

## Requirements

- Debian under WSL2.
- XSDK checkout at `/home/Matheus/xsdk`.
- PowerPC Xenon compiler at `/home/Matheus/xsdk-gcc/bin/powerpc-xsdk-elf-gcc`.
- XSDK CMake toolchain `/home/Matheus/xsdk/cmake/xsdk.cmake`.
- SDL-xenon outside this repository at `/home/Matheus/SDL-xenon`, with `/home/Matheus/SDL-xenon/build-xsdk/libSDL3.a`.
- CMake and Ninja.

These absolute paths describe the validated development machine. Adjust `xbox360/CMakeLists.txt` for another installation; do not vendor the SDK or SDL-xenon.

## Configure and build

From the repository root in Debian/WSL:

```sh
cmake -S xbox360 -B xbox360/build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=/home/Matheus/xsdk/cmake/xsdk.cmake
cmake --build xbox360/build --target zelda_xenon_core
cmake --build xbox360/build --target zelda3_xbox360
```

The app target produces `xbox360/build/zelda3_xbox360.xex`. The original PC build remains governed by the upstream Makefile and `BUILDING.md`.

## Game assets

Provide your own legally obtained compatible US Zelda 3 ROM as `zelda3.sfc` at the repository root. The upstream extractor validates SHA-256 `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`. Do not bypass validation. Generate assets locally:

```sh
python3 assets/restool.py --rom zelda3.sfc --extract-from-rom
```

This creates `zelda3_assets.dat`. Keep it beside the title executable on Xbox 360. Do not commit the ROM or extracted assets.

## Runtime validation

The XEX was generated but has not been validated on a physical Xbox 360. Xenia Canary reports `0xC00000B5` during `xsdk_present_open` while filling the display LUT; the same failure was reproduced with the minimal XSDK presentation sample. The title has not reached frame presentation in that emulator. A console test is still required.
