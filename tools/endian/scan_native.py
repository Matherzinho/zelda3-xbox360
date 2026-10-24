#!/usr/bin/env python3
"""Report uses of native (C-initialized) 16/32-bit tables that bypass the type system: mem* calls and casts."""
import re, glob, os
os.chdir('/home/claude/zelda3-xbox360')
decl = re.compile(r'\b(?:static\s+)?(?:const\s+)?(u?int16|u?int32|uint16_t|int16_t|uint32_t)\s+(\w+)\s*((?:\[[^\]]*\])+)\s*=')
total = 0
for f in sorted(glob.glob('src/*.c') + glob.glob('src/*.h')):
    s = open(f, newline='').read()
    names = {m.group(2): m.group(1) for m in decl.finditer(s)}
    if not names: continue
    for ln_no, line in enumerate(s.split('\n'), 1):
        for nm, ty in names.items():
            if not re.search(r'\b%s\b' % nm, line): continue
            if re.search(r'\bmem(cpy|cmp|move|set)\s*\(', line) or re.search(r'\(\s*(const\s+)?(u?int8|uint8_t|char|void|zle16_t|zles16_t|zle32_t)\s*\*\s*\)\s*&?\s*%s\b' % nm, line):
                print('%s:%d [%s %s] %s' % (f, ln_no, ty, nm, line.strip()[:110])); total += 1
print('total', total)
