#!/usr/bin/env python3
"""Dump a Full Tilt! 2 table exe's rule table, and the points each rule function scores.

usage: ft2rules.py hero.exe [score_helper_va]

FT2 table exes (32-bit PE, Borland) keep a rule table in their data section: 16-byte entries
{tag*, rule function, score count, int32 scores*}, where each tag is a 28-byte record whose first field points to the
component name. Alien.exe fills the score lists; hero.exe leaves them empty and passes its points straight to a
score helper, score(component, points), from inside each rule function. This tool finds the table from the "drain"
tag, prints it, then (needs objdump from binutils) lists the immediate values passed to the score helper by each rule
function and the control.cpp functions it calls, two levels deep. "@addr" marks a value scored by a called helper.

Message codes in FT2 rule functions: 76 = collision, 1017 = reset (Space Cadet: 63 and 1024).
hero.exe: score helper 0x401a38, current mode 0x43d088 (modes 1-7), mode start 0x40787c(mode), mode end 0x407e08.
"""
import bisect
import collections
import re
import struct
import subprocess
import sys


class PE:
    def __init__(self, path):
        d = self.d = open(path, 'rb').read()
        o = struct.unpack_from('<I', d, 0x3c)[0]
        n = struct.unpack_from('<H', d, o + 6)[0]
        opt = struct.unpack_from('<H', d, o + 20)[0]
        self.base = struct.unpack_from('<I', d, o + 24 + 28)[0]
        self.secs = []
        for i in range(n):
            name, vs, va, rs, ra = struct.unpack_from('<8sIIII', d, o + 24 + opt + 40 * i)
            self.secs.append((name.rstrip(b'\0').decode(), va + self.base, vs, ra, rs))

    def off(self, va):
        for _, v, vs, ra, rs in self.secs:
            if v <= va < v + max(vs, rs):
                return ra + va - v if va - v < rs else None

    def va(self, off):
        for _, v, vs, ra, rs in self.secs:
            if ra <= off < ra + rs:
                return v + off - ra

    def u32(self, va):
        o = self.off(va)
        return struct.unpack_from('<I', self.d, o)[0] if o is not None else None

    def cstr(self, va):
        o = self.off(va)
        if o is None:
            return None
        return self.d[o:self.d.find(b'\0', o)].decode('latin1')

    def code_range(self):
        _, v, vs, _, rs = self.secs[0]
        return v, v + rs


def rule_table(pe):
    lo, hi = pe.code_range()

    def entry(a):
        t, f, c, s = (pe.u32(a + 4 * i) for i in range(4))
        if None in (t, f, c, s) or not lo <= f < hi or c > 64:
            return None
        name_ptr = pe.u32(t)
        name = pe.cstr(name_ptr) if name_ptr else None
        if not name or not name.isprintable():
            return None
        scores = [struct.unpack('<i', struct.pack('<I', pe.u32(s + 4 * i)))[0] for i in range(c)]
        return name, f, scores

    for m in re.finditer(rb'\0drain\0', pe.d):
        name_va = pe.va(m.start() + 1)
        for tag_match in re.finditer(re.escape(struct.pack('<I', name_va)), pe.d):
            tag = pe.va(tag_match.start())
            for ref in re.finditer(re.escape(struct.pack('<I', tag)), pe.d):
                a = pe.va(ref.start())
                if not entry(a):
                    continue
                while entry(a - 16):
                    a -= 16
                rows = []
                while entry(a):
                    rows.append((a,) + entry(a))
                    a += 16
                return rows
    return []


def score_scan(path, pe, rows, helper):
    lo, hi = pe.code_range()
    asm = subprocess.run(['objdump', '-d', '-M', 'intel', f'--start-address={lo:#x}', f'--stop-address={hi:#x}', path],
                         capture_output=True, text=True).stdout
    lines = []
    for line in asm.splitlines():
        m = re.match(r'\s+([0-9a-f]+):\s+(?:[0-9a-f]{2} )+\s*(.*)', line)
        if m and m.group(2).strip():
            lines.append((int(m.group(1), 16), m.group(2).strip()))
    index = {a: i for i, (a, _) in enumerate(lines)}
    starts = sorted(a for i, (a, t) in enumerate(lines[:-1])
                    if t.startswith('push   ebp') and lines[i + 1][1].startswith('mov    ebp,esp'))
    funcs = [r[2] for r in rows]
    control_lo, control_hi = min(funcs) & ~0xfff, (max(funcs) | 0xfff) + 0x1000

    def body(f):
        j = bisect.bisect_right(starts, f)
        end = starts[j] if j < len(starts) else f + 0x400
        i = index.get(f)
        out = []
        while i is not None and i < len(lines) and lines[i][0] < end:
            out.append(lines[i])
            i += 1
        return out

    def scores(f, depth, seen):
        out = []
        b = body(f)
        for k, (_, t) in enumerate(b):
            m = re.match(r'call\s+0x([0-9a-f]+)$', t)
            if not m:
                continue
            callee = int(m.group(1), 16)
            if callee == helper:
                pushes = [x[1] for x in b[max(0, k - 4):k] if x[1].startswith('push')]
                arg = pushes[-2].split()[-1] if len(pushes) >= 2 else '?'
                try:
                    arg = str(int(arg, 16))
                except ValueError:
                    pass
                out.append(arg if depth == 0 else f'{arg}@{f:x}')
            elif control_lo <= callee < control_hi and depth < 2 and callee not in seen:
                out += scores(callee, depth + 1, seen | {callee})
        return out

    grouped = collections.OrderedDict()
    for _, name, f, _ in rows:
        grouped.setdefault(f, []).append(name)
    for f, names in grouped.items():
        print(f'{f:#x} {" ".join(names)}\n         scores {scores(f, 0, {f})}')


def main():
    path = sys.argv[1]
    pe = PE(path)
    rows = rule_table(pe)
    print(f'# rule table: {len(rows)} entries')
    for a, name, f, sc in rows:
        print(f'{a:#x} {name:28s} func {f:#x} scores {sc}')
    if len(sys.argv) > 2:
        print('\n# points passed to the score helper, per rule function')
        score_scan(path, pe, rows, int(sys.argv[2], 16))


if __name__ == '__main__':
    main()
