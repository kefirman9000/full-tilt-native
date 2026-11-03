"""
ft1lift.py - read Full Tilt 1 table rule code (16-bit NE exes, code segment 23) as annotated assembly.

All three Full Tilt 1 tables (CADET, DRAGON, PIRATES) share one engine: code segments 1-22 and 24 are identical,
only segment 23 (the table rules) differs. CADET's rules are already in control.cpp, so CADET is the reference:
pairing its rule table with control.cpp's score_components[] names the engine functions the rules call
(see ft1rosetta.py). This module gives the shared pieces: decoding a rule function with its relocations resolved
and the Windows floating point emulator calls (INT 34h-3Dh) turned back into x87 instructions.

usage: python ft1lift.py GAME.EXE SEG:OFF   (dump one function)
"""
import os, re, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ne import NE
from capstone import Cs, CS_ARCH_X86, CS_MODE_16

RULE_SEG = 23

class Exe:
    def __init__(self, path):
        self.n = NE(path)
        d = self.n.d
        ne = self.n.ne
        modcnt = struct.unpack_from('<H', d, ne + 0x1e)[0]
        modtab = ne + struct.unpack_from('<H', d, ne + 0x28)[0]
        imptab = ne + struct.unpack_from('<H', d, ne + 0x2a)[0]
        self.mods = []
        for i in range(modcnt):
            o = struct.unpack_from('<H', d, modtab + 2 * i)[0]
            L = d[imptab + o]
            self.mods.append(d[imptab + o + 1:imptab + o + 1 + L].decode())
        self.code = {}
        self.reloc = {}
        for s in self.n.segs:
            if s['data'] or not s['len']:
                continue
            self.code[s['n']], self.reloc[s['n']] = self._load(s['n'])

    def _load(self, seg):
        """segment bytes with FP-emulator calls rewritten to x87, and {offset: target} for relocations"""
        b = bytearray(self.n.seg(seg))
        targets = {}
        for atype, rtype, off, a, bb in self.n.relocs(seg):
            kind, additive = rtype & 3, rtype & 4
            if kind == 0:
                tgt = ('seg', a, bb) if a != 0xff else ('entry', bb)
            elif kind == 1:
                tgt = ('import', self.mods[a - 1], bb)
            elif kind == 2:
                tgt = ('import', self.mods[a - 1], 'name@%d' % bb)
            else:
                continue                      # OS fixups (FP emulator) are decoded below
            if additive:
                targets[off] = (tgt, atype)
                continue
            loc, seen = off, 0
            while loc != 0xFFFF and loc + 2 <= len(b) and seen < 5000:
                nxt = struct.unpack_from('<H', b, loc)[0]
                targets[loc] = (tgt, atype)
                loc, seen = nxt, seen + 1
        # INT 34h..3Bh = x87 escape D8..DF, INT 3Dh = FWAIT (Microsoft FP emulator encoding)
        i = 0
        while i < len(b) - 1:
            if b[i] == 0xCD and 0x34 <= b[i + 1] <= 0x3B:
                b[i], b[i + 1] = 0x9B, 0xD8 + (b[i + 1] - 0x34)
                i += 2
                continue
            if b[i] == 0xCD and b[i + 1] == 0x3D:
                b[i], b[i + 1] = 0x9B, 0x90
                i += 2
                continue
            i += 1
        return bytes(b), targets

    def target_at(self, seg, insn):
        """far-call / far-pointer target recorded by a relocation inside this instruction"""
        rel = self.reloc.get(seg, {})
        for k in range(insn.size):
            t = rel.get(insn.address + k)
            if t:
                return t[0]
        return None

def switch_table(code, insns_before):
    """Compiler switch: 'mov cx, N' ; 'mov bx, TABLE' ; compare loop ; 'jmp word ptr cs:[bx + 2*N]'.
    TABLE holds N dword case values followed by N word targets. Returns [(value, target)] or None."""
    n = tbl = None
    for p in reversed(insns_before[-12:]):
        t = p.mnemonic + ' ' + p.op_str
        if n is None and t.startswith('mov cx, '):
            n = int(p.op_str.split(', ')[1], 0)
        if tbl is None and t.startswith('mov bx, ') and p.op_str.split(', ')[1].startswith('0x'):
            tbl = int(p.op_str.split(', ')[1], 0)
    if n is None or tbl is None or not (0 < n < 64) or tbl + 6 * n > len(code):
        return None
    vals = struct.unpack_from('<%di' % n, code, tbl)
    tgts = struct.unpack_from('<%dH' % n, code, tbl + 4 * n)
    return list(zip(vals, tgts))

