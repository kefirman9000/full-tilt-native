"""
ft1draft.py - annotated listing + draft C++ for a Full Tilt 1 table's rules (DRAGON.EXE / PIRATES.EXE).

For every rule function in the table's rule table (ft1rules.py) it prints the lifted code (ft1lift.py) with
  - switch cases named with the decomp's MessageCode names (ft1_names.txt, learned from CADET by ft1rosetta.py)
  - data-segment component pointers named after their component tag (e.g. [0x22fc] -> v_gate1)
  - engine calls and virtual slots named (AddScore, Message, Play, ...)
  - 32-bit float immediates decoded
and, where it recognises the compiler's idiom for comp->Message(code, value), a line of draft C++.
The output is a porting aid for writing <table>_control.cpp by hand; it is derived from the user's own exe, so it
is written locally and not committed.

usage: python ft1draft.py DRAGON.EXE > dragon_draft.txt
"""
import os, re, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ne import NE, far_ptrs, cstr
from ft1lift import Exe, disasm_function, trace_messages, component_vars as comp_vars
from ft1rosetta import rule_table

HERE = os.path.dirname(os.path.abspath(__file__))

def load_names():
    codes, calls, virt = {}, {}, {}
    for line in open(os.path.join(HERE, 'ft1_names.txt')):
        line = line.split('#')[0].split()
        if len(line) == 3 and line[0] == 'code':
            codes[int(line[1])] = line[2]
        elif len(line) == 3 and line[0] == 'call':
            calls[line[1]] = line[2]
        elif len(line) == 3 and line[0] == 'virtual':
            virt[line[1].replace('vslot', '')] = line[2].replace('->', '')
    return codes, calls, virt

def component_vars(path):
    n = NE(path)
    D = n.autodata
    out = {}
    for L, t in far_ptrs(n, D).items():
        if t[0] != D:
            continue
        s = cstr(n, D, t[1])
        if s and 2 < len(s) < 40 and all(32 < ord(c) < 127 for c in s) and ('_' in s or s in ('drain',)):
            out[L + 4] = s
    return out

def as_float(v):
    f = struct.unpack('<f', struct.pack('<I', v & 0xffffffff))[0]
    return ('%g' % f) if abs(f) < 1e7 and (f == 0 or abs(f) > 1e-6) else None

def main():
    path = sys.argv[1]
    exe = Exe(path)
    codes, calls, virt = load_names()
    comps = comp_vars(path)
    rules = rule_table(path)
    byfunc = {}
    for name, f in rules.items():
        byfunc.setdefault(f, []).append(name)

    def code_name(v):
        return 'MessageCode::%s' % codes[v] if v in codes else 'MessageCode(%d) /* FT1 code, not seen in CADET */' % v

    def mem_name(op):
        m = re.search(r'\[(0x[0-9a-f]+)\]', op)
        if m and '[bp' not in op and '[bx' not in op and '[si' not in op and '[di' not in op:
            a = int(m.group(1), 16)
            if a in comps:
                return comps[a]
            if a - 2 in comps:
                return comps[a - 2] + '(seg)'
            return 'g_%04x' % a
        return None

    for (seg, off), names in sorted(byfunc.items()):
        print('=' * 100)
        print('rule function %d:%04x  for components: %s' % (seg, off, ', '.join(sorted(names))))
        cases = {}
        insns = disasm_function(exe, seg, off, cases=cases)
        pending = []      # recent pushes, for recognising comp->Message(code, value)
        for i in insns:
            if i.address in cases:
                print('    case %s:' % ', '.join(code_name(v) for v in cases[i.address]))
                for who, c, v in trace_messages(exe, seg, i.address, lambda a: comps.get(a, 'g_%04x' % a)):
                    if isinstance(c, int):
                        if re.match(r'^(0x[0-9a-f]+|\d+)$', v):
                            iv = int(v, 0)
                            val = (as_float(iv) or str(iv)) if iv else '0.0'
                            val = val if ('.' in val or 'e' in val) else val + '.0'
                        else:
                            val = '/* %s */ value' % v
                        print('      >>> C++: %s->Message(%s, %s);' % (who, code_name(c), val))
                pending = []
            txt = '%04x  %-6s %s' % (i.address, i.mnemonic, i.op_str)
            notes = []
            t = exe.target_at(seg, i)
            if t and t[0] == 'seg':
                key = '%d:%04x' % (t[1], t[2])
                notes.append(calls.get(key, 'engine ' + key) if t[1] != seg else 'rule helper ' + key)
            if i.mnemonic == 'lcall' and i.op_str.startswith('[bx'):
                m = re.search(r'\+ (0x[0-9a-f]+|\d+)\]', i.op_str)
                slot = '+' + (m.group(1) if m else '0')
                notes.append('virtual ' + virt.get(slot, 'slot' + slot))
            mn = mem_name(i.op_str)
            if mn:
                notes.append(mn)
            if re.search(r'es:\[bx \+ 4\]', i.op_str):
                notes.append('->MessageField')
            m = re.match(r'push (0x[0-9a-f]+|\d+)$', i.mnemonic + ' ' + i.op_str)
            if m and 0x66 in i.prefix:
                v = int(m.group(1), 0)
                fl = as_float(v)
                if fl and v > 0x1000:
                    notes.append('float %s' % fl)
            print('      %-48s %s' % (txt, ('; ' + ', '.join(notes)) if notes else ''))
            if i.mnemonic in ('retf', 'ret'):
                pending = []

if __name__ == '__main__':
    main()
