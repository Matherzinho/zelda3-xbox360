#!/usr/bin/env python3
"""Rewrite &ARR[i] and bare ARR (pointer decay) for LE 16-bit array macros / asset tables; &SCALAR for LE scalars."""
import re, glob, os, sys
os.chdir('/home/claude/zelda3-xbox360')
def rd(p): return open(p, newline='').read()
def wr(p, s): open(p, 'w', newline='').write(s)
defs = {}   # name -> pointer expression
sdefs = {}  # LE scalar name -> byte address expr
sptr = {}   # LE scalar name -> typed pointer expr
for f in glob.glob('src/*.h') + glob.glob('src/*.c'):
    t = rd(f)
    for m in re.finditer(r'#define (\w+) RAM16A\(((?:g_ram|g_zenv\.sram)\+[^\r\n]+?)\)\s*(?:\r?\n|//)', t):
        defs[m.group(1)] = 'RAM16P(%s)' % m.group(2)
    for m in re.finditer(r'#define (k\w+) RAM16A\((g_asset_ptrs\[\d+\])\)', t):
        defs[m.group(1)] = 'RAM16P(%s)' % m.group(2)
    for m in re.finditer(r'#define (k\w+) RAMS16A\((g_asset_ptrs\[\d+\])\)', t):
        defs[m.group(1)] = 'RAMS16P(%s)' % m.group(2)
    for m in re.finditer(r'#define (\w+) RAM(16|32)\((g_ram\+[^\r\n]+?)\)\s*(?:\r?\n|//)', t):
        sdefs[m.group(1)] = '(%s)' % m.group(3)
        sptr[m.group(1)] = ('RAM16P(%s)' if m.group(2) == '16' else '((zle32_t*)(%s))') % m.group(3)

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

def match_bracket(s, i):
    d = 0
    while i < len(s):
        if s[i] == '[': d += 1
        elif s[i] == ']':
            d -= 1
            if d == 0: return i
        i += 1
    return -1
CASTS = [r'\(\s*uint16\s*\*\s*\)\s*(?=&?\s*\(?\s*g_ram)', r'\(\s*int16\s*\*\s*\)\s*(?=&?\s*\(?\s*g_ram)']
SPECIAL = [('&(&ow_scroll_vars0.ystart)[ya]', '((zle16_t*)((uint8*)&ow_scroll_vars0 + offsetof(OwScrollVars, ystart)) + (ya))'), ('&uvram.data[0]', '((zle16_t*)((uint8*)&uvram + offsetof(UploadVram_3, data)))')]
def fix_text(t):
    for a, b in SPECIAL: t = t.replace(a, b)
    def deref_cast(t, ctype, mac):
        pat = re.compile(r'(?<![\w)\]])\*\s*\(\s*' + ctype + r'\s*\*\s*\)\s*(?=&\s*g_ram\[|\(\s*g_ram\s*\+)')
        while True:
            m = pat.search(t)
            if not m: return t
            i = m.end()
            if t[i] == '&':
                k = t.index('[', i); j = match_bracket(t, k); end = j + 1
                operand = t[i:end]
            else:
                d = 0; end = i
                while True:
                    if t[end] == '(': d += 1
                    elif t[end] == ')':
                        d -= 1
                        if d == 0: end += 1; break
                    end += 1
                operand = t[i:end]
            t = t[:m.start()] + '%s(%s)' % (mac, operand) + t[end:]
    t = deref_cast(t, 'uint16', 'RAM16'); t = deref_cast(t, 'int16', 'RAMS16')
    t = re.sub(CASTS[0], '(zle16_t *)', t); t = re.sub(CASTS[1], '(zles16_t *)', t)
    out = []
    for line in t.split('\n'):
        if line.lstrip().startswith('#define') or line.lstrip().startswith('#include'):
            out.append(line); continue
        # &ARR[expr]
        for name, ptr in defs.items():
            if name not in line: continue
            if ptr.startswith('RAM16P(g_'):
                base = ptr[len('RAM16P('):-1]
                for fn, mk in (('BYTE', lambda b, e: '(*(uint8*)(%s+2*(%s)))' % (b, e)),
                               ('HIBYTE', lambda b, e: '(*(uint8*)(%s+2*(%s)+1))' % (b, e)),
                               ('WORD', lambda b, e, n=name: '%s[%s]' % (n, e))):
                    pat = re.compile(r'\b' + fn + r'\(\s*' + re.escape(name) + r'\[')
                    while True:
                        m = pat.search(line)
                        if not m: break
                        j = match_bracket(line, m.end() - 1)
                        k = line.find(')', j)
                        if k < 0: break
                        line = line[:m.start()] + mk(base, line[m.end():j]) + line[k + 1:]
            pat = re.compile(r'&\s*\b' + re.escape(name) + r'\[')
            pos = 0
            while True:
                m = pat.search(line, pos)
                if not m: break
                if not is_unary_amp(line, m.start()): pos = m.end(); continue
                j = match_bracket(line, m.end() - 1)
                line = line[:m.start()] + '(%s + (%s))' % (ptr, line[m.end():j]) + line[j + 1:]
            # bare name (not followed by '[' or word char, not preceded by . or ->)
            line = re.sub(r'(?<![\w.>])' + re.escape(name) + r'\b(?!\s*\[)', lambda m, p=ptr: p, line)
        for name, addr in sdefs.items():
            if name not in line: continue
            line = re.sub(r'\(\s*uint8\s*\*\s*\)\s*&\s*' + re.escape(name) + r'\b(?!\s*[\[(\w)])', lambda m, a=addr: a, line)
            line = re.sub(r'\(\(\s*uint8\s*\*\s*\)\s*&\s*' + re.escape(name) + r'\)\[(\d)\]',
                          lambda m, a=addr: '(*(uint8*)(g_ram+%s+%s))' % (a[len('(g_ram+'):-1], m.group(1)), line)
            line = re.sub(r'\(\s*(u?)int16\s*\*\s*\)\s*&\s*' + re.escape(name) + r'\b(?!\s*[\[(\w])',
                          lambda m, a=addr: '(zle%s16_t *)%s' % ('' if m.group(1) else 's', a), line)
            line = re.sub(r'(^|[(,=;{?:!|+\-*/<>~]|\breturn)(\s*)&(?!&)\s*' + re.escape(name) + r'\b(?!\s*[\[(\w])', lambda m, a=sptr[name]: m.group(1) + m.group(2) + a, line)
        out.append(line)
    return '\n'.join(out)
if __name__ == '__main__':
    print(len(defs), 'ptr names;', len(sdefs), 'scalar names')
    for p in sys.argv[1:]:
        t = rd(p); n = fix_text(t)
        if n != t: wr(p, n); print('fixed', p)
