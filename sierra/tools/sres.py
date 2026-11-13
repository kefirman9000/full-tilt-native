#!/usr/bin/env python3
"""
sres.py - read Sierra/Dynamix resource archives (RESOURCE.MAP + RESOURCE.00n), as used by
3-D Ultra Pinball (1995) and its sequels.

RESOURCE.MAP: u8[4] header, u16 volume count, then per volume: NUL-terminated file name,
u16 entry count, entries of (u32 hash, u32 offset into that volume).
Each entry in a volume: char[13] name (NUL padded), u32 size, size bytes of data.

usage: sres.py GAMEDIR list
       sres.py GAMEDIR extract OUTDIR [PATTERN]
"""
import sys, os, struct, fnmatch

def read_map(gamedir):
    m = open(os.path.join(gamedir, 'RESOURCE.MAP'), 'rb').read()
    nvol = struct.unpack_from('<H', m, 4)[0]
    p = 6
    vols = []
    for _ in range(nvol):
        e = m.index(b'\0', p)
        name = m[p:e].decode('latin1'); p = e + 1
        n = struct.unpack_from('<H', m, p)[0]; p += 2
        ents = [struct.unpack_from('<II', m, p + 8*i) for i in range(n)]
        p += 8 * n
        vols.append((name, ents))
    return vols

def entries(gamedir):
    """yield (volume, name, offset, size, hash)"""
    for vol, ents in read_map(gamedir):
        path = os.path.join(gamedir, vol)
        with open(path, 'rb') as f:
            for h, off in ents:
                f.seek(off)
                hd = f.read(17)
                name = hd[:13].split(b'\0')[0].decode('latin1')
                size = struct.unpack_from('<I', hd, 13)[0]
                yield vol, name, off + 17, size, h

def read(gamedir, vol, off, size):
    with open(os.path.join(gamedir, vol), 'rb') as f:
        f.seek(off); return f.read(size)

if __name__ == '__main__':
    g, cmd = sys.argv[1], sys.argv[2]
    if cmd == 'list':
        for vol, name, off, size, h in entries(g):
            print('%-13s %-14s %9d %08x' % (vol, name, size, h))
    elif cmd == 'extract':
        out = sys.argv[3]; pat = sys.argv[4] if len(sys.argv) > 4 else '*'
        os.makedirs(out, exist_ok=True)
        for vol, name, off, size, h in entries(g):
            if fnmatch.fnmatch(name.upper(), pat.upper()):
                open(os.path.join(out, name), 'wb').write(read(g, vol, off, size))
