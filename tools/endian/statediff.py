#!/usr/bin/env python3
"""Compare two ZSTATE files frame by frame; print first differing regions."""
import sys
R=0x20000; V=0x10000; C=0x200; O=0x220; S=0x2000; P=256*224*4
FR=R+V+C+O+S+P
regions=[('ram',0,R),('vram',R,V),('cgram',R+V,C),('oam',R+V+C,O),('sram',R+V+C+O,S),('pix',R+V+C+O+S,P)]
a=open(sys.argv[1],'rb').read(); b=open(sys.argv[2],'rb').read()
n=min(len(a),len(b))//FR
maxshow=int(sys.argv[3]) if len(sys.argv)>3 else 1
shown=0
for f in range(n):
    fa=a[f*FR:(f+1)*FR]; fb=b[f*FR:(f+1)*FR]
    if fa==fb: continue
    print(f'frame {f}:')
    for name,off,ln in regions:
        x=fa[off:off+ln]; y=fb[off:off+ln]
        if x==y: continue
        d=[i for i in range(ln) if x[i]!=y[i]]
        print(f'  {name}: {len(d)} bytes differ; first at', ', '.join(f'0x{i:x}(ref {x[i]:02x} be {y[i]:02x})' for i in d[:8]))
    shown+=1
    if shown>=maxshow: break
else:
    if shown==0: print(f'all {n} frames IDENTICAL')
