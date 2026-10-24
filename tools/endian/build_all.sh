#!/bin/bash
# Reproducible pipeline: original sources (commit 6b28a73) -> conversions -> manual fixes (work/manual.patch)
cd /home/claude/zelda3-xbox360 || exit 1
git checkout -q 6b28a73 -- src snes third_party && git reset -q || exit 1
python3 /home/claude/work/convert.py >/dev/null || exit 1
python3 /home/claude/work/ptrfix.py $(ls src/*.c src/*.h | grep -v -e src/main.c -e src/types.h -e src/variables.h -e src/assets.h) >/dev/null
CONVC="${CONVC:-$(cat /home/claude/work/convc.txt)}"; CONVH="${CONVH:-$(cat /home/claude/work/convh.txt)}"
python3 /home/claude/work/ptrconv.py $CONVC $CONVH >/dev/null
python3 /home/claude/work/datafix.py
[ -f /home/claude/work/post.sh ] && bash /home/claude/work/post.sh
true
