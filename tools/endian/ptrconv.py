#!/usr/bin/env python3
"""Rewrite uint16/int16 pointer variables and params in given files to zle16_t/zles16_t, with uses fixed."""
import re, sys, os
os.chdir('/home/claude/zelda3-xbox360')
def rd(p): return open(p, newline='').read()
def wr(p, s): open(p, 'w', newline='').write(s)

ITEM = r'\*\s*\w+(?:\s*=\s*(?:[^,;()]|\((?:[^()]|\([^()]*\))*\))*)?'
DECL = re.compile(r'\b(const\s+)?(u?)int16(\s*)(' + ITEM + r'(?:\s*,\s*' + ITEM + r')*)')


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
    # s[i]=='[' ; return index of matching ']'
    d = 0
    while i < len(s):
        if s[i] == '[': d += 1
        elif s[i] == ']':
            d -= 1
            if d == 0: return i
        i += 1
    return -1

def fix_uses(body, name):
    pre = r'(?<![\w.>])'
    # &NAME[expr] -> (NAME + (expr))
    out = []; i = 0
    pat = re.compile(r'&\s*' + pre.replace('(?<![', '(?<![') + re.escape(name) + r'\[')
    pat = re.compile(r'&\s*\b' + re.escape(name) + r'\[')
    while True:
        m = pat.search(body, i)
        if not m: out.append(body[i:]); break
        if not is_unary_amp(body, m.start()):
            out.append(body[i:m.end()]); i = m.end(); continue
        j = match_bracket(body, m.end() - 1)
        out.append(body[i:m.start()]); out.append('(%s + (%s))' % (name, body[m.end():j])); i = j + 1
    body = ''.join(out)
    # NAME[expr] -> NAME[expr].v   (also NAME[expr][...] not expected)
    out = []; i = 0
    pat = re.compile(pre + re.escape(name) + r'\[')
    while True:
        m = pat.search(body, i)
        if not m: out.append(body[i:]); break
        j = match_bracket(body, m.end() - 1)
        out.append(body[i:j + 1])
        if not body[j + 1:j + 3] == '.v': out.append('.v')
        i = j + 1
    body = ''.join(out)
    # *++NAME / *--NAME
    body = re.sub(r'(?<![\w)\]])\*\s*(\+\+|--)\s*' + re.escape(name) + r'\b', r'(\1%s)->v' % name, body)
    # *NAME++ / *NAME-- / *NAME  (unary deref)
    body = re.sub(r'(?<![\w)\]])\*\s*' + re.escape(name) + r'(\+\+|--)', r'(%s\1)->v' % name, body)
    body = re.sub(r'(?<![\w)\]])\*\s*' + re.escape(name) + r'\b(?!\s*[\[(])', r'%s->v' % name, body)
    # BYTE/HIBYTE/WORD applied to a converted element
    for fn, mk in (('BYTE', lambda e: '(*(uint8*)&%s)' % e), ('HIBYTE', lambda e: '(((uint8*)&%s)[1])' % e), ('WORD', lambda e: e + '.v')):
        pat = re.compile(r'\b' + fn + r'\(\s*' + re.escape(name) + r'\[')
        while True:
            m = pat.search(body)
            if not m: break
            j = match_bracket(body, m.end() - 1)
            if body[j + 1:j + 3] != '.v' or body[j + 3:j + 4] != ')':
                body = body[:m.start()] + fn + '#(' + body[m.end() - len(name) - 1:]; continue
            elem = '%s[%s]' % (name, body[m.end():j])
            if fn == 'WORD': body = body[:m.start()] + elem + '.v' + body[j + 4:]
            else: body = body[:m.start()] + mk(elem) + body[j + 4:]
    body = body.replace('BYTE#(', 'BYTE(').replace('HIBYTE#(', 'HIBYTE(').replace('WORD#(', 'WORD(')
    return body

EXCLUDE_FUNCS = ('GetTileAttribute', 'Sprite_GetTileAttribute')

def process(path):
    s = rd(path)
    lines = s.split('\n')
    # function ranges: header line at col0 containing '(' and ending with '{' (maybe comment), through next '^}'
    out = []; i = 0; nfix = 0; names_total = 0
    while i < len(lines):
        ln = lines[i]
        if re.match(r'^[A-Za-z_][^;]*\)\s*\{', ln) and not ln.startswith(('typedef', 'struct', 'enum')) and not any(re.search(r'\b%s\(' % f, ln) for f in EXCLUDE_FUNCS):
            j = i
            while j < len(lines) and not lines[j].startswith('}'): j += 1
            func = '\n'.join(lines[i:j + 1])
            names = []
            def declsub(m):
                const, u, sp, rest = m.group(1) or '', m.group(2), m.group(3), m.group(4)
                ns = re.findall(r'(?:^|,\s*)\*\s*(\w+)', rest)
                names.extend(ns)
                rest = re.sub(r'(^|,\s*)\*(\s*)(\w+)', lambda mm: mm.group(1) + '*\x00' + mm.group(2) + mm.group(3), rest)
                return '%s%szle%s16_t%s%s' % (const, '', '' if u else 's', sp or ' ', rest)
            new = DECL.sub(declsub, func)
            for nm in dict.fromkeys(names):
                # fix uses (everything except the declarations: declarations have no [ or deref issues)
                new = fix_uses(new, nm)
            new = new.replace('\x00', '')
            if new != func: nfix += 1
            names_total += len(names)
            out.append(new); i = j + 1
        else:
            if re.match(r'^[A-Za-z_][^=;{]*\([^;]*\)\s*;\s*(//.*)?\s*$', ln) and not ln.startswith(('typedef', 'extern', '#')) and not any(re.search(r'\b%s\(' % f, ln) for f in EXCLUDE_FUNCS):
                ln = DECL.sub(lambda m: '%szle%s16_t%s%s' % (m.group(1) or '', '' if m.group(2) else 's', m.group(3) or ' ', m.group(4)), ln)
            out.append(ln); i += 1
    wr(path, '\n'.join(out))
    return nfix, names_total

if False:
    for p in sys.argv[1:]:
        print(p, process(p))

def process_header(path):
    s = rd(path)
    def sub(m):
        const, u, sp, rest = m.group(1) or '', m.group(2), m.group(3), m.group(4)
        return '%szle%s16_t%s%s' % (const, '' if u else 's', sp or ' ', rest)
    n = '\n'.join(l if any(re.search(r'\b%s\(' % f, l) for f in EXCLUDE_FUNCS) else DECL.sub(sub, l) for l in s.split('\n'))
    wr(path, n); return s != n

if __name__ == '__main__':
    for p in sys.argv[1:]:
        if p.endswith('.h'): print(p, process(p), process_header(p))
        else: print(p, process(p))
