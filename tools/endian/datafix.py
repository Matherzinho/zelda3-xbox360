#!/usr/bin/env python3
import re, os
os.chdir('/home/claude/zelda3-xbox360')
def rd(p): return open(p, newline='').read()
def wr(p, s): open(p, 'w', newline='').write(s)
def conv(path, pat_list):
    s = rd(path)
    for pat, rep in pat_list: s = re.sub(pat, rep, s)
    wr(path, s)
rd16 = (r'\*\(uint16 ?\*\)(\((?:[^()]|\([^()]*\))*\)|&file_data\[\d+\]|file_data)', r'RAM16(\1)')
rd32 = (r'\*\(uint32 ?\*\)(\((?:[^()]|\([^()]*\))*\)|&file_data\[\d+\])', r'RAM32(\1)')
conv('src/util.c', [rd16, rd32])
conv('src/spc_player.c', [rd16])
conv('src/audio.c', [rd16, rd32])
conv('src/zelda_cpu_infra.c', [(r'\*\(uint16 ?\*\)\((g_emulated_ram\+0xAE[02])\)', r'RAM16(\1)')])
