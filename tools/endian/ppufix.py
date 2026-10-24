#!/usr/bin/env python3
"""Store PPU VRAM/CGRAM/OAM in little-endian order (as on the SNES) and fix ppu.c accesses."""
import re, os
os.chdir('/home/claude/zelda3-xbox360')
def rd(p): return open(p, newline='').read()
def wr(p, s): open(p, 'w', newline='').write(s)
s = rd('snes/ppu.h')
s = s.replace('#include "snes/saveload.h"', '#include "snes/saveload.h"\n#include "types.h"', 1)
s = s.replace('  uint16_t oam[0x110];', '  zle16_t oam[0x110];  // little-endian, as SNES OAM')
s = s.replace('  uint16_t cgram[0x100];', '  zle16_t cgram[0x100];  // little-endian, as SNES CGRAM')
s = s.replace('  uint16_t vram[0x8000];', '  zle16_t vram[0x8000];  // little-endian, as SNES VRAM')
wr('snes/ppu.h', s)
s = rd('src/zelda_rtl.h')
s = s.replace('  uint16 *vram;', '  zle16_t *vram;  // == ppu->vram, little-endian')
wr('src/zelda_rtl.h', s)

# ---- ppu.c ----
s = rd('snes/ppu.c')
s = s.replace('const uint16 *', 'const zle16_t *')
s = s.replace('uint16 *addr = &ppu->vram[', 'zle16_t *addr = &ppu->vram[')
s = s.replace('addr[0] | addr[8] << 16', 'addr[0].v | addr[8].v << 16')
s = s.replace('& 0x7fff], addr[0])', '& 0x7fff], addr[0].v)')
s = re.sub(r'= \*tp;', '= tp->v;', s)
def match_bracket(t, i):
    d = 0
    while i < len(t):
        if t[i] == '[': d += 1
        elif t[i] == ']':
            d -= 1
            if d == 0: return i
        i += 1
    return -1
out = []; i = 0
pat = re.compile(r'ppu->(vram|cgram|oam)\[')
while True:
    m = pat.search(s, i)
    if not m: out.append(s[i:]); break
    j = match_bracket(s, m.end() - 1)
    pre = s[max(0, m.start() - 1):m.start()]
    out.append(s[i:j + 1])
    if pre != '&': out.append('.v')
    i = j + 1
s = ''.join(out)
wr('snes/ppu.c', s)
s = rd('snes/snes.c'); s = s.replace('snes->ppu->vram[0])', 'snes->ppu->vram[0].v)'); wr('snes/snes.c', s)
s = rd('snes/ppu.c')
old = """  pixelBuffer[0] = ((b << 3) | (b >> 2)) * ppu->brightness / 15;
  pixelBuffer[1] = ((g << 3) | (g >> 2)) * ppu->brightness / 15;
  pixelBuffer[2] = ((r << 3) | (r >> 2)) * ppu->brightness / 15;
  pixelBuffer[3] = 0;"""
new = """  // Same layout as the fast renderer: one native-endian uint32 0x00RRGGBB per pixel.
  uint32 b8 = ((b << 3) | (b >> 2)) * ppu->brightness / 15;
  uint32 g8 = ((g << 3) | (g >> 2)) * ppu->brightness / 15;
  uint32 r8 = ((r << 3) | (r >> 2)) * ppu->brightness / 15;
  uint32 px = (r8 & 0xff) << 16 | (g8 & 0xff) << 8 | (b8 & 0xff);
  memcpy(pixelBuffer, &px, 4);"""
s2 = s.replace(old.replace('\n', '\r\n'), new.replace('\n', '\r\n')).replace(old, new)
assert s2 != s, 'slow pixel path not found'
wr('snes/ppu.c', s2)
