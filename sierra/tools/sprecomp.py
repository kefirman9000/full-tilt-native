#!/usr/bin/env python3
"""
sprecomp.py - static recompiler for 32-bit Windows (Win32s / Win32) game executables -> C99.

Built for Sierra's 3-D Ultra Pinball (PINBALL.EXE, a Borland C++ Win32s program). Reads the PE
from the user's own game folder and writes one C file in which every reachable basic block of x86
code becomes a C function over the runtime CPU state (sierra/runtime/sierra_rt.h).

Only machine code is translated. Game data (graphics, strings, tables) is NOT copied into the
output: the runtime loads the original EXE image from the user's game folder at run time, at its
preferred base address, exactly where Windows would put it.

Code discovery: recursive descent from the entry point and the exports, plus every relocated
dword that points into a code section (function pointers, vtables, switch tables: the PE .reloc
table lists all of them), plus optional seeds (addresses found by running the game).

usage: sprecomp.py GAME.EXE PREFIX OUT.c [--seeds seeds.txt] [--parts N]
"""
import sys, struct, argparse, zlib
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86 import *

# ----------------------------------------------------------------------------
# PE loading
# ----------------------------------------------------------------------------
class PE:
    def __init__(self, path):
        d = open(path, 'rb').read()
        assert d[:2] == b'MZ', 'not an executable'
        pe = struct.unpack_from('<I', d, 0x3c)[0]
        assert d[pe:pe+4] == b'PE\0\0', 'not a 32-bit Windows (PE) executable'
        machine, nsec = struct.unpack_from('<HH', d, pe + 4)
        assert machine == 0x14c, 'not an x86 executable'
        optsz = struct.unpack_from('<H', d, pe + 20)[0]
        opt = pe + 24
        self.entry = struct.unpack_from('<I', d, opt + 16)[0]
        self.base = struct.unpack_from('<I', d, opt + 28)[0]
        self.size = struct.unpack_from('<I', d, opt + 56)[0]
        ndirs = struct.unpack_from('<I', d, opt + 92)[0]
        self.dirs = [struct.unpack_from('<II', d, opt + 96 + 8*i) for i in range(min(ndirs, 16))]
        self.img = bytearray(self.size)
        self.secs = []
        so = opt + optsz
        for i in range(nsec):
            name, vs, va, rs, rp, _, _, _, _, ch = struct.unpack_from('<8sIIIIIIHHI', d, so + 40*i)
            n = min(rs, vs if vs else rs)
            self.img[va:va+n] = d[rp:rp+n]
            self.secs.append((name.rstrip(b'\0').decode('latin1'), va, max(vs, rs), ch))
        self.code = [(self.base + va, self.base + va + sz) for (nm, va, sz, ch) in self.secs if ch & 0x20]
        self.crc = 0
        for (nm, va, sz, ch) in self.secs:
            if ch & 0x20:
                self.crc = zlib.crc32(bytes(self.img[va:va+sz]), self.crc)
        self.crc &= 0xffffffff
        self.relocs = self._relocs()
        self.exports = self._exports()

    def _relocs(self):
        out = []
        rva, size = self.dirs[5] if len(self.dirs) > 5 else (0, 0)
        p, end = rva, rva + size
        while p < end:
            page, bsz = struct.unpack_from('<II', self.img, p)
            if bsz < 8:
                break
            for k in range((bsz - 8) // 2):
                e = struct.unpack_from('<H', self.img, p + 8 + 2*k)[0]
                if e >> 12 == 3:
                    out.append(page + (e & 0xfff))
            p += bsz
        return out

    def _exports(self):
        rva, size = self.dirs[0]
        if not rva:
            return []
        nfun, nnames, afun = struct.unpack_from('<III', self.img, rva + 20)
        return [self.base + struct.unpack_from('<I', self.img, afun + 4*i)[0] for i in range(nfun)
                if struct.unpack_from('<I', self.img, afun + 4*i)[0]]

    def in_code(self, a):
        return any(lo <= a < hi for lo, hi in self.code)

    def rd32(self, a):
        o = a - self.base
        if 0 <= o <= self.size - 4:
            return struct.unpack_from('<I', self.img, o)[0]
        return None

md = Cs(CS_ARCH_X86, CS_MODE_32)
md.detail = True

TERMINATORS = {'jmp', 'ljmp', 'ret', 'retf', 'iretd', 'iret', 'hlt', 'int3', 'ud2'}
def is_branch(mn):
    return mn in ('jmp', 'call', 'jcxz', 'jecxz') or (mn.startswith('j')) or mn.startswith('loop')

# ----------------------------------------------------------------------------
# discovery
# ----------------------------------------------------------------------------
class Program:
    def __init__(self, pe, seeds=()):
        self.pe = pe
        self.ins = {}
        self.entries = set()
        self.spec = set()
        self.discover(seeds)

    def decode(self, a):
        if a in self.ins:
            return self.ins[a]
        if not self.pe.in_code(a):
            return None
        o = a - self.pe.base
        i = next(md.disasm(bytes(self.pe.img[o:o+16]), a), None)
        if i is not None:
            self.ins[a] = i
        return i

    def walk(self, start, speculative):
        work = [start]
        added = 0
        while work:
            a = work.pop()
            if not self.pe.in_code(a):
                continue
            if a in self.entries:
                if not speculative and a in self.spec:
                    self.spec.discard(a)
                else:
                    continue
            self.entries.add(a)
            added += 1
            if speculative:
                self.spec.add(a)
            o = a
            for _ in range(20000):
                i = self.decode(o)
                if i is None:
                    break
                mn = i.mnemonic
                nxt = o + i.size
                if is_branch(mn):
                    ops = i.operands
                    if ops and ops[0].type == X86_OP_IMM:
                        work.append(ops[0].imm & 0xffffffff)
                    if mn == 'jmp':
                        break
                    work.append(nxt)
                    break
                if mn in TERMINATORS:
                    break
                o = nxt
        return added

    def plausible(self, a):
        o = a
        for _ in range(8):
            i = self.decode(o)
            if i is None or i.mnemonic in ('arpl', 'bound', 'insb', 'insd', 'outsb', 'outsd', 'hlt', 'in', 'out',
                                           'les', 'lds', 'into', 'aaa', 'aas', 'daa', 'das', 'sahf', 'retf',
                                           'iretd', 'salc', 'lock', 'ljmp', 'lcall'):
                return False
            if i.mnemonic in TERMINATORS or is_branch(i.mnemonic):
                return True
            o += i.size
        return True

    def discover(self, seeds):
        pe = self.pe
        self.walk(pe.base + pe.entry, False)
        for e in pe.exports:
            self.walk(e, False)
        for s in seeds:
            self.walk(s, False)
        # relocated pointers into code: function pointers, vtables, switch tables
        for r in pe.relocs:
            v = pe.rd32(pe.base + r)
            if v is None or not pe.in_code(v) or v in self.entries:
                continue
            if pe.in_code(pe.base + r):
                # the pointer itself lives in code: an absolute jump/call operand or a switch table
                pass
            if self.plausible(v):
                self.walk(v, True)

    def blocks(self):
        for a in sorted(self.entries):
            out = []
            o = a
            for _ in range(20000):
                i = self.decode(o)
                if i is None:
                    out.append(None); break
                out.append(i)
                mn = i.mnemonic
                if is_branch(mn) or mn in TERMINATORS:
                    break
                o += i.size
                if o in self.entries:
                    # another block starts here (a jump target): end this one so code is not duplicated
                    out.append(('fall', o))
                    break
            yield a, out

# ----------------------------------------------------------------------------
# C emission
# ----------------------------------------------------------------------------
class Unsupported(Exception):
    pass

SZ = {1: '8', 2: '16', 4: '32', 8: '64'}
CT = {1: 'uint8_t', 2: 'uint16_t', 4: 'uint32_t'}
ST = {1: 'int8_t', 2: 'int16_t', 4: 'int32_t'}
MASK = {1: 0xff, 2: 0xffff, 4: 0xffffffff}
SEGREG = {'cs': '0x1b', 'ds': '0x23', 'es': '0x23', 'ss': '0x23', 'fs': '0x3b', 'gs': '0'}
CC = {'je': 'ZF', 'jne': '!ZF', 'jb': 'CF', 'jae': '!CF', 'jbe': '(CF||ZF)', 'ja': '(!CF&&!ZF)',
      'jl': '(SF!=OF)', 'jge': '(SF==OF)', 'jle': '(ZF||SF!=OF)', 'jg': '(!ZF&&SF==OF)', 'js': 'SF',
      'jns': '!SF', 'jo': 'OF', 'jno': '!OF', 'jp': 'PF', 'jnp': '!PF'}
ALU = {'add': 'ADD', 'adc': 'ADC', 'sub': 'SUB', 'sbb': 'SBB', 'and': 'AND', 'or': 'OR', 'xor': 'XOR'}

class Emitter:
    def __init__(self, prog, prefix):
        self.p = prog
        self.prefix = prefix

    def reg(self, i, r):
        n = i.reg_name(r)
        if n in SEGREG:
            raise Unsupported('segment register')
        return n.upper()

    def ea(self, i, op):
        mm = op.mem
        parts = []
        if mm.base:
            parts.append(self.reg(i, mm.base))
        if mm.index:
            x = self.reg(i, mm.index)
            parts.append('%s*%d' % (x, mm.scale) if mm.scale != 1 else x)
        if mm.disp or not parts:
            parts.append('0x%xu' % (mm.disp & 0xffffffff))
        e = '(uint32_t)(%s)' % '+'.join(parts)
        if mm.segment and i.reg_name(mm.segment) == 'fs':
            e = '(uint32_t)(R.fsbase+%s)' % e
        elif mm.segment and i.reg_name(mm.segment) == 'gs':
            raise Unsupported('gs')
        return e

    def rd(self, i, op, size=None):
        size = size or op.size
        if op.type == X86_OP_REG:
            n = i.reg_name(op.reg)
            if n in SEGREG:
                return SEGREG[n]
            return n.upper()
        if op.type == X86_OP_IMM:
            return '0x%xu' % (op.imm & MASK[size])
        if op.type == X86_OP_MEM:
            if op.size not in (1, 2, 4):
                raise Unsupported('operand size %d' % op.size)
            return 'RD%s(%s)' % (SZ[op.size], self.ea(i, op))
        raise Unsupported('operand')

    def wr(self, i, op, val):
        if op.type == X86_OP_REG:
            n = i.reg_name(op.reg)
            if n in SEGREG:
                return '(void)(%s);' % val
            return '%s = (%s)(%s);' % (n.upper(), CT[op.size], val)
        if op.type == X86_OP_MEM:
            if op.size not in (1, 2, 4):
                raise Unsupported('operand size %d' % op.size)
            return 'WR%s(%s, %s);' % (SZ[op.size], self.ea(i, op), val)
        raise Unsupported('write operand')

    def rmw(self, i, op, fn):
        """read-modify-write of a register or memory operand; fn(x) -> C expr"""
        if op.type == X86_OP_MEM:
            return '{ const uint32_t a_ = %s; WR%s(a_, %s); }' % (self.ea(i, op), SZ[op.size], fn('RD%s(a_)' % SZ[op.size]))
        r = self.rd(i, op)
        return '%s = %s;' % (r, fn(r))

    # ---- x87, decoded from the opcode bytes (capstone's operand order for these is not trusted) ----
    def x87(self, i):
        b = bytes(i.bytes)
        k = 0
        while k < len(b) and b[k] in (0x9b, 0x66, 0x26, 0x2e, 0x36, 0x3e, 0x64, 0x65, 0xf2, 0xf3):
            k += 1
        op, modrm = b[k], b[k+1]
        mod, reg, rm = modrm >> 6, (modrm >> 3) & 7, modrm & 7
        if mod != 3:
            mem = [o for o in i.operands if o.type == X86_OP_MEM]
            if not mem:
                raise Unsupported('x87 mem')
            a = self.ea(i, mem[0])
            ar = ['FADD', 'FMUL', 'FCOM', 'FCOMP', 'FSUB', 'FSUBR', 'FDIV', 'FDIVR'][reg]
            def arith(src):
                if ar == 'FCOM':  return 'FCOM(ST(0), %s);' % src
                if ar == 'FCOMP': return 'FCOM(ST(0), %s); FPOP();' % src
                e = {'FADD': 'ST(0) + v', 'FMUL': 'ST(0) * v', 'FSUB': 'ST(0) - v', 'FSUBR': 'v - ST(0)',
                     'FDIV': 'ST(0) / v', 'FDIVR': 'v / ST(0)'}[ar]
                return '{ double v = %s; ST(0) = %s; }' % (src, e)
            if op == 0xd8: return arith('(double)RDF32(%s)' % a)
            if op == 0xdc: return arith('RDF64(%s)' % a)
            if op == 0xda: return arith('(double)(int32_t)RD32(%s)' % a)
            if op == 0xde: return arith('(double)(int16_t)RD16(%s)' % a)
            if op == 0xd9:
                return {0: 'FPUSH((double)RDF32(%s));', 2: 'WRF32(%s, ST(0));', 3: 'WRF32(%s, FPOP());',
                        5: 'R.fcw = RD16(%s);', 7: 'WR16(%s, R.fcw);',
                        4: 'rt_trap(EIP, "fldenv"); (void)%s;', 6: '{ uint32_t a_ = %s; memset(GPTR(a_), 0, 28); WR16(a_, R.fcw); WR16(a_ + 4, FNSTSW()); }'}[reg] % a
            if op == 0xdb:
                return {0: 'FPUSH((double)(int32_t)RD32(%s));',
                        1: 'WR32(%s, (uint32_t)(int32_t)ST(0)); FPOP();',
                        2: 'WR32(%s, (uint32_t)(int32_t)rt_fround(ST(0)));',
                        3: 'WR32(%s, (uint32_t)(int32_t)rt_fround(ST(0))); FPOP();',
                        5: 'FPUSH(RDF80(%s));', 7: 'WRF80(%s, FPOP());'}[reg] % a
            if op == 0xdd:
                return {0: 'FPUSH(RDF64(%s));', 1: 'WR64(%s, (uint64_t)(int64_t)ST(0)); FPOP();',
                        2: 'WRF64(%s, ST(0));', 3: 'WRF64(%s, FPOP());', 7: 'WR16(%s, FNSTSW());',
                        4: 'rt_trap(EIP, "frstor"); (void)%s;', 6: 'rt_trap(EIP, "fnsave"); (void)%s;'}[reg] % a
            if op == 0xdf:
                return {0: 'FPUSH((double)(int16_t)RD16(%s));',
                        2: 'WR16(%s, (uint16_t)(int16_t)rt_fround(ST(0)));',
                        3: 'WR16(%s, (uint16_t)(int16_t)rt_fround(ST(0))); FPOP();',
                        5: 'FPUSH((double)(int64_t)RD64(%s));',
                        7: 'WR64(%s, (uint64_t)rt_fround(ST(0))); FPOP();',
                        4: 'rt_trap(EIP, "fbld"); (void)%s;', 6: 'rt_trap(EIP, "fbstp"); (void)%s;'}[reg] % a
            raise Unsupported('x87 %02x/%d' % (op, reg))
        s = rm
        if op == 0xd8:
            return {0: 'ST(0) = ST(0) + ST(%d);', 1: 'ST(0) = ST(0) * ST(%d);', 2: 'FCOM(ST(0), ST(%d));',
                    3: 'FCOM(ST(0), ST(%d)); FPOP();', 4: 'ST(0) = ST(0) - ST(%d);', 5: 'ST(0) = ST(%d) - ST(0);',
                    6: 'ST(0) = ST(0) / ST(%d);', 7: 'ST(0) = ST(%d) / ST(0);'}[reg] % s
        if op == 0xdc:
            return {0: 'ST(%d) = ST(%d) + ST(0);', 1: 'ST(%d) = ST(%d) * ST(0);',
                    4: 'ST(%d) = ST(0) - ST(%d);', 5: 'ST(%d) = ST(%d) - ST(0);',
                    6: 'ST(%d) = ST(0) / ST(%d);', 7: 'ST(%d) = ST(%d) / ST(0);'}.get(reg, 'FCOM(ST(0), ST(%d));%.0d') % (s, s)
        if op == 0xde:
            if modrm == 0xd9:
                return 'FCOM(ST(0), ST(1)); FPOP(); FPOP();'
            return {0: 'ST(%d) = ST(%d) + ST(0); FPOP();', 1: 'ST(%d) = ST(%d) * ST(0); FPOP();',
                    4: 'ST(%d) = ST(0) - ST(%d); FPOP();', 5: 'ST(%d) = ST(%d) - ST(0); FPOP();',
                    6: 'ST(%d) = ST(0) / ST(%d); FPOP();', 7: 'ST(%d) = ST(%d) / ST(0); FPOP();'}[reg] % (s, s)
        if op == 0xd9:
            if reg == 0: return '{ double v = ST(%d); FPUSH(v); }' % s
            if reg == 1: return '{ double v = ST(0); ST(0) = ST(%d); ST(%d) = v; }' % (s, s)
            if reg == 3: return 'ST(%d) = ST(0); FPOP();' % s
            t = {0xd0: '', 0xe0: 'ST(0) = -ST(0);', 0xe1: 'ST(0) = fabs(ST(0));', 0xe4: 'FCOM(ST(0), 0.0);',
                 0xe5: 'rt_fpu_misc(0xe5);', 0xe8: 'FPUSH(1.0);', 0xe9: 'FPUSH(3.321928094887362347870);',
                 0xea: 'FPUSH(1.442695040888963407360);', 0xeb: 'FPUSH(3.141592653589793238463);',
                 0xec: 'FPUSH(0.301029995663981195214);', 0xed: 'FPUSH(0.693147180559945309417);',
                 0xee: 'FPUSH(0.0);', 0xfa: 'ST(0) = sqrt(ST(0));', 0xfe: 'ST(0) = sin(ST(0));',
                 0xff: 'ST(0) = cos(ST(0));', 0xfc: 'ST(0) = rt_frndint(ST(0));'}
            if modrm in t: return t[modrm]
            return 'rt_fpu_misc(0x%02x);' % modrm
        if op == 0xdd:
            if reg == 0: return 'R.ftag[(R.ftop + %d) & 7] = 1;' % s
            if reg == 2: return 'ST(%d) = ST(0);' % s
            if reg == 3: return 'ST(%d) = ST(0); FPOP();' % s
            if reg == 4: return 'FCOM(ST(0), ST(%d));' % s
            if reg == 5: return 'FCOM(ST(0), ST(%d)); FPOP();' % s
        if op == 0xda and modrm == 0xe9:
            return 'FCOM(ST(0), ST(1)); FPOP(); FPOP();'
        if op == 0xdb:
            if modrm == 0xe2: return 'R.fsw &= 0x7f00;'
            if modrm == 0xe3: return 'FNINIT();'
            if reg in (5, 6): return 'FCOMI(ST(0), ST(%d));' % s
        if op == 0xdf:
            if modrm == 0xe0: return 'AX = FNSTSW();'
            if reg in (5, 6): return 'FCOMI(ST(0), ST(%d)); FPOP();' % s
        raise Unsupported('x87 %02x %02x' % (op, modrm))

    # ---- one instruction ----------------------------------------------------
    def insn(self, i, nxt):
        mn = i.mnemonic
        ops = i.operands
        out = []
        emit = out.append
        def jumpto(t):
            emit('EIP = 0x%xu; return;' % (t & 0xffffffff))

        if i.bytes[0] in (0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf) or \
                (i.bytes[0] == 0x9b and len(i.bytes) > 1 and 0xd8 <= i.bytes[1] <= 0xdf):
            emit(self.x87(i))
            return out
        if mn.startswith('rep ') or mn.startswith('repe ') or mn.startswith('repne ') or \
                mn[:4] in ('movs', 'stos', 'lods', 'cmps', 'scas') and mn[-1] in 'bwd' and len(mn) == 5:
            parts = mn.split(' ')
            rep, base = (parts[0], parts[1]) if len(parts) == 2 else ('', parts[0])
            if base[:4] in ('movs', 'stos', 'lods', 'cmps', 'scas') and len(base) == 5:
                kind = base[:4].upper()
                size = {'b': 1, 'w': 2, 'd': 4}[base[-1]]
                rk = {'': 'REP_NONE', 'rep': 'REP_REP', 'repe': 'REP_REPE', 'repne': 'REP_REPNE'}[rep]
                if kind in ('CMPS', 'SCAS') and rk == 'REP_REP':
                    rk = 'REP_REPE'
                emit('STR_%s(%d, %s);' % (kind, size, rk))
                return out
        if mn in ('nop', 'wait', 'fwait', 'pause'):
            pass
        elif mn == 'mov':
            d, s = ops
            emit(self.wr(i, d, self.rd(i, s, d.size)))
        elif mn in ALU:
            d, s = ops
            sv = self.rd(i, s, d.size)
            if s.type == X86_OP_IMM:
                sv = '0x%xu' % (s.imm & MASK[d.size])
            f = ALU[mn] + SZ[d.size]
            if mn in ('xor', 'sub') and d.type == X86_OP_REG and s.type == X86_OP_REG and d.reg == s.reg:
                emit('%s = %s(%s, %s);' % (self.rd(i, d), f, self.rd(i, d), sv))
            else:
                emit(self.rmw(i, d, lambda x: '%s(%s, %s)' % (f, x, sv)))
        elif mn in ('cmp', 'test'):
            d, s = ops
            fn = ('SUB' if mn == 'cmp' else 'AND') + SZ[d.size]
            emit('(void)%s(%s, %s);' % (fn, self.rd(i, d), self.rd(i, s, d.size)))
        elif mn in ('inc', 'dec', 'neg', 'not'):
            d = ops[0]
            f = mn.upper() + SZ[d.size]
            emit(self.rmw(i, d, lambda x: '%s(%s)' % (f, x)))
        elif mn in ('shl', 'sal', 'shr', 'sar', 'rol', 'ror', 'rcl', 'rcr'):
            d = ops[0]
            cnt = '1' if len(ops) == 1 else self.rd(i, ops[1], 1)
            f = {'sal': 'SHL'}.get(mn, mn.upper()) + SZ[d.size]
            emit(self.rmw(i, d, lambda x: '%s(%s, %s)' % (f, x, cnt)))
        elif mn in ('shld', 'shrd'):
            d, s, c = ops
            if d.size != 4:
                raise Unsupported('shld16')
            cnt = self.rd(i, c, 1)
            emit(self.rmw(i, d, lambda x: '%s32(%s, %s, %s)' % (mn.upper(), x, self.rd(i, s), cnt)))
        elif mn in ('mul', 'div', 'idiv') or (mn == 'imul' and len(ops) == 1):
            s = ops[0]
            emit('%s%s(%s);' % (mn.upper(), SZ[s.size], self.rd(i, s)))
        elif mn == 'imul':
            d = ops[0]
            a, b = (ops[0], ops[1]) if len(ops) == 2 else (ops[1], ops[2])
            bv = ('0x%xu' % (b.imm & MASK[d.size])) if b.type == X86_OP_IMM else self.rd(i, b)
            emit(self.wr(i, d, 'IMUL%sx(%s, %s)' % (SZ[d.size], self.rd(i, a), bv)))
        elif mn == 'lea':
            d, s = ops
            mm = s.mem
            if mm.segment:  # lea ignores segments
                pass
            emit(self.wr(i, d, self.ea(i, s).replace('R.fsbase+', '')))
        elif mn == 'xchg':
            a, b = ops
            if a.type == X86_OP_REG and b.type == X86_OP_REG:
                ra, rb = self.rd(i, a), self.rd(i, b)
                emit('{ %s t = %s; %s = %s; %s = t; }' % (CT[a.size], ra, ra, rb, rb))
            else:
                m_, r_ = (a, b) if a.type == X86_OP_MEM else (b, a)
                rr = self.rd(i, r_)
                emit('{ const uint32_t a_ = %s; %s t = RD%s(a_); WR%s(a_, %s); %s = t; }' % (
                    self.ea(i, m_), CT[m_.size], SZ[m_.size], SZ[m_.size], rr, rr))
        elif mn == 'xadd':
            d, s = ops
            rs = self.rd(i, s)
            if d.type == X86_OP_MEM:
                emit('{ const uint32_t a_ = %s; %s t = RD%s(a_); WR%s(a_, ADD%s(t, %s)); %s = t; }' % (
                    self.ea(i, d), CT[d.size], SZ[d.size], SZ[d.size], SZ[d.size], rs, rs))
            else:
                rd_ = self.rd(i, d)
                emit('{ %s t = %s; %s = ADD%s(t, %s); %s = t; }' % (CT[d.size], rd_, rd_, SZ[d.size], rs, rs))
        elif mn == 'cmpxchg':
            d, s = ops
            acc = {1: 'AL', 2: 'AX', 4: 'EAX'}[d.size]
            if d.type == X86_OP_MEM:
                emit('{ const uint32_t a_ = %s; %s t = RD%s(a_); (void)SUB%s(%s, t); if (ZF) WR%s(a_, %s); else %s = t; }' % (
                    self.ea(i, d), CT[d.size], SZ[d.size], SZ[d.size], acc, SZ[d.size], self.rd(i, s), acc))
            else:
                rd_ = self.rd(i, d)
                emit('{ %s t = %s; (void)SUB%s(%s, t); if (ZF) %s = %s; else %s = t; }' % (
                    CT[d.size], rd_, SZ[d.size], acc, rd_, self.rd(i, s), acc))
        elif mn == 'push':
            s = ops[0]
            if s.type == X86_OP_IMM:
                emit('PUSH32(0x%xu);' % (s.imm & 0xffffffff) if s.size != 2 else 'PUSH16(0x%xu);' % (s.imm & 0xffff))
            elif s.type == X86_OP_REG and i.reg_name(s.reg) in SEGREG:
                emit('PUSH32(%s);' % SEGREG[i.reg_name(s.reg)])
            else:
                emit('{ %s v = %s; PUSH%s(v); }' % (CT[s.size], self.rd(i, s), SZ[s.size]))
        elif mn == 'pop':
            d = ops[0]
            if d.type == X86_OP_REG and i.reg_name(d.reg) in SEGREG:
                emit('(void)POP32();')
            elif d.type == X86_OP_MEM:
                # the address is computed after ESP is incremented (pop [esp+x] sees the new ESP)
                emit('{ %s v = POP%s(); WR%s(%s, v); }' % (CT[d.size], SZ[d.size], SZ[d.size], self.ea(i, d)))
            else:
                emit('%s = POP%s();' % (self.rd(i, d), SZ[d.size]))
        elif mn in ('pushal', 'pushad'):
            emit('{ uint32_t sp0 = ESP; PUSH32(EAX); PUSH32(ECX); PUSH32(EDX); PUSH32(EBX); PUSH32(sp0); '
                 'PUSH32(EBP); PUSH32(ESI); PUSH32(EDI); }')
        elif mn in ('popal', 'popad'):
            emit('{ EDI = POP32(); ESI = POP32(); EBP = POP32(); (void)POP32(); EBX = POP32(); EDX = POP32(); '
                 'ECX = POP32(); EAX = POP32(); }')
        elif mn in ('pushfd', 'pushf'):
            emit('PUSH32(GETFLAGS());')
        elif mn in ('popfd', 'popf'):
            emit('SETFLAGS(POP32());')
        elif mn == 'cbw':
            emit('AX = (uint16_t)(int16_t)(int8_t)AL;')
        elif mn == 'cwde':
            emit('EAX = (uint32_t)(int32_t)(int16_t)AX;')
        elif mn == 'cwd':
            emit('DX = (AX & 0x8000) ? 0xffff : 0;')
        elif mn == 'cdq':
            emit('EDX = (EAX & 0x80000000u) ? 0xffffffffu : 0;')
        elif mn == 'lahf':
            emit('AH = (uint8_t)GETFLAGS();')
        elif mn == 'sahf':
            emit('SETFLAGS((GETFLAGS() & ~0xffu) | AH);')
        elif mn in ('clc', 'stc', 'cmc', 'cld', 'std', 'cli', 'sti'):
            emit({'clc': 'CF = 0;', 'stc': 'CF = 1;', 'cmc': 'CF = !CF;', 'cld': 'DF = 0;', 'std': 'DF = 1;',
                  'cli': '', 'sti': ''}[mn])
        elif mn == 'xlatb':
            emit('AL = RD8(EBX + AL);')
        elif mn in ('movzx', 'movsx'):
            d, s = ops
            v = self.rd(i, s)
            if mn == 'movzx':
                emit(self.wr(i, d, '(%s)(%s)' % (CT[d.size], v)))
            else:
                emit(self.wr(i, d, '(%s)(%s)(%s)(%s)' % (CT[d.size], ST[d.size], ST[s.size], v)))
        elif mn.startswith('set'):
            c = 'j' + mn[3:]
            c = {'jz': 'je', 'jnz': 'jne', 'jc': 'jb', 'jnc': 'jae', 'jnae': 'jb', 'jnb': 'jae', 'jna': 'jbe',
                 'jnbe': 'ja', 'jnge': 'jl', 'jnl': 'jge', 'jng': 'jle', 'jnle': 'jg', 'jpe': 'jp', 'jpo': 'jnp'}.get(c, c)
            emit(self.wr(i, ops[0], '(%s) ? 1 : 0' % CC[c]))
        elif mn.startswith('cmov'):
            c = 'j' + mn[4:]
            emit('if (%s) { %s }' % (CC[c], self.wr(i, ops[0], self.rd(i, ops[1]))))
        elif mn in ('bt', 'bts', 'btr', 'btc'):
            d, s = ops
            bits = d.size * 8
            if d.type == X86_OP_MEM and s.type == X86_OP_REG:
                # memory form with register offset can address outside the operand
                b = self.rd(i, s)
                a = '(uint32_t)(%s + ((int32_t)%s >> 3))' % (self.ea(i, d), b)
                bit = '(%s & 7)' % b
                emit('{ const uint32_t a_ = %s; uint8_t v = RD8(a_); CF = (v >> %s) & 1; %s }' % (a, bit, {
                    'bt': '', 'bts': 'WR8(a_, v | (1u << %s));' % bit, 'btr': 'WR8(a_, v & ~(1u << %s));' % bit,
                    'btc': 'WR8(a_, v ^ (1u << %s));' % bit}[mn]))
            else:
                bit = '((%s) & %d)' % (self.rd(i, s, 1), bits - 1)
                f = {'bt': None, 'bts': lambda x: '(%s | (1u << %s))' % (x, bit), 'btr': lambda x: '(%s & ~(1u << %s))' % (x, bit),
                     'btc': lambda x: '(%s ^ (1u << %s))' % (x, bit)}[mn]
                emit('CF = (%s >> %s) & 1;' % (self.rd(i, d), bit))
                if f:
                    emit(self.rmw(i, d, lambda x: '(%s)%s' % (CT[d.size], f(x))))
        elif mn in ('bsf', 'bsr'):
            d, s = ops
            emit(self.wr(i, d, '%s32(%s, %s)' % (mn.upper(), self.rd(i, s), self.rd(i, d))))
        elif mn == 'bswap':
            r = self.rd(i, ops[0]); emit('%s = BSWAP32(%s);' % (r, r))
        elif mn in ('daa', 'das', 'aaa', 'aas'):
            emit('%s();' % mn.upper())
        elif mn in ('aam', 'aad'):
            emit('%s(%d);' % (mn.upper(), ops[0].imm if ops else 10))
        elif mn == 'cpuid':
            emit('rt_cpuid();')
        elif mn == 'rdtsc':
            emit('rt_rdtsc();')
        elif mn == 'enter':
            n, lvl = ops[0].imm & 0xffff, ops[1].imm & 0x1f
            if lvl:
                raise Unsupported('enter level')
            emit('PUSH32(EBP); EBP = ESP; ESP -= %d;' % n)
        elif mn == 'leave':
            emit('ESP = EBP; EBP = POP32();')
        # ---- control flow ----
        elif mn in CC:
            emit('if (%s) { EIP = 0x%xu; return; }' % (CC[mn], ops[0].imm & 0xffffffff)); jumpto(nxt)
        elif mn in ('jcxz', 'jecxz'):
            emit('if (%s == 0) { EIP = 0x%xu; return; }' % ('ECX' if mn == 'jecxz' else 'CX', ops[0].imm)); jumpto(nxt)
        elif mn in ('loop', 'loope', 'loopne'):
            cond = {'loop': 'ECX', 'loope': 'ECX && ZF', 'loopne': 'ECX && !ZF'}[mn]
            emit('ECX--; if (%s) { EIP = 0x%xu; return; }' % (cond, ops[0].imm)); jumpto(nxt)
        elif mn == 'jmp':
            o = ops[0]
            if o.type == X86_OP_IMM:
                jumpto(o.imm)
            else:
                emit('EIP = %s; return;' % self.rd(i, o))
        elif mn == 'call':
            o = ops[0]
            if o.type == X86_OP_IMM:
                emit('PUSH32(0x%xu); EIP = 0x%xu; return;' % (nxt, o.imm & 0xffffffff))
            else:
                emit('{ uint32_t t = %s; PUSH32(0x%xu); EIP = t; return; }' % (self.rd(i, o), nxt))
        elif mn == 'ret':
            n = ops[0].imm if ops else 0
            emit('EIP = POP32(); %sreturn;' % ('ESP += %d; ' % n if n else ''))
        elif mn == 'int3':
            emit('rt_int(3, 0x%xu); return;' % nxt)
        elif mn == 'int':
            emit('rt_int(0x%x, 0x%xu); return;' % (ops[0].imm & 0xff, nxt))
        elif mn in ('hlt', 'ud2', 'in', 'out', 'insb', 'insd', 'outsb', 'outsd', 'iretd', 'retf', 'ljmp', 'lcall',
                    'les', 'lds', 'lfs', 'lgs', 'lss', 'arpl', 'bound', 'into', 'salc', 'lock'):
            raise Unsupported(mn)
        else:
            raise Unsupported(mn)
        return out

    def block(self, a, insns):
        spec = a in self.p.spec
        body = []
        for k, i in enumerate(insns):
            if i is None:
                body.append('rt_bad(0x%xu); return;' % (insns[k-1].address + insns[k-1].size if k else a))
                break
            if isinstance(i, tuple):
                body.append('EIP = 0x%xu; return;' % i[1])
                break
            nxt = i.address + i.size
            try:
                code = self.insn(i, nxt)
            except (Unsupported, KeyError) as e:
                if not spec:
                    sys.stderr.write('warning: %s: untranslatable instruction at %08x (%s %s): %s; trapping\n' % (
                        self.prefix, i.address, i.mnemonic, i.op_str, e))
                body.append('EIP = 0x%xu; rt_trap(0x%xu, "%s"); return;' % (i.address, i.address, i.mnemonic))
                break
            txt = (i.mnemonic + ' ' + i.op_str)[:40].replace('*/', '* /')
            body.append('/* %08x %-40s */ %s' % (i.address, txt, ' '.join(c for c in code if c)))
            if k == len(insns) - 1 and not any('return;' in c for c in code):
                body.append('EIP = 0x%xu; return;' % nxt)
        return body

    def emit(self, out, parts=1):
        """Write OUT (block tables). With parts > 1 the blocks go to OUT_partN.c files (N = 1..parts), so a
        compiler can build them in parallel; otherwise everything is in OUT."""
        p, P = self.p, self.prefix
        head = ['/* GENERATED by sierra/tools/sprecomp.py from the user\'s own game file. Do not edit, do not commit. */',
                '#include "sierra_rt.h"', '']
        blocks = list(p.blocks())
        addrs = [a for a, _ in blocks]
        fname = lambda a: ('%s_b_%08x' % (P, a)) if parts > 1 else ('b_%08x' % a)
        storage = '' if parts > 1 else 'static '
        chunks = [blocks[k * len(blocks) // parts:(k + 1) * len(blocks) // parts] for k in range(parts)]
        base = out[:-2] if out.endswith('.c') else out
        L = list(head)
        for k, chunk in enumerate(chunks):
            B = L if parts == 1 else list(head)
            for a, insns in chunk:
                B.append('%svoid %s(void) {' % (storage, fname(a)))
                B.extend('    ' + b for b in self.block(a, insns))
                B.append('}')
            if parts > 1:
                open('%s_part%d.c' % (base, k + 1), 'w').write('\n'.join(B) + '\n')
        if parts > 1:
            for k in range(0, len(addrs), 6):
                L.append(' '.join('void %s(void);' % fname(x) for x in addrs[k:k+6]))
        L.append('')
        L.append('static const uint32_t %s_addrs[%d] = {' % (P, len(addrs)))
        for k in range(0, len(addrs), 8):
            L.append('    ' + ' '.join('0x%08xu,' % x for x in addrs[k:k+8]))
        L.append('};')
        L.append('static const rt_block_fn %s_fns[%d] = {' % (P, len(addrs)))
        for k in range(0, len(addrs), 6):
            L.append('    ' + ' '.join('%s,' % fname(x) for x in addrs[k:k+6]))
        L.append('};')
        lo = min(l for l, h in p.pe.code); hi = max(h for l, h in p.pe.code)
        L.append('const rt_program %s_program = { "%s", 0x%08xu, 0x%08xu, 0x%08xu, 0x%08xu, %s_addrs, %s_fns, %d };' % (
            P, P, p.pe.crc, p.pe.base, lo, hi, P, P, len(addrs)))
        open(out, 'w').write('\n'.join(L) + '\n')
        return len(addrs)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe'); ap.add_argument('prefix'); ap.add_argument('out')
    ap.add_argument('--seeds', help='file with extra entry points, one hex address per line')
    ap.add_argument('--parts', type=int, default=1, help='split the blocks over this many C files')
    a = ap.parse_args()
    seeds = []
    if a.seeds:
        try:
            for line in open(a.seeds):
                line = line.split('#')[0].strip()
                if line:
                    seeds.append(int(line, 16))
        except FileNotFoundError:
            pass
    pe = PE(a.exe)
    p = Program(pe, seeds)
    n = Emitter(p, a.prefix).emit(a.out, max(1, a.parts))
    print('%s: %d blocks (%d confirmed, %d speculative), %d instructions, crc %08x' % (
        a.prefix, n, n - len(p.spec), len(p.spec), len(p.ins), pe.crc))

if __name__ == '__main__':
    main()
