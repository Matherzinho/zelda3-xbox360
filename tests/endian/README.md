# Big-endian test

The Xbox 360 is big-endian. This test builds the game core twice, natively (x86) and for big-endian PowerPC, runs
both headless for the same frames and compares them. `harness.c` prints, for every frame, hashes of the frame's
pixels, the game RAM, VRAM, CGRAM+OAM, save RAM and (with `ZAUDIO=1`) the audio samples; all hashes are computed on
values, so they match across hosts when the game behaves the same.

Needs `gcc`, `powerpc-linux-gnu-gcc` (with `libc6-dev-powerpc-cross`) and `qemu-user` (Debian/Ubuntu packages of
those names), and your locally generated `zelda3_assets.dat`.

```sh
tests/endian/run.sh /path/to/zelda3_assets.dat 600     # attract mode, 600 frames
ZREPLAY=0 tests/endian/run.sh /path/to/zelda3_assets.dat 20000   # replay saves/ref chapter 1
```

`ZREPLAY=N` replays `saves/ref` chapter N+1 (0-12), the upstream reference playthrough of the whole game.
`ZSTATE=<file>` (with `ZSTATE_FROM=<frame>`) also writes the raw state of each frame, for finding the first byte
that differs.
