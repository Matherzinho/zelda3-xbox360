#!/usr/bin/env python3
"""Step 1: mechanical little-endian conversion of the zelda3 core for big-endian hosts."""
import re, glob, os
os.chdir('/home/claude/zelda3-xbox360')
def rd(p): return open(p, newline='').read()
def wr(p, s): open(p, 'w', newline='').write(s)

# ---------------- types.h ----------------
s = rd('src/types.h'); nl = '\r\n' if '\r\n' in s else '\n'
old_block = s[s.index('#define BYTE(x)'):s.index('#define XY(x, y)')]
new_block = '''// ---- Endianness ---------------------------------------------------------------------------
// The game's RAM, ROM-derived assets and save data are little-endian (SNES). On a big-endian
// host (Xbox 360) multi-byte accesses go through types the compiler byte-swaps for us.
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define ZELDA_BIG_ENDIAN_HOST 1
#define ZELDA_LE_STRUCT __attribute__((scalar_storage_order("little-endian"), packed))
typedef struct ZELDA_LE_STRUCT { uint16 a[0x10000]; } zle16_array_t;
typedef struct ZELDA_LE_STRUCT { int16 a[0x10000]; } zles16_array_t;
typedef struct ZELDA_LE_STRUCT { uint32 a[0x10000]; } zle32_array_t;
#define RAM16(p) (((zle16_t*)(p))->v)
#define RAM32(p) (((zle32_t*)(p))->v)
#define RAMS16(p) (((zles16_t*)(p))->v)
#define RAM16A(p) (((zle16_array_t*)(p))->a)
#define RAMS16A(p) (((zles16_array_t*)(p))->a)
#define RAM32A(p) (((zle32_array_t*)(p))->a)
#define ZELDA_LOW_IDX(x) (sizeof(x) - 1)
#define ZELDA_HIGH_IDX(x) (sizeof(x) - 2)
#else
#define ZELDA_LE_STRUCT
#define RAM16(p) (*(uint16*)(p))
#define RAM32(p) (*(uint32*)(p))
#define RAMS16(p) (*(int16*)(p))
#define RAM16A(p) ((uint16*)(p))
#define RAMS16A(p) ((int16*)(p))
#define RAM32A(p) ((uint32*)(p))
#define ZELDA_LOW_IDX(x) 0
#define ZELDA_HIGH_IDX(x) 1
#endif

// Little-endian scalar wrappers (plain structs on little-endian hosts), usable for pointers on every host.
typedef struct ZELDA_LE_STRUCT { uint16 v; } zle16_t;
typedef struct ZELDA_LE_STRUCT { int16 v; } zles16_t;
typedef struct ZELDA_LE_STRUCT { uint32 v; } zle32_t;
#define RAM16P(p) ((zle16_t*)(p))
#define RAMS16P(p) ((zles16_t*)(p))

// Byte/word views of a variable. For byte lvalues WORD/DWORD read the following bytes as little-endian.
#define BYTE(x) (((uint8*)&(x))[ZELDA_LOW_IDX(x)])
#define HIBYTE(x) (((uint8*)&(x))[ZELDA_HIGH_IDX(x)])
#define WORD(x) RAM16(&(x))
#define DWORD(x) RAM32(&(x))
'''.replace('\n', nl)
s = s.replace(old_block, new_block)
s = s.replace('#define load24(x) ((*(uint32*)&(x))&0xffffff)', '#define load24(x) (RAM32(&(x))&0xffffff)')
s = s.replace('#include <stdbool.h>', '#include <stdbool.h>\n#include <stddef.h>', 1) if 'stddef.h' not in s else s
wr('src/types.h', s)

# ---------------- var maps from variables.h ----------------
vars_h = rd('src/variables.h')
re_scalar = re.compile(r'#define (\w+) \(\*\(uint(16|32)\s*\*\)\((g_ram|g_zenv\.sram)\s*\+\s*(0x[0-9A-Fa-f]+|\d+)\)\)')
le_scalar = {m.group(1): (m.group(3), m.group(4)) for m in re_scalar.finditer(vars_h) if m.group(2) == '16' and m.group(3) == 'g_ram'}
for f in glob.glob('src/*.c'):
    for m in re_scalar.finditer(rd(f)):
        if m.group(2) == '16' and m.group(3) == 'g_ram': le_scalar.setdefault(m.group(1), (m.group(3), m.group(4)))

# ---------------- generic replacements ----------------
A = re.compile(r'\(\*\(uint(16|32)\s*\*\)\((g_ram|g_zenv\.sram)\s*\+\s*([^()]+?)\)\)')
B = re.compile(r'\(\(uint16\s*\*\)\((g_ram|g_zenv\.sram)\s*\+\s*([^()]+?)\)\)')
stats = {'A': 0, 'B': 0, 'asset16': 0, 'sasset16': 0, 'asset32': 0, 'bytes': 0, 'words': 0}
def conv(path):
    s = rd(path); o = s
    s, n = A.subn(lambda m: 'RAM%s(%s+%s)' % (m.group(1), m.group(2), m.group(3)), s); stats['A'] += n
    s, n = B.subn(lambda m: 'RAM16A(%s+%s)' % (m.group(1), m.group(2)), s); stats['B'] += n
    if path.endswith('assets.h'):
        s, n = re.subn(r'\(\(uint16\*\)(g_asset_ptrs\[\d+\])\)', r'RAM16A(\1)', s); stats['asset16'] += n
        s, n = re.subn(r'\(\(int16\*\)(g_asset_ptrs\[\d+\])\)', r'RAMS16A(\1)', s); stats['sasset16'] += n
        s, n = re.subn(r'\(\(uint32\*\)(g_asset_ptrs\[\d+\])\)', r'RAM32A(\1)', s); stats['asset32'] += n
    # BYTE/HIBYTE/WORD on little-endian g_ram variables: address is known, no address-of needed.
    def sub_name(fn, repl):
        nonlocal s
        for name, (base, off) in le_scalar.items():
            pat = re.compile(r'\b%s\(\s*%s\s*\)' % (fn, re.escape(name)))
            s, n = pat.subn(repl(name, off), s)
            stats['bytes' if fn != 'WORD' else 'words'] += n
    sub_name('BYTE', lambda nm, off: '(*(uint8*)(g_ram+(%s)))' % off)
    sub_name('HIBYTE', lambda nm, off: '(*(uint8*)(g_ram+(%s)+1))' % off)
    sub_name('WORD', lambda nm, off: nm)
    if s != o: wr(path, s)
for f in sorted(glob.glob('src/*.h') + glob.glob('src/*.c')):
    if f.endswith(('src/main.c', 'src/types.h')): continue
    conv(f)

# ---------------- structs laid over g_ram ----------------
s = rd('src/variables.h')
for name in ['MovableBlockData', 'RoomBounds', 'OwScrollVars', 'MirrorHdmaVars', 'UploadVram_Row', 'UploadVram_32x32', 'UploadVram_3']:
    s = re.sub(r'typedef struct %s \{' % name, 'typedef struct ZELDA_LE_STRUCT %s {' % name, s)
wr('src/variables.h', s)
print(stats, 'le scalar vars known:', len(le_scalar))
