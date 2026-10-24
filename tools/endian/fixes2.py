#!/usr/bin/env python3
"""Second pass: tables, structs, address-of LE scalars, vram subscripts, LE-returning calls, native-param functions."""
import re, os, glob
os.chdir('/home/claude/zelda3-xbox360')
def rd(p): return open(p, newline='').read()
def wr(p, s): open(p, 'w', newline='').write(s)

def is_unary_amp(t, k):
    """t[k] == '&': True if it is the address-of operator (not '&&' or binary '&')."""
    if k + 1 < len(t) and t[k + 1] == '&': return False
    j = k - 1
    while j >= 0 and t[j] in ' \t': j -= 1
    if j < 0: return True
    if t[j] == '&': return False
    if t[j] == ')':
        # a cast like '(uint8 *)&x' keeps the '&' unary
        k2 = t.rfind('(', 0, j)
        inner = t[k2 + 1:j] if k2 >= 0 else ''
        if __import__('re').match(r'^\s*(const\s+)?(unsigned\s+)?[A-Za-z_]\w*(\s*\*)+\s*$', inner) or \
           __import__('re').match(r'^\s*(const\s+)?(u?int(8|16|32|64)|zle16_t|zles16_t|zle32_t|char|void|int|size_t)\s*\**\s*$', inner):
            return True
        return False
    if t[j].isalnum() or t[j] in '_]': 
        # 'return &x' is unary
        m = __import__('re').search(r'\breturn$', t[:j + 1])
        return bool(m)
    return True

def match_bracket(t, i):
    d = 0
    while i < len(t):
        if t[i] == '[': d += 1
        elif t[i] == ']':
            d -= 1
            if d == 0: return i
        i += 1
    return -1
def match_paren(t, i):
    d = 0
    while i < len(t):
        if t[i] == '(': d += 1
        elif t[i] == ')':
            d -= 1
            if d == 0: return i
        i += 1
    return -1

def add_v_after_subscripts(s, prefix_re, ndims=1, skip_amp=True):
    """For every occurrence of PREFIX[..] (ndims subscripts), append .v unless address-taken or already .v."""
    out = []; i = 0
    pat = re.compile(prefix_re)
    while True:
        m = pat.search(s, i)
        if not m: out.append(s[i:]); break
        j = m.end() - 1  # at '['
        ok = True
        for d in range(ndims):
            if j >= len(s) or s[j] != '[': ok = False; break
            k = match_bracket(s, j)
            j = k + 1
        end = j
        before = s[:m.start()].rstrip()
        amp = before.endswith('&') and is_unary_amp(before, len(before) - 1)
        out.append(s[i:end])
        if ok and not (skip_amp and amp) and not s.startswith('.v', end) and not s[end:end+1] == '[':
            out.append('.v')
        i = end
    return ''.join(out)

# ---- C: static tables that are used as little-endian data ----
TABLES = {
 'src/attract.c': ['kAttract_CopyToVram_Tab0', 'kAttract_CopyToVram_Tab1'],
 # memcpy sources / byte-viewed tables (found by scan_native.py)
 'src/sprite.c': ['kOam_ResetRegionBases'],
 'src/select_file.c': ['kNamePlayer_Tab1'],
 'src/ending.c': ['kEndingSprites_X', 'kEndingSprites_Y', 'kPolyhedralPalette'],
 'src/hud.c': ['kHudAbilityText', 'kHudGlovesText', 'kProgressIconPendantsBg', 'kProgressIconCrystalsBg',
               'kHudBottlesItemText', 'kHudMushroomItemText', 'kHudMirrorItemText', 'kHudFluteItemText',
               'kHudBowItemText', 'kHudItemText', 'kNotAssignedItemText', 'kHudItemBoxTab1', 'kHudItemBoxTab2',
               'kUpdateMagicPowerTilemap', 'kHudTilemapLeftPart', 'kHudTilemapRightPart',
               'kBottleMenuTop', 'kBottleMenuTop2', 'kBottleMenuBottom', 'kBytesForNewTile0xC_TopOfR',
               'kBytesForNewTile0xD_BottomofR', 'kBytesForNewTile0xE_TopOfL', 'kBytesForNewTile0xF_BottomofL',
               'kHudEquipmentDungeonItemText', 'kHudInventoryBg'],
 'src/load_gfx.c': ['kPaletteFilteringBits'],
 'src/dungeon.c': ['kBossRooms'],
 'src/tagalong.c': ['kTagalong_IndoorRooms', 'kTagalong_OutdoorRooms'],
}
for path, names in TABLES.items():
    s = rd(path)
    for nm in names:
        m = re.search(r'\b(u?)int16 (' + nm + r')((?:\[[^\]]*\])+)\s*=', s)
        if not m:
            print('TABLE NOT FOUND', path, nm); continue
        ndims = m.group(3).count('[')
        s = s[:m.start()] + ('zle16_t ' if m.group(1) else 'zles16_t ') + m.group(2) + m.group(3) + s[m.start() + len(m.group(0)) - 1:]
        decl_end = s.index('=', m.start())
        head, tail = s[:decl_end], s[decl_end:]
        # don't touch the declaration's own [dims]
        tail = add_v_after_subscripts(tail, r'(?<![\w.>])' + nm + r'\[', ndims)
        s = head + tail
    wr(path, s)

# ---- hud.c: ItemBoxGfx holds tile words that go to VRAM ----
s = rd('src/hud.c')
s = s.replace('typedef struct ItemBoxGfx {\r\n  uint16 v[4];', 'typedef struct ItemBoxGfx {\r\n  zle16_t v[4];')
s = s.replace('typedef struct ItemBoxGfx {\n  uint16 v[4];', 'typedef struct ItemBoxGfx {\n  zle16_t v[4];')
wr('src/hud.c', s)

