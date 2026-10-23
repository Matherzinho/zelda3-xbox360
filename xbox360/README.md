# Zelda 3 Xbox 360 test package

The Xenon build target is `zelda3_xbox360`; the XEX is emitted in `build/zelda3_xbox360.xex`.

## Required game data

The source repository does not include `zelda3_assets.dat`. The title requires a locally generated copy of this data at runtime. The frontend reports the asset-loading error through the XSDK title log and exits with status 2.

Use a ROM you are legally entitled to use. The extractor expects the USA ROM identified by SHA-1 `6D4F10A8B10E10DBE624CB23CF03B88BB8252973` (the README's documented SHA-256 is `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`). It strips a 0x200-byte copier header before hashing when present. Place your file outside the repository if desired, then from the Zelda repository run:

```sh
python3 assets/restool.py --rom /absolute/path/to/your/zelda3.sfc --extract-from-rom
```

The script validates the ROM, extracts resources and writes `zelda3_assets.dat` to the repository root. It requires Python 3 and the dependencies in `requirements.txt`. Do not run `make clean`; that target removes generated assets.

## Copy and launch on Xbox 360

Copy `zelda3_xbox360.xex` and `zelda3_assets.dat` into the same title directory on the Xbox storage device. Launch with that directory as the title's working directory so the default relative filename resolves. If the launcher can set environment variables, `ZELDA3_ASSETS` may instead name the full asset path. That override uses the C runtime environment and has not been verified on console.

The frontend logs a specific missing, invalid-header, or truncated-file message using `xsdk_log_write`. Use the XSDK title log available from the launch environment to inspect it.

The build has not been run on an Xbox 360. Filesystem drive mapping, launcher working-directory behavior, rendering and controller input still need a hardware test.
