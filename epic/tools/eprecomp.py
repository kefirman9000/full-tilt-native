#!/usr/bin/env python3
"""
eprecomp.py - static recompiler for Epic Pinball (1993) DOS executables -> C99.

Reads a 16-bit real-mode MZ executable (EPn.EXE / PINBALL.EXE) from the user's
own game folder and writes one C file in which every reachable basic block of
x86 code becomes a C function operating on the runtime CPU state (epic_rt.h).

Only machine code is translated. Game data (graphics, tables, text) is NOT
copied into the output: the runtime loads the original EXE image from the
user's game folder at run time, exactly where DOS would have put it.

usage: eprecomp.py GAME.EXE PREFIX OUT.c [--seeds seeds.txt]
"""
import sys, struct, re, zlib, argparse, collections
from capstone import Cs, CS_ARCH_X86, CS_MODE_16
from capstone.x86 import *

# ----------------------------------------------------------------------------
# MZ loading
# ----------------------------------------------------------------------------
def load_mz(path):
    d = open(path, 'rb').read()
    (sig, lastp, npages, nrel, hdr, minalloc, maxalloc, ss, sp, csum, ip, cs,
     relo, ov) = struct.unpack('<2sHHHHHHHHHHHHH', d[:28])
    assert sig in (b'MZ', b'ZM'), 'not an MZ executable'
    hsz = hdr * 16
    isz = npages * 512 - ((512 - lastp) if lastp else 0) - hsz
    img = d[hsz:hsz + isz]
    rel = [struct.unpack('<HH', d[relo + 4*i: relo + 4*i + 4]) for i in range(nrel)]
    relpos = set()
    for off, seg in rel:
        relpos.add(seg*16 + off)
    return dict(img=img, rel=rel, relpos=relpos, ss=ss, sp=sp, cs=cs, ip=ip,
                minalloc=minalloc, maxalloc=maxalloc, crc=zlib.crc32(img) & 0xffffffff)

def load_masi(path):
    """MASI REAL16 driver module (MDRVnnnR.MUS): raw code loaded at offset 0 of a segment by the game.
    Header dwords from 0x18: file size, header size, ?, then entry-point offsets (from the start of the file)."""
    d = open(path, 'rb').read()
    assert d[:8] == b'\xfeREAL16\xfe', 'not a MASI REAL16 module'
    f = struct.unpack('<12I', d[0x18:0x48])
    entries = [x for x in f[3:7] if f[1] <= x < len(d)]
    return dict(img=d, rel=[], relpos=set(), ss=0, sp=0, cs=0, ip=entries[0] if entries else f[1],
                minalloc=0, maxalloc=0, crc=zlib.crc32(d) & 0xffffffff, raw=True, entries=entries)

md = Cs(CS_ARCH_X86, CS_MODE_16)
md.detail = True

# ----------------------------------------------------------------------------
# discovery
# ----------------------------------------------------------------------------
TERMINATORS = {'jmp', 'ljmp', 'ret', 'retf', 'iret', 'hlt'}
def is_branch(mn):
    return mn in ('jmp', 'ljmp', 'call', 'lcall', 'jcxz', 'jecxz') or mn.startswith('j') or mn.startswith('loop')

