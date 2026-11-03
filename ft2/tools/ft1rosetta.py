"""
ft1rosetta.py - learn the Full Tilt 1 engine's names from CADET.EXE + control.cpp.

CADET.EXE's rule table and control.cpp's score_components[] list the same 88 components. For every pair this script
lifts the CADET rule function (ft1lift.py) and reads the C++ function, then aligns them:
  - message codes: the switch cases in the exe vs. the MessageCode names tested in C++ (Full Tilt 1 numbers them
    differently from the Windows XP build the decomp comes from)
  - engine calls: far call targets (seg:off) in order vs. the calls in the C++ body
Writes ft1_names.txt with the learned maps, used to annotate DRAGON.EXE and PIRATES.EXE rule code.

usage: python ft1rosetta.py CADET.EXE ../../SpaceCadetPinball/control.cpp
"""
import collections, os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ft1lift import Exe, disasm_function

def rule_table(exe_path):
    out = subprocess.run([sys.executable, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'ft1rules.py'), exe_path],
                         stdout=subprocess.PIPE, universal_newlines=True).stdout
    rows = {}
    for line in out.splitlines():
        m = re.match(r'0x[0-9a-f]+ (\S+)\s+func (\d+):([0-9a-f]+)', line)
        if m:
            rows[m.group(1)] = (int(m.group(2)), int(m.group(3), 16))
    return rows

def cpp_tables(src):
    tags = dict(re.findall(r'component_tag<\w+>\s+(\w+)\s*=\s*\{"([^"]+)"\}', src))
    comp = {}
    for tagvar, func in re.findall(r'component_info\{(\w+),\s*\{(\w+)', src):
        if tagvar in tags:
            comp[tags[tagvar]] = func
    bodies = {}
    for m in re.finditer(r'\n(?:void|int) control::(\w+)\(MessageCode code, TPinballComponent\* caller\)\n\{', src):
        depth, i = 1, m.end()
        while depth and i < len(src):
            depth += {'{': 1, '}': -1}.get(src[i], 0)
            i += 1
        bodies[m.group(1)] = src[m.end():i - 1]
    return comp, bodies

def cpp_codes(body):
    """MessageCode names the function tests, in order of first appearance"""
    out = []
    for m in re.finditer(r'(?:code\s*==\s*|case\s+)MessageCode::(\w+)', body):
        if m.group(1) not in out:
            out.append(m.group(1))
    return out

def main():
    exe_path, cpp_path = sys.argv[1], sys.argv[2]
    exe = Exe(exe_path)
    rules = rule_table(exe_path)
    comp, bodies = cpp_tables(open(cpp_path, encoding='latin1').read())
    votes = collections.defaultdict(collections.Counter)
    pairs = 0
    for name, (seg, off) in sorted(rules.items()):
        func = comp.get(name)
        if not func or func not in bodies:
            continue
        pairs += 1
        cases = {}
        disasm_function(exe, seg, off, cases=cases)
        exe_codes = []
        for t in sorted(cases):
            for v in cases[t]:
                if v not in exe_codes:
                    exe_codes.append(v)
        names = cpp_codes(bodies[func])
        # one-to-one functions give certain pairs; equal-length lists pair up after sorting by value
        if len(exe_codes) == 1 and len(names) == 1:
            votes[exe_codes[0]][names[0]] += 3
        elif len(exe_codes) == len(names) and exe_codes:
            for v, n in zip(sorted(exe_codes), names):
                votes[v][n] += 1
    # ---- engine calls: co-occurrence of exe call targets and C++ call names over all pairs ----
    exe_sets, cpp_sets = [], []
    for name, (seg, off) in sorted(rules.items()):
        func = comp.get(name)
        if not func or func not in bodies:
            continue
        tg = set()
        for i in disasm_function(exe, seg, off):
            t = exe.target_at(seg, i)
            if i.mnemonic == 'lcall' and t and t[0] == 'seg' and t[1] != seg:
                tg.add('%d:%04x' % (t[1], t[2]))
            elif i.mnemonic == 'lcall' and i.op_str.startswith('[bx'):
                m = re.search(r'\+ (0x[0-9a-f]+|\d+)\]', i.op_str)
                tg.add('vslot+%s' % (m.group(1) if m else '0'))
        body = bodies[func]
        nm = set(re.findall(r'\b(\w+::\w+)\s*\(', body)) | set('->' + x for x in re.findall(r'->\s*(\w+)\s*\(', body))
        nm = {x for x in nm if not x.startswith('MessageCode') and not x.startswith('static_cast')}
        exe_sets.append(tg)
        cpp_sets.append(nm)
    cnt_t, cnt_n, both = collections.Counter(), collections.Counter(), collections.Counter()
    for tg, nm in zip(exe_sets, cpp_sets):
        cnt_t.update(tg); cnt_n.update(nm)
        for t in tg:
            for n in nm:
                both[(t, n)] += 1
    calls = []
    virtual = {}
    for t in sorted(cnt_t):
        if not t.startswith('vslot'):
            continue
        cands = sorted(((both[(t, n)] / (cnt_t[t] + cnt_n[n] - both[(t, n)]), n) for n in cnt_n if both[(t, n)]), reverse=True)
        if cands:
            score, n = cands[0]
            virtual[n] = t
            calls.append('virtual %s %s  # match %.2f, seen %d' % (t, n, score, cnt_t[t]))
    # direct calls: align in order. Drop the virtual methods from the C++ calls; when the remaining count equals
    # the number of direct far calls in the exe function, pair them position by position.
    seqvotes = collections.defaultdict(collections.Counter)
    for name, (seg, off) in sorted(rules.items()):
        func = comp.get(name)
        if not func or func not in bodies:
            continue
        exe_seq = []
        for i in disasm_function(exe, seg, off):
            t = exe.target_at(seg, i)
            if i.mnemonic == 'lcall' and t and t[0] == 'seg' and t[1] != seg:
                exe_seq.append('%d:%04x' % (t[1], t[2]))
        cpp_seq = []
        for m in re.finditer(r'(\b\w+::\w+|->\s*\w+)\s*\(', bodies[func]):
            n = m.group(1).replace(' ', '')
            if n.startswith('->') and n in virtual:
                continue
            if n.startswith('MessageCode') or n.startswith('static_cast'):
                continue
            cpp_seq.append(n)
        if exe_seq and len(exe_seq) == len(cpp_seq):
            for t, n in zip(exe_seq, cpp_seq):
                seqvotes[t][n] += 1
    for t in sorted(seqvotes):
        n, v = seqvotes[t].most_common(1)[0]
        calls.append('call %s %s  # %d of %d aligned functions agree' % (t, n, v, sum(seqvotes[t].values())))
    print('pairs aligned: %d of %d' % (pairs, len(rules)))
    lines = ['# Full Tilt 1 message code -> decomp MessageCode (learned from CADET.EXE + control.cpp)']
    for v in sorted(votes):
        best, n = votes[v].most_common(1)[0]
        lines.append('code %d %s  # votes %d of %d' % (v, best, n, sum(votes[v].values())))
    lines.append('# engine call target -> decomp name (co-occurrence over the 88 CADET/control.cpp pairs)')
    lines += calls
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'ft1_names.txt')
    open(out, 'w').write('\n'.join(lines) + '\n')
    print('\n'.join(lines))

if __name__ == '__main__':
    main()
