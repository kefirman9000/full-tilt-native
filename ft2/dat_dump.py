#!/usr/bin/env python3
"""Dump named groups and bitmap stats from a Full Tilt! 2 table .DAT (PARTOUT(4.0) container).
Usage: python3 dat_dump.py ALIEN.DAT > alien_groups.txt
Layout follows SpaceCadetPinball's partman.cpp."""
import struct, sys
from collections import Counter
b = open(sys.argv[1], 'rb').read()
assert b[:20] == b'PARTOUT(4.0)RESOURCE', 'not a PARTOUT(4.0) file'
app, desc = b[21:71].rstrip(b'\0').decode(), b[71:171].rstrip(b'\0').decode()
size, ngroups, body, unk = struct.unpack_from('<iHiH', b, 171)
pos = 171 + 12 + unk
fixed = {0: 2, 2: 2, 13: 0}
print(f'# {app} / {desc}: {ngroups} groups')
stats = Counter()
for g in range(ngroups):
    name, ents = None, []
    n = b[pos]; pos += 1
    for _ in range(n):
        t = b[pos]; p = pos + 1
        sz = fixed.get(t, -1)
        if sz < 0:
            sz, = struct.unpack_from('<I', b, p); p += 4
        if t == 3: name = b[p:p+sz].rstrip(b'\0').decode('latin1')
        if t == 1:
            res, w, h, x, y, bsz, flags = struct.unpack_from('<BHHhhIB', b, p)
            stats[(res, 'spliced' if flags & 4 else 'raw')] += 1
        ents.append(t); pos = p + sz
    if name: print(f'{g}\t{name}\ttypes={ents}')
print('# bitmaps by (resolution, kind):', dict(stats))