class Program:
    def __init__(self, path, seeds=()):
        with open(path, 'rb') as fh:
            magic = fh.read(8)
        self.m = load_masi(path) if magic == b'\xfeREAL16\xfe' else load_mz(path)
        self.raw = bool(self.m.get('raw'))
        self.img = self.m['img']
        self.n = len(self.img)
        self.ins = {}          # (seg,off) -> capstone insn
        self.spec = set()      # (seg,off) entries reached only speculatively
        self.entries = set()   # block entry points
        self.extsegs = set()   # far segments outside the image (runtime-loaded drivers)
        self.jtables = []
        self.seeds = list(seeds)
        self.discover()

    def lin(self, seg, off):
        return seg*16 + off

    def decode(self, seg, off):
        k = (seg, off)
        if k in self.ins:
            return self.ins[k]
        a = self.lin(seg, off)
        if a < 0 or a >= self.n:
            return None
        i = next(md.disasm(self.img[a:a+16], off), None)
        if i is not None:
            self.ins[k] = i
        return i

    def code_ok(self, seg, off):
        a = self.lin(seg, off)
        return 0 <= a < self.n - 1

    def far_target_is_external(self, seg):
        # Sound/music drivers are loaded at run time into the segment just past
        # the image; calls into them are handled natively by the runtime.
        return seg*16 >= self.n - 0x100

    def walk(self, start, speculative):
        work = [start]
        newly = []
        while work:
            seg, off = work.pop()
            if not self.code_ok(seg, off):
                continue
            if (seg, off) in self.entries:
                if not speculative and (seg, off) in self.spec:
                    self.spec.discard((seg, off))
                    # re-walk as confirmed so its successors get confirmed too
                else:
                    continue
            self.entries.add((seg, off))
            if speculative:
                self.spec.add((seg, off))
            newly.append((seg, off))
            o = off
            for _ in range(4000):
                i = self.decode(seg, o)
                if i is None:
                    break
                mn = i.mnemonic
                nxt = (o + i.size) & 0xffff
                if is_branch(mn):
                    ops = i.operands
                    if mn in ('lcall', 'ljmp') and len(ops) == 2:
                        tseg, toff = ops[0].imm & 0xffff, ops[1].imm & 0xffff
                        if self.far_target_is_external(tseg):
                            if not speculative:
                                self.extsegs.add(tseg)
                        elif not speculative or tseg == seg:
                            work.append((tseg, toff))
                    elif ops and ops[0].type == X86_OP_IMM:
                        work.append((seg, ops[0].imm & 0xffff))
                    else:
                        self.indirect(seg, o, i, work)
                    if mn in ('jmp', 'ljmp'):
                        break
                    # fall-through of call / conditional jump is a new block entry
                    work.append((seg, nxt))
                    break
                if mn in TERMINATORS:
                    break
                if mn == 'int':
                    # INT 21h/4Ch and INT 20h never return: do not decode the bytes after them as code
                    if not (i.op_str == '0x20' or (i.op_str == '0x21' and self.sets_ah_4c(seg, o))):
                        work.append((seg, nxt))
                    break
                o = nxt
        return newly

    def sets_ah_4c(self, seg, off):
        for sz in (2, 3):
            p = self.ins.get((seg, (off - sz) & 0xffff))
            if p is not None and p.size == sz:
                t = p.mnemonic + ' ' + p.op_str
                if t == 'mov ah, 0x4c' or (t.startswith('mov ax, 0x4c') and len(t) == len('mov ax, 0x4c00')):
                    return True
        return False

    def indirect(self, seg, off, i, work):
        """Recognise `mov bx, cs:[bx+TABLE]` ; `jmp bx` jump tables."""
        # look back a few instructions
        prev = []
        o = off
        for back in range(1, 12):
            for sz in range(1, 7):
                p = self.ins.get((seg, (off - sz) & 0xffff))
                if p is not None and ((p.address + p.size) & 0xffff) == o:
                    prev.append(p); o = p.address; break
            else:
                break
        tbl = None; limit = 128
        for p in prev:
            if p.mnemonic == 'mov' and len(p.operands) == 2 and p.operands[1].type == X86_OP_MEM:
                mm = p.operands[1].mem
                if mm.base != 0 and mm.disp:
                    tbl = mm.disp & 0xffff
            if p.mnemonic == 'sub' and p.operands[1].type == X86_OP_IMM:
                limit = max(1, 0x100 - (p.operands[1].imm & 0xff))
        if tbl is None:
            self.jtables.append((seg, off, None, 0))
            return
        targets = []
        lo = 0xffff
        for k in range(limit):
            ta = tbl + 2*k
            if ta >= lo:
                break
            a = self.lin(seg, ta)
            if a + 2 > self.n:
                break
            v = struct.unpack('<H', self.img[a:a+2])[0]
            if not self.code_ok(seg, v):
                break
            targets.append(v)
            lo = min(lo, v)
        for v in targets:
            work.append((seg, v))
        self.jtables.append((seg, off, tbl, len(targets)))

    def immediate_seeds(self):
        """Code addresses that are only ever stored (interrupt handlers etc.)."""
        out = set()
        rng = {}
        for (s, o) in self.entries:
            if (s, o) in self.spec:
                continue
            lo, hi = rng.get(s, (0xffff, 0))
            rng[s] = (min(lo, o), max(hi, o))
        for (seg, off), i in list(self.ins.items()):
            if (seg, off) in self.spec:
                continue
            if i.mnemonic in ('lea', 'mov') and len(i.operands) == 2:
                src = i.operands[1]
                v = None
                if i.mnemonic == 'lea' and src.mem.base == 0 and src.mem.index == 0:
                    v = src.mem.disp & 0xffff
                elif i.mnemonic == 'mov' and src.type == X86_OP_IMM and i.operands[0].size == 2:
                    v = src.imm & 0xffff
                if v is not None and seg in rng and rng[seg][0] <= v <= rng[seg][1] + 0x100:
                    out.add((seg, v))
        return out

    def far_pointer_seeds(self):
        for rnd in range(3):
            covered = set()
            for (sg, off), i in self.ins.items():
                a = self.lin(sg, off)
                covered.update(range(a, a + i.size))
            codesegs = {s for (s, o) in self.entries if (s, o) not in self.spec}
            hi = {}
            for (s, o) in self.entries:
                if (s, o) not in self.spec:
                    hi[s] = max(hi.get(s, 0), o)
            added = 0
            for pos in sorted(self.m['relpos']):
                if pos in covered or pos < 2 or pos + 2 > self.n:
                    continue
                seg = struct.unpack('<H', self.img[pos:pos+2])[0]
                off = struct.unpack('<H', self.img[pos-2:pos])[0]
                if seg not in codesegs or off > hi[seg] + 0x100 or (seg, off) in self.entries:
                    continue
                if self.plausible(seg, off):
                    added += len(self.walk((seg, off), False))
            if not added:
                break

    def after_terminator_seeds(self):
        for rnd in range(40):
            covered = set()
            for (sg, off), i in self.ins.items():
                for b in range(i.size):
                    covered.add((sg, (off + b) & 0xffff))
            hi = {}
            for (sg, o) in self.entries:
                if (sg, o) not in self.spec:
                    hi[sg] = max(hi.get(sg, 0), o)
            cands = set()
            for (sg, off), i in list(self.ins.items()):
                if i.mnemonic not in ('ret', 'retf', 'iret', 'jmp', 'ljmp'):
                    continue
                n = (off + i.size) & 0xffff
                # skip alignment padding
                for _ in range(16):
                    a = self.lin(sg, n)
                    if a >= self.n or self.img[a] not in (0x90, 0x00) or (sg, n) in covered:
                        break
                    n = (n + 1) & 0xffff
                if (sg, n) in covered or (sg, n) in self.entries or n > hi.get(sg, 0) + 0x40:
                    continue
                cands.add((sg, n))
            added = 0
            for c in sorted(cands):
                if self.plausible(*c):
                    added += len(self.walk(c, True))
            if not added:
                break

    def plausible(self, seg, off):
        o = off
        for _ in range(6):
            i = self.decode(seg, o)
            if i is None or i.mnemonic in ('arpl', 'bound', 'insb', 'insw', 'outsb', 'outsw', 'outsd', 'hlt') \
                    or i.mnemonic.startswith('f'):
                return False
            if i.mnemonic in TERMINATORS:
                return True
            o = (o + i.size) & 0xffff
        return True

    def discover(self):
        entry = (self.m['cs'], self.m['ip'])
        self.walk(entry, False)
        for e in self.m.get('entries', []):
            self.walk((0, e), False)
        for s in self.seeds:
            self.walk(s, False)
        # far pointers stored in data: their segment word is relocated, the offset sits just before it
        self.far_pointer_seeds()
        # functions reached only through pointers usually start right after another function's ret/jmp
        self.after_terminator_seeds()
        # instruction-start set of confirmed code, to reject misaligned seeds
        for rnd in range(4):
            starts = set(self.ins)
            covered = set()
            for (seg, off), i in self.ins.items():
                for b in range(1, i.size):
                    covered.add((seg, (off + b) & 0xffff))
            added = 0
            for (seg, v) in sorted(self.immediate_seeds()):
                if (seg, v) in self.entries or (seg, v) in covered:
                    continue
                if seg != self.m['cs'] and (seg, v) not in starts:
                    pass
                if self.plausible(seg, v):
                    added += len(self.walk((seg, v), True))
            if not added:
                break

    # ------------------------------------------------------------------------
    def blocks(self):
        """Yield (seg, off, [insns]) for every entry."""
        for (seg, off) in sorted(self.entries):
            out = []
            o = off
            for _ in range(4000):
                i = self.decode(seg, o)
                if i is None:
                    out.append(None); break
                out.append(i)
                mn = i.mnemonic
                if is_branch(mn) or mn in TERMINATORS or mn == 'int':
                    break
                o = (o + i.size) & 0xffff
            yield seg, off, out