def disasm_function(exe, seg, start, limit=0x2000, cases=None):
    """Instructions of the function at seg:start (follows branches and switch tables until every path returns).
    If `cases` is a dict it receives {target: [case values]} for switch statements."""
    md = Cs(CS_ARCH_X86, CS_MODE_16)
    md.detail = True
    code = exe.code[seg]
    seen = {}
    work = [start]
    while work:
        a = work.pop()
        while a not in seen and 0 <= a < len(code) and a < start + limit:
            i = next(md.disasm(code[a:a + 16], a), None)
            if i is None:
                break
            seen[a] = i
            mn = i.mnemonic
            if mn == 'jmp' and i.op_str.startswith('word ptr cs:[bx'):
                tab = switch_table(code, [seen[k] for k in sorted(seen) if k < a])
                if tab:
                    for v, t in tab:
                        work.append(t)
                        if cases is not None:
                            cases.setdefault(t, []).append(v)
                break
            if mn in ('ret', 'retf', 'iret'):
                break
            if (mn.startswith('j') or mn.startswith('loop')) and i.operands and i.operands[0].type == 2:
                work.append(i.operands[0].imm & 0xffff)
                if mn == 'jmp':
                    break
            elif mn == 'jmp':
                break
            a += i.size
    return [seen[k] for k in sorted(seen)]

def describe(exe, seg, i, names=None):
    t = exe.target_at(seg, i)
    txt = '%04x  %-6s %s' % (i.address, i.mnemonic, i.op_str)
    if t:
        if t[0] == 'seg':
            key = '%d:%04x' % (t[1], t[2])
            txt += '    ; -> %s%s' % (key, ' ' + names[key] if names and key in names else '')
        else:
            txt += '    ; -> %s' % (t,)
    return txt

if __name__ == '__main__':
    exe = Exe(sys.argv[1])
    s, o = sys.argv[2].split(':')
    cases = {}
    for i in disasm_function(exe, int(s), int(o, 16), cases=cases):
        if i.address in cases:
            print('      case %s:' % ', '.join(str(v) for v in cases[i.address]))
        print(describe(exe, int(s), i))

def trace_messages(exe, seg, start, comp_name, max_steps=400):
    """Follow one execution path from `start` (unconditional jumps taken, conditional jumps noted, both sides of a
    conditional explored up to a small budget) and return the virtual Message calls made on it:
    [(component_or_operand, code, value_operand)]. Handles the compiler's shared call tails."""
    from capstone import Cs, CS_ARCH_X86, CS_MODE_16
    md = Cs(CS_ARCH_X86, CS_MODE_16)
    md.detail = True
    code = exe.code[seg]
    out = []
    paths = [(start, [])]
    visited = set()
    steps = 0
    while paths and steps < max_steps:
        pc, stack = paths.pop()
        while 0 <= pc < len(code) and steps < max_steps:
            steps += 1
            if (pc, len(stack)) in visited:
                break
            visited.add((pc, len(stack)))
            i = next(md.disasm(code[pc:pc + 16], pc), None)
            if i is None:
                break
            mn, op = i.mnemonic, i.op_str
            if mn == 'push':
                stack.append(op)
            elif mn == 'add' and op.startswith('sp, '):
                stack = []
            elif mn == 'lcall' and op == '[bx + 4]' and len(stack) >= 3:
                this, c, v = stack[-1], stack[-2], stack[-3]
                m = re.search(r'\[(0x[0-9a-f]+)\]', this)
                who = comp_name(int(m.group(1), 16)) if m and '[bp' not in this else ('caller' if '[bp + 0xa]' in this else this)
                try:
                    out.append((who, int(c, 0), v))
                except ValueError:
                    out.append((who, c, v))
                stack = stack[:-3]
            elif mn in ('ret', 'retf'):
                break
            elif mn == 'jmp' and i.operands and i.operands[0].type == 2:
                pc = i.operands[0].imm & 0xffff
                continue
            elif mn == 'jmp':
                break
            elif (mn.startswith('j') or mn.startswith('loop')) and i.operands and i.operands[0].type == 2:
                paths.append((i.operands[0].imm & 0xffff, list(stack)))
            pc += i.size
    return out

def component_vars(path):
    """{data-segment address of a component pointer: component tag name}. Each component tag in the data segment is
    a far pointer to its name followed by the component pointer that the table fills in when it links components."""
    from ne import far_ptrs, cstr
    n = NE(path)
    D = n.autodata
    out = {}
    for L, t in far_ptrs(n, D).items():
        if t[0] != D:
            continue
        name = cstr(n, D, t[1])
        if name and re.match(r'^[a-z][a-z0-9_]{1,38}$', name):
            out[L + 4] = name
    return out
