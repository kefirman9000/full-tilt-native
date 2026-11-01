#!/usr/bin/env python3
"""Detailed PARTOUT(4.0) group dump. usage: datx.py FILE [first last]"""
import struct, sys
b = open(sys.argv[1], 'rb').read()
size, ngroups, body, unk = struct.unpack_from('<iHiH', b, 171)
pos = 171 + 12 + unk
fixed = {0: 2, 2: 2, 13: 0}
lo = int(sys.argv[2]) if len(sys.argv) > 2 else 0
hi = int(sys.argv[3]) if len(sys.argv) > 3 else ngroups-1
groups = []
for g in range(ngroups):
    ents = []
    n = b[pos]; pos += 1
    for _ in range(n):
        t = b[pos]; p = pos + 1
        sz = fixed.get(t, -1)
        if sz < 0:
            sz, = struct.unpack_from('<I', b, p); p += 4
        ents.append((t, b[p:p+sz])); pos = p + sz
    groups.append(ents)
def desc(t, d):
    if t == 0: return f"S0={struct.unpack('<h', d)[0]}"
    if t == 2: return f"S2={struct.unpack('<h', d)[0]}"
    if t == 3: return f"name={d.rstrip(bytes(1)).decode('latin1')!r}"
    if t == 1:
        res, w, h, x, y, bsz, flags = struct.unpack_from('<BHHhhIB', d, 0)
        return f"bmp(r{res} {w}x{h} @{x},{y} f{flags})"
    if t == 5: return f"pal({len(d)})"
    if t == 10:
        a = struct.unpack(f'<{len(d)//2}h', d); return f"sh{list(a[:16])}{'...' if len(a)>16 else ''}"
    if t == 11:
        a = struct.unpack(f'<{len(d)//4}f', d); return f"fl[{' '.join('%g'%v for v in a[:14])}{' ...(%d)'%len(a) if len(a)>14 else ''}]"
    if t == 12: return f"zmap({len(d)})"
    return f"t{t}({len(d)})"
if __name__ == '__main__':
    for g in range(lo, hi+1):
        print(g, ' | '.join(desc(t, d) for t, d in groups[g]))