# ----------------------------------------------------------------------------
# C emission
# ----------------------------------------------------------------------------
class Unsupported(Exception):
    pass

SEGNAMES = {'cs': 'CS', 'ds': 'DS', 'es': 'ES', 'ss': 'SS', 'fs': 'FS', 'gs': 'GS'}
SZ = {1: '8', 2: '16', 4: '32'}
CT = {1: 'uint8_t', 2: 'uint16_t', 4: 'uint32_t'}
MASK = {1: 0xff, 2: 0xffff, 4: 0xffffffff}

class Emitter:
    def __init__(self, prog, prefix):
        self.p = prog
        self.prefix = prefix
        self.lines = []
        self.cur_ext = set()

    def regname(self, i, r):
        return i.reg_name(r).upper()

    # ---- operand helpers ---------------------------------------------------
    def immv(self, i, val, size):
        """C text for an instruction's immediate operand of `size` bytes."""
        if self.p.raw and i.imm_size:
            addr = (i.address + i.imm_offset) & 0xffff
            enc = i.imm_size
            r = 'RD%d(CS, 0x%x)' % (enc * 8, addr)
            if enc < size:
                r = '(%s)(int%d_t)(int%d_t)%s' % (CT[size], size * 8, enc * 8, r)
            return r
        return '0x%x' % (val & MASK[size])

    def reloc_imm(self, i, val, size):
        """Return C text for an immediate, symbolic if it is a relocated segment."""
        if self.p.raw:
            return self.immv(i, val, size)
        a = self.p.lin(self.cur_seg, i.address)
        # immediate bytes are at the end of the instruction for mov r,imm / push imm
        for k in range(i.size - 1):
            if a + k in self.p.m['relpos']:
                if size == 2:
                    return 'RSEG(0x%x)' % (val & 0xffff)
        return '0x%x' % (val & MASK[size])

    def ea(self, i, op):
        mm = op.mem
        parts = []
        if mm.base:
            parts.append(self.regname(i, mm.base))
        if mm.index:
            idx = self.regname(i, mm.index)
            if mm.scale != 1:
                idx = '%s*%d' % (idx, mm.scale)
            parts.append(idx)
        if mm.disp or not parts:
            parts.append('0x%x' % (mm.disp & 0xffffffff))
        addr32 = any(len(x) == 3 and x.startswith('E') for x in parts)
        expr = '+'.join(parts)
        if addr32:
            ea = '(uint16_t)(%s)' % expr  # real mode: 32-bit EA must stay < 64K
        else:
            ea = '(uint16_t)(%s)' % expr
        if mm.segment:
            seg = SEGNAMES[i.reg_name(mm.segment)]
        elif mm.base and i.reg_name(mm.base) in ('bp', 'ebp', 'sp', 'esp'):
            seg = 'SS'
        else:
            seg = 'DS'
        return seg, ea

    def rd(self, i, op, pre):
        """Return C expression reading operand; may append setup code to pre."""
        if op.type == X86_OP_REG:
            return self.regname(i, op.reg)
        if op.type == X86_OP_IMM:
            return self.reloc_imm(i, op.imm, op.size)
        if op.type == X86_OP_MEM:
            seg, ea = self.ea(i, op)
            pre.append('const uint16_t sg = %s; const uint16_t ea = %s;' % (seg, ea))
            return 'RD%s(sg, ea)' % SZ[op.size]
        raise Unsupported('operand')

    def wr(self, i, op, val, pre):
        if op.type == X86_OP_REG:
            return '%s = (%s)(%s);' % (self.regname(i, op.reg), CT[op.size], val)
        if op.type == X86_OP_MEM:
            if not any(x.startswith('const uint16_t sg') for x in pre):
                seg, ea = self.ea(i, op)
                pre.append('const uint16_t sg = %s; const uint16_t ea = %s;' % (seg, ea))
            return 'WR%s(sg, ea, %s);' % (SZ[op.size], val)
        raise Unsupported('write operand')

    # ---- per-instruction translation --------------------------------------
    def insn(self, i, last, nxt):
        mn = i.mnemonic
        ops = i.operands
        out = []
        pre = []
        seg_over = None
        for p in i.prefix:
            if p in (0x26, 0x2e, 0x36, 0x3e, 0x64, 0x65):
                seg_over = {0x26: 'ES', 0x2e: 'CS', 0x36: 'SS', 0x3e: 'DS', 0x64: 'FS', 0x65: 'GS'}[p]
        def emit(s):
            out.append(s)
        def jumpto(target):
            emit('IP = 0x%x; return;' % (target & 0xffff))

        ALU = {'add': 'ADD', 'adc': 'ADC', 'sub': 'SUB', 'sbb': 'SBB', 'and': 'AND',
               'or': 'OR', 'xor': 'XOR'}
        CC = {'je': 'ZF', 'jne': '!ZF', 'jb': 'CF', 'jae': '!CF', 'jbe': '(CF||ZF)',
              'ja': '(!CF&&!ZF)', 'jl': '(SF!=OF)', 'jge': '(SF==OF)', 'jle': '(ZF||SF!=OF)',
              'jg': '(!ZF&&SF==OF)', 'js': 'SF', 'jns': '!SF', 'jo': 'OF', 'jno': '!OF',
              'jp': 'PF', 'jnp': '!PF'}

        if mn == 'nop':
            pass
        elif mn == 'mov':
            d, s = ops
            if d.type == X86_OP_REG and self.regname(i, d.reg) in ('CS',):
                raise Unsupported('mov cs')
            if s.type == X86_OP_MEM:
                if d.type != X86_OP_REG:
                    raise Unsupported('mem->mem')
                seg, ea = self.ea(i, s)
                emit('%s = RD%s(%s, %s);' % (self.regname(i, d.reg), SZ[d.size], seg, ea))
            else:
                if s.type == X86_OP_IMM:
                    v = self.reloc_imm(i, s.imm, d.size)
                else:
                    v = self.regname(i, s.reg)
                if d.type == X86_OP_MEM:
                    seg, ea = self.ea(i, d)
                    emit('WR%s(%s, %s, %s);' % (SZ[d.size], seg, ea, v))
                else:
                    emit('%s = (%s)(%s);' % (self.regname(i, d.reg), CT[d.size], v))
        elif mn in ALU:
            d, s = ops
            sv = self.rd(i, s, []) if s.type != X86_OP_MEM else None
            if s.type == X86_OP_IMM:
                sv = self.immv(i, s.imm, d.size)
            if s.type == X86_OP_MEM:
                seg, ea = self.ea(i, s)
                sv = 'RD%s(%s, %s)' % (SZ[s.size], seg, ea)
                emit('{ %s t = %s(%s, %s); %s }' % (CT[d.size], ALU[mn] + SZ[d.size], self.regname(i, d.reg), sv,
                                                     self.wr(i, d, 't', [])))
            elif d.type == X86_OP_MEM:
                seg, ea = self.ea(i, d)
                emit('{ const uint16_t sg = %s, ea = %s; WR%s(sg, ea, %s%s(RD%s(sg, ea), %s)); }' % (
                    seg, ea, SZ[d.size], ALU[mn], SZ[d.size], SZ[d.size], sv))
            else:
                r = self.regname(i, d.reg)
                emit('%s = %s%s(%s, %s);' % (r, ALU[mn], SZ[d.size], r, sv))
        elif mn in ('cmp', 'test'):
            d, s = ops
            fn = 'SUB' if mn == 'cmp' else 'AND'
            pre1 = []
            a = self.rd(i, d, pre1)
            if s.type == X86_OP_MEM:
                seg, ea = self.ea(i, s)
                b = 'RD%s(%s, %s)' % (SZ[s.size], seg, ea)
            elif s.type == X86_OP_IMM:
                b = self.immv(i, s.imm, d.size)
            else:
                b = self.regname(i, s.reg)
            emit('{ %s (void)%s%s(%s, %s); }' % (' '.join(pre1), fn, SZ[d.size], a, b))
        elif mn in ('inc', 'dec', 'neg', 'not'):
            d = ops[0]
            fn = {'inc': 'INC', 'dec': 'DEC', 'neg': 'NEG', 'not': 'NOT'}[mn] + SZ[d.size]
            if d.type == X86_OP_MEM:
                seg, ea = self.ea(i, d)
                emit('{ const uint16_t sg = %s, ea = %s; WR%s(sg, ea, %s(RD%s(sg, ea))); }' % (seg, ea, SZ[d.size], fn, SZ[d.size]))
            else:
                r = self.regname(i, d.reg)
                emit('%s = %s(%s);' % (r, fn, r))
        elif mn in ('shl', 'sal', 'shr', 'sar', 'rol', 'ror', 'rcl', 'rcr'):
            d = ops[0]
            cnt = '1' if len(ops) == 1 else (self.regname(i, ops[1].reg) if ops[1].type == X86_OP_REG else self.immv(i, ops[1].imm, 1))
            fn = {'sal': 'SHL'}.get(mn, mn.upper()) + SZ[d.size]
            if d.type == X86_OP_MEM:
                seg, ea = self.ea(i, d)
                emit('{ const uint16_t sg = %s, ea = %s; WR%s(sg, ea, %s(RD%s(sg, ea), %s)); }' % (seg, ea, SZ[d.size], fn, SZ[d.size], cnt))
            else:
                r = self.regname(i, d.reg)
                emit('%s = %s(%s, %s);' % (r, fn, r, cnt))
        elif mn in ('mul', 'div', 'idiv') or (mn == 'imul' and len(ops) == 1):
            s = ops[0]
            pre1 = []
            v = self.rd(i, s, pre1)
            emit('{ %s %s%s(%s); }' % (' '.join(pre1), mn.upper(), SZ[s.size], v))
        elif mn == 'imul':
            d = ops[0]
            if len(ops) == 2:
                a, b = ops[0], ops[1]
            else:
                a, b = ops[1], ops[2]
            pre1 = []
            av = self.rd(i, a, pre1)
            bv = self.immv(i, b.imm, d.size) if b.type == X86_OP_IMM else self.rd(i, b, pre1)
            emit('{ %s %s = IMUL%sx(%s, %s); }' % (' '.join(pre1), self.regname(i, d.reg), SZ[d.size], av, bv))
        elif mn == 'lea':
            d, s = ops
            seg, ea = self.ea(i, s)
            emit('%s = %s;' % (self.regname(i, d.reg), ea))
        elif mn in ('lds', 'les'):
            d, s = ops
            seg, ea = self.ea(i, s)
            sr = 'DS' if mn == 'lds' else 'ES'
            emit('{ const uint16_t sg = %s, ea = %s; uint16_t o = RD16(sg, ea); %s = RD16(sg, (uint16_t)(ea+2)); %s = o; }' % (
                seg, ea, sr, self.regname(i, d.reg)))
        elif mn == 'xchg':
            a, b = ops
            if a.type == X86_OP_REG and b.type == X86_OP_REG:
                ra, rb = self.regname(i, a.reg), self.regname(i, b.reg)
                emit('{ %s t = %s; %s = %s; %s = t; }' % (CT[a.size], ra, ra, rb, rb))
            else:
                m_, r_ = (a, b) if a.type == X86_OP_MEM else (b, a)
                seg, ea = self.ea(i, m_)
                rr = self.regname(i, r_.reg)
                emit('{ const uint16_t sg = %s, ea = %s; %s t = RD%s(sg, ea); WR%s(sg, ea, %s); %s = t; }' % (
                    seg, ea, CT[m_.size], SZ[m_.size], SZ[m_.size], rr, rr))
        elif mn == 'push':
            s = ops[0]
            pre1 = []
            if s.type == X86_OP_IMM:
                v = self.reloc_imm(i, s.imm, 2)
                emit('PUSH%s(%s);' % (SZ[max(2, s.size)] if s.size != 1 else '16', v))
            else:
                v = self.rd(i, s, pre1)
                emit('{ %s PUSH%s(%s); }' % (' '.join(pre1), SZ[s.size], v))
        elif mn == 'pop':
            d = ops[0]
            if d.type == X86_OP_REG:
                r = self.regname(i, d.reg)
                if r == 'CS':
                    raise Unsupported('pop cs')
                emit('%s = POP%s();' % (r, SZ[d.size]))
            else:
                seg, ea = self.ea(i, d)
                emit('{ %s t = POP%s(); WR%s(%s, %s, t); }' % (CT[d.size], SZ[d.size], SZ[d.size], seg, ea))
        elif mn in ('pushaw', 'pusha') or (mn in ('pushal', 'pushad') and 0x66 not in i.prefix):
            emit('PUSHA();')
        elif mn in ('popaw', 'popa') or (mn in ('popal', 'popad') and 0x66 not in i.prefix):
            emit('POPA();')
        elif mn in ('pushf', 'pushfw'):
            emit('PUSH16(GETFLAGS());')
        elif mn in ('popf', 'popfw'):
            emit('SETFLAGS(POP16());')
        # Capstone names these inconsistently in 16-bit mode (it prints plain 99h as 'cdq'); the operand-size
        # prefix 66h is what decides the width.
        elif mn in ('cbw', 'cwde'):
            if 0x66 in i.prefix:
                emit('EAX = (uint32_t)(int32_t)(int16_t)AX;')
            else:
                emit('AX = (uint16_t)(int16_t)(int8_t)AL;')
        elif mn in ('cwd', 'cdq'):
            if 0x66 in i.prefix:
                emit('EDX = (EAX & 0x80000000u) ? 0xffffffffu : 0;')
            else:
                emit('DX = (AX & 0x8000) ? 0xffff : 0;')
        elif mn == 'lahf':
            emit('AH = (uint8_t)GETFLAGS();')
        elif mn == 'sahf':
            emit('SETFLAGS((GETFLAGS() & 0xff00) | AH);')
        elif mn in ('clc', 'stc', 'cmc', 'cld', 'std', 'cli', 'sti'):
            emit({'clc': 'CF = 0;', 'stc': 'CF = 1;', 'cmc': 'CF = !CF;', 'cld': 'DF = 0;',
                  'std': 'DF = 1;', 'cli': 'IFL = 0;', 'sti': 'IFL = 1; rt_sti();'}[mn])
        elif mn == 'xlatb':
            seg = seg_over or 'DS'
            emit('AL = RD8(%s, (uint16_t)(BX + AL));' % seg)
        elif mn in ('in', 'out'):
            if mn == 'in':
                d, s = ops
                port = 'DX' if s.type == X86_OP_REG else self.immv(i, s.imm, 1)
                emit('%s = IN%s(%s);' % (self.regname(i, d.reg), SZ[d.size], port))
            else:
                d, s = ops
                port = 'DX' if d.type == X86_OP_REG else self.immv(i, d.imm, 1)
                emit('OUT%s(%s, %s);' % (SZ[s.size], port, self.regname(i, s.reg)))
        elif re.match(r'^(rep |repe |repne )?(movs|stos|lods|cmps|scas)[bwd]$', mn):
            rep = ''
            if mn.startswith('rep'):
                rep, base = mn.split(' ')
            else:
                base = mn
            kind = base[:4].upper()
            size = {'b': '8', 'w': '16', 'd': '32'}[base[-1]]
            src = seg_over or 'DS'
            repk = {'': 'NONE', 'rep': 'REP', 'repe': 'REPE', 'repne': 'REPNE'}[rep]
            if kind in ('CMPS', 'SCAS') and repk == 'REP':
                repk = 'REPE'
            emit('STR_%s%s(%s, %s);' % (kind, size, src, repk))
        # ---- control flow ----
        elif mn in CC:
            t = ops[0].imm & 0xffff
            emit('if (%s) { IP = 0x%x; return; }' % (CC[mn], t))
            jumpto(nxt)
        elif mn == 'jcxz':
            emit('if (CX == 0) { IP = 0x%x; return; }' % (ops[0].imm & 0xffff)); jumpto(nxt)
        elif mn in ('loop', 'loope', 'loopne'):
            t = ops[0].imm & 0xffff
            cond = {'loop': 'CX', 'loope': 'CX && ZF', 'loopne': 'CX && !ZF'}[mn]
            emit('CX = (uint16_t)(CX - 1); if (%s) { IP = 0x%x; return; }' % (cond, t)); jumpto(nxt)
        elif mn == 'jmp':
            o = ops[0]
            if o.type == X86_OP_IMM:
                jumpto(o.imm)
            else:
                pre1 = []
                v = self.rd(i, o, pre1)
                emit('{ %s IP = %s; return; }' % (' '.join(pre1), v))
        elif mn == 'call':
            o = ops[0]
            if o.type == X86_OP_IMM:
                emit('PUSH16(0x%x); IP = 0x%x; return;' % (nxt, o.imm & 0xffff))
            else:
                pre1 = []
                v = self.rd(i, o, pre1)
                emit('{ %s uint16_t t = %s; PUSH16(0x%x); IP = t; return; }' % (' '.join(pre1), v, nxt))
        elif mn in ('lcall', 'ljmp'):
            if len(ops) == 2:
                seg_txt = self.reloc_imm(i, ops[0].imm, 2)
                off = ops[1].imm & 0xffff
                if mn == 'lcall':
                    emit('PUSH16(CS); PUSH16(0x%x); CS = %s; IP = 0x%x; return;' % (nxt, seg_txt, off))
                else:
                    emit('CS = %s; IP = 0x%x; return;' % (seg_txt, off))
            else:
                seg, ea = self.ea(i, ops[0])
                if mn == 'lcall':
                    emit('{ const uint16_t sg = %s, ea = %s; uint16_t o = RD16(sg, ea), s = RD16(sg, (uint16_t)(ea+2)); '
                         'PUSH16(CS); PUSH16(0x%x); CS = s; IP = o; return; }' % (seg, ea, nxt))
                else:
                    emit('{ const uint16_t sg = %s, ea = %s; uint16_t o = RD16(sg, ea), s = RD16(sg, (uint16_t)(ea+2)); '
                         'CS = s; IP = o; return; }' % (seg, ea))
        elif mn == 'ret':
            n = ops[0].imm if ops else 0
            emit('IP = POP16(); %sreturn;' % ('SP = (uint16_t)(SP + %d); ' % n if n else ''))
        elif mn == 'retf':
            n = ops[0].imm if ops else 0
            emit('IP = POP16(); CS = POP16(); %sreturn;' % ('SP = (uint16_t)(SP + %d); ' % n if n else ''))
        elif mn == 'iret':
            emit('IP = POP16(); CS = POP16(); SETFLAGS(POP16()); return;')
        elif mn == 'int':
            emit('rt_int(0x%x, 0x%x); return;' % (ops[0].imm & 0xff, nxt))
        elif mn == 'hlt':
            emit('rt_hlt(); IP = 0x%x; return;' % nxt)
        elif mn == 'enter':
            emit('ENTER(0x%x, %d);' % (ops[0].imm & 0xffff, ops[1].imm & 0x1f))
        elif mn in ('pushal', 'pushad'):
            emit('{ uint32_t sp0 = ESP & 0xffff; PUSH32(EAX); PUSH32(ECX); PUSH32(EDX); PUSH32(EBX); PUSH32(sp0); '
                 'PUSH32(EBP); PUSH32(ESI); PUSH32(EDI); }')
        elif mn in ('popal', 'popad'):
            emit('{ EDI = POP32(); ESI = POP32(); EBP = POP32(); (void)POP32(); EBX = POP32(); EDX = POP32(); '
                 'ECX = POP32(); EAX = POP32(); }')
        elif mn in ('movzx', 'movsx'):
            d, s_ = ops
            pre1 = []
            v = self.rd(i, s_, pre1)
            if mn == 'movzx':
                val = '(%s)(%s)' % (CT[d.size], v)
            else:
                st = {1: 'int8_t', 2: 'int16_t'}[s_.size]
                val = '(%s)(%s)(%s)' % (CT[d.size], st.replace('int', 'int') if False else {2: 'int16_t', 4: 'int32_t'}[d.size], '(%s)(%s)' % (st, v))
            emit('{ %s %s = %s; }' % (' '.join(pre1), self.regname(i, d.reg), val))
        elif mn in ('lfs', 'lgs', 'lss'):
            d, s_ = ops
            seg, ea = self.ea(i, s_)
            sr = {'lfs': 'FS', 'lgs': 'GS', 'lss': 'SS'}[mn]
            emit('{ const uint16_t sg = %s, ea = %s; uint16_t o = RD16(sg, ea); %s = RD16(sg, (uint16_t)(ea+2)); %s = o; }' % (
                seg, ea, sr, self.regname(i, d.reg)))
        elif mn == 'leave':
            emit('SP = BP; BP = POP16();')
        else:
            raise Unsupported(mn)
        return out

    def block(self, seg, off, insns):
        self.cur_seg = seg
        spec = (seg, off) in self.p.spec
        name = 'b_%04x_%04x' % (seg, off)
        body = []
        ok = True
        for k, i in enumerate(insns):
            if i is None:
                body.append('rt_bad(CS, 0x%x); return;' % ((insns[k-1].address + insns[k-1].size) & 0xffff if k else off))
                break
            nxt = (i.address + i.size) & 0xffff
            last = k == len(insns) - 1
            try:
                code = self.insn(i, last, nxt)
            except Unsupported as e:
                if not spec:
                    sys.stderr.write('warning: %s: untranslatable instruction at %04x:%04x (%s %s); trapping\n' % (
                        self.prefix, seg, i.address, i.mnemonic, i.op_str))
                body.append('rt_bad(CS, 0x%x); return; /* unsupported in speculative block: %s */' % (i.address, i.mnemonic))
                ok = False
                break
            body.append('/* %04x  %-28s */ %s' % (i.address, (i.mnemonic + ' ' + i.op_str)[:28].replace('*/', '* /'), ' '.join(code)))
            if last and not any('return;' in c for c in code):
                body.append('IP = 0x%x; return;' % nxt)
        return name, body

    def emit(self, out):
        p = self.p
        m = p.m
        P = self.prefix
        L = []
        L.append('/* GENERATED by epic/tools/eprecomp.py from the user\'s own game file. Do not edit, do not commit. */')
        L.append('#include "epic_rt.h"')
        L.append('uint16_t %s_load_seg;' % P)
        L.append('#define RSEG(x) ((uint16_t)(%s_load_seg + (x)))' % P)
        L.append('')
        names = {}
        nbad = 0
        segs = sorted({s for s, o in p.entries})
        blocks = list(p.blocks())
        for seg, off, insns in blocks:
            name, body = self.block(seg, off, insns)
            names[(seg, off)] = name
            L.append('static void %s(void) {' % name)
            for b in body:
                L.append('    ' + b)
            L.append('}')
        L.append('')
        # dispatch per segment
        for s in segs:
            L.append('static rt_block_fn %s_seg_%04x(uint16_t ip) {' % (P, s))
            L.append('    switch (ip) {')
            for (seg, off), name in sorted(names.items()):
                if seg == s:
                    L.append('    case 0x%04x: return %s;' % (off, name))
            L.append('    default: return 0;')
            L.append('    }')
            L.append('}')
        L.append('static rt_block_fn %s_lookup(uint16_t relseg, uint16_t ip) {' % P)
        L.append('    switch (relseg) {')
        for s in segs:
            L.append('    case 0x%04x: return %s_seg_%04x(ip);' % (s, P, s))
        L.append('    default: return 0;')
        L.append('    }')
        L.append('}')
        ext = sorted(p.extsegs)
        L.append('static const uint16_t %s_extsegs[] = { %s 0xffff };' % (P, ''.join('0x%04x, ' % e for e in ext)))
        L.append('const rt_program %s_program = {' % P)
        L.append('    "%s", 0x%08xu, %d, %s_lookup, %s_extsegs, &%s_load_seg,' % (P, m['crc'], len(m['img']), P, P, P))
        L.append('    0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, %d' % (m['cs'], m['ip'], m['ss'], m['sp'], m['minalloc'], m['maxalloc'], 1 if p.raw else 0))
        L.append('};')
        open(out, 'w').write('\n'.join(L) + '\n')
        return len(blocks)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe'); ap.add_argument('prefix'); ap.add_argument('out')
    ap.add_argument('--seeds', help='file with extra entry points: SEG:OFF per line (image-relative)')
    ap.add_argument('--report', action='store_true')
    a = ap.parse_args()
    seeds = []
    if a.seeds:
        try:
            for line in open(a.seeds):
                line = line.split('#')[0].strip()
                if line:
                    s, o = line.split(':'); seeds.append((int(s, 16), int(o, 16)))
        except FileNotFoundError:
            pass
    p = Program(a.exe, seeds)
    e = Emitter(p, a.prefix)
    n = e.emit(a.out)
    conf = len(p.entries) - len(p.spec)
    print('%s: %d blocks (%d confirmed, %d speculative), %d insns, jtables=%s, ext=%s' % (
        a.prefix, n, conf, len(p.spec), len(p.ins), [(hex(s), hex(o), hex(t) if t else None, c) for s, o, t, c in p.jtables],
        [hex(x) for x in p.extsegs]))

if __name__ == '__main__':
    main()
