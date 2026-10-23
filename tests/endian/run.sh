#!/bin/bash
# Usage: tests/endian/run.sh <zelda3_assets.dat> [frames]
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
ASSETS="$(readlink -f "${1:?assets path}")"; FRAMES="${2:-300}"; OUT=${OUT:-/tmp/zelda_endian}; mkdir -p $OUT
SRCS=$(ls src/*.c snes/*.c | grep -v -e 'src/main.c' -e 'src/opengl.c' -e 'src/glsl_shader.c' -e 'src/config.c')
SRCS="$SRCS tests/endian/harness.c third_party/opus-1.3.1-stripped/opus_decoder_amalgam.c"
FLAGS="-O1 -w -DZELDA_XENON=1 -iquote src -iquote . -I tests/endian -iquote third_party/opus-1.3.1-stripped"
gcc $FLAGS $SRCS -lm -o $OUT/ref_le || exit 1
powerpc-linux-gnu-gcc $FLAGS -static $SRCS -lm -o $OUT/test_be 2> $OUT/build_be.err || { echo "BE BUILD FAILED: $(grep -c 'error' $OUT/build_be.err) errors (see $OUT/build_be.err)"; exit 1; }
$OUT/ref_le "$ASSETS" $FRAMES $OUT/ref.ppm > $OUT/ref.txt
qemu-ppc $OUT/test_be "$ASSETS" $FRAMES $OUT/be.ppm > $OUT/be.txt 2> $OUT/be.err || echo "BE run exited with $? (see $OUT/be.err)"
echo "reference frames: $(wc -l < $OUT/ref.txt)   big-endian frames: $(wc -l < $OUT/be.txt)"
if diff -q $OUT/ref.txt $OUT/be.txt >/dev/null; then echo IDENTICAL; else echo "first mismatch:"; diff $OUT/ref.txt $OUT/be.txt | head -4; fi