# ---- structs laid over RAM made only of arrays: zle16_t members instead of the storage-order attribute ----
s = rd('src/variables.h')
s = s.replace('typedef struct ZELDA_LE_STRUCT UploadVram_Row {', 'typedef struct UploadVram_Row {')
s = s.replace('typedef struct ZELDA_LE_STRUCT UploadVram_32x32 {', 'typedef struct UploadVram_32x32 {')
s = s.replace('typedef struct ZELDA_LE_STRUCT UploadVram_3 {', 'typedef struct UploadVram_3 {')
s = re.sub(r'(typedef struct UploadVram_Row \{\s*)uint16 col\[32\];', r'\1zle16_t col[32];', s)
s = re.sub(r'(typedef struct UploadVram_3 \{\s*uint8 pad\[256\];\s*)uint16 data\[4\];', r'\1zle16_t data[4];', s)
wr('src/variables.h', s)

GAME_C = [p for p in glob.glob('src/*.c') if not p.endswith(('main.c', 'config.c', 'opengl.c', 'glsl_shader.c', 'audio.c', 'spc_player.c', 'util.c'))]
for path in GAME_C:
    s = rd(path); o = s
    s = s.replace('((zle16_t*)((uint8*)&uvram + offsetof(UploadVram_3, data)))', '&uvram.data[0]')
    s = add_v_after_subscripts(s, r'\.col\[', 1)
    s = add_v_after_subscripts(s, r'uvram\.data\[', 1)
    # direct VRAM / CGRAM / OAM subscripts
    s = add_v_after_subscripts(s, r'g_zenv\.vram\[', 1)
    s = add_v_after_subscripts(s, r'g_zenv\.ppu->(?:cgram|oam|vram)\[', 1)
    # functions returning little-endian pointers, subscripted directly
    for fn in ['SrcPtr', 'GetMap16toMap8Table']:
        pat = re.compile(r'\b' + fn + r'\(')
        out = []; i = 0
        while True:
            m = pat.search(s, i)
            if not m: out.append(s[i:]); break
            k = match_paren(s, m.end() - 1)
            out.append(s[i:k + 1]); i = k + 1
            if s.startswith('[', i):
                j = match_bracket(s, i)
                before = ''.join(out).rstrip()
                out.append(s[i:j + 1]); i = j + 1
                if not (before.endswith('&') and is_unary_amp(before, len(before) - 1)) and not s.startswith('.v', i): out.append('.v')
        s = ''.join(out)
    if s != o: wr(path, s)

assets = {}
for m in re.finditer(r'#define (k\w+) RAM16A\((g_asset_ptrs\[\d+\])\)', rd('src/assets.h')):
    assets[m.group(1)] = 'RAM16P(%s)' % m.group(2)
for path in GAME_C:
    s = rd(path); o = s
    # 16-bit casts of byte pointers: everything such casts point at in game code is little-endian data
    s = re.sub(r'\(\s*const\s+uint16\s*\*\s*\)', '(const zle16_t *)', s)
    s = re.sub(r'\(\s*uint16\s*\*\s*\)', '(zle16_t *)', s)
    # subscript / deref of a typed LE pointer expression
    s = add_v_after_subscripts(s, r'\(RAM16P\(g_ram\+0x[0-9A-Fa-f]+\)\)\[', 1)
    s = re.sub(r'\*\((sign8\(\w+\) \? RAM16P\([^()]*\) : RAM16P\([^()]*\))\)', r'(\1)->v', s)
    # byte views of a converted element
    s = re.sub(r'\bBYTE\((\w+)->v\)', r'(*(uint8*)(\1))', s)
    s = re.sub(r'\bHIBYTE\((\w+)->v\)', r'(((uint8*)(\1))[1])', s)
    s = re.sub(r'\bWORD\((\w+)->v\)', r'\1->v', s)
    # sync helpers take raw bytes
    s = re.sub(r'EmuSyncMemoryRegion\(\(\(zle32_t\*\)\((g_ram\+0x[0-9A-Fa-f]+)\)\)', r'EmuSyncMemoryRegion((\1)', s)
    s = re.sub(r'EmuSyncMemoryRegion\(RAM16P\((g_ram\+0x[0-9A-Fa-f]+)\)', r'EmuSyncMemoryRegion((\1)', s)
    # local aliases of asset tables become pointer aliases
    s = re.sub(r'(#define \w+ )(k\w+)(\s*(?:\r?\n|//))', lambda m: m.group(1) + assets.get(m.group(2), m.group(2)) + m.group(3), s)
    if path.endswith('hud.c'):
        s = add_v_after_subscripts(s, r'\bsrc->v\[', 1)
    if s != o: wr(path, s)

def deref_casts(s):
    pat = re.compile(r'(?<![\w)\]])\*\s*\(\s*((?:const\s+)?zle16_t\s*\*)\s*\)\s*')
    out = []; i = 0
    while True:
        m = pat.search(s, i)
        if not m: out.append(s[i:]); break
        j = m.end()
        if s[j] == '(':
            e = match_paren(s, j) + 1
        else:
            mm = re.match(r'&?\s*\w+', s[j:]); e = j + mm.end()
            while e < len(s) and s[e] == '[': e = match_bracket(s, e) + 1
        out.append(s[i:m.start()]); out.append('((%s)%s)->v' % (m.group(1), s[j:e])); i = e
    return ''.join(out)
for path in GAME_C:
    s = rd(path); n = deref_casts(s)
    if n != s: wr(path, n)
