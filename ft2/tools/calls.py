# calls.py ANNOTATED_ASM : collapse pushes + call into "CALL target(args)" lines (cdecl, this = first arg)
import re, sys
helpers = {}
for l in open('' + __import__('os').path.join(__import__('os').path.dirname(__file__), 'alien_helpers.txt') + ''):
    if l.strip() and not l.startswith('#'):
        a, n = l.split(None, 1); helpers[int(a, 16)] = n.strip()
regs = {}
prologue = True
pushes = []
out = []
def val(op):
    op = op.strip()
    if op in regs: return regs[op]
    m = re.match(r'(?:DWORD PTR )?\[ebp([+-]0x[0-9a-f]+)\]', op)
    if m:
        o = int(m.group(1), 16)
        return 'arg%d' % ((o - 8) // 4 + 1) if o > 0 else 'local%x' % -o
    m = re.match(r'0x([0-9a-f]+)$', op)
    if m:
        v = int(m.group(1), 16)
        return str(v) if v < 0x100000 else hex(v)
    return op.replace('DWORD PTR ', '')
for line in open(sys.argv[1]):
    line = line.rstrip('\n')
    if line.startswith('===='):
        out.append(line); regs.clear(); pushes.clear(); prologue = True; continue
    m = re.match(r'  ([0-9a-f]+): (.*?)(  ;.*)?$', line)
    if not m: continue
    addr, ins, note = m.group(1), m.group(2), (m.group(3) or '')
    note = note.replace('  ; ', '')
    mm = re.match(r'mov (e[a-d]x|e[sd]i),(.*)$', ins)
    if mm: regs[mm.group(1)] = val(mm.group(2)) + ('{%s}' % note if note.startswith('text') else ''); continue
    mm = re.match(r'push (.*)$', ins)
    if mm and prologue and mm.group(1) in ('ebp', 'ebx', 'esi', 'edi', 'ecx'):
        continue
    if not mm and not ins.startswith('mov ebp,esp') and not ins.startswith('sub esp') and not ins.startswith('add esp,0xff'):
        prologue = False
    if mm:
        v = val(mm.group(1))
        if note.startswith('text'): v += ' {%s}' % note[5:]
        elif note.endswith('f') and '.' in note: v = note
        pushes.append(v); continue
    mm = re.match(r'call (.*)$', ins)
    if mm:
        t = mm.group(1)
        mt = re.match(r'0x([0-9a-f]+)$', t)
        if mt: t = helpers.get(int(mt.group(1), 16), 'FUN_' + mt.group(1))
        elif re.match(r'DWORD PTR \[e[a-d]x\+0x([0-9a-f]+)\]', t):
            off = int(re.match(r'DWORD PTR \[e[a-d]x\+0x([0-9a-f]+)\]', t).group(1), 16)
            t = 'VIRT+%#x' % off
        args = list(reversed(pushes))
        if t.startswith('VIRT') and args:
            out.append('  %s: %s.%s(%s)' % (addr, args[0], {4: 'Message', 0x18: 'Play', 0xc: 'Score', 0x10: 'Fn10'}.get(int(t[5:], 16), t), ', '.join(args[1:])))
        else:
            out.append('  %s: CALL %s(%s)' % (addr, t, ', '.join(args)))
        pushes.clear(); regs.pop('eax', None); continue
    if ins.startswith(('add esp', 'pop ', 'fstp st(0)', 'mov eax,DWORD PTR [ecx]', 'mov eax,DWORD PTR [eax]', 'mov edx,DWORD PTR [ecx]')): continue
    if re.match(r'(ret|j\w+|cmp|test|inc|dec|mov|sub|add|and|or|xor|lea|fld|fstp|fcomp|setne|sete|imul|neg|shl|sar|cdq|idiv|fild|fmul|fadd|fsub|fdiv|movsx|movzx|fnstsw|sahf)', ins):
        if ins.startswith(('j', 'ret')): pushes.clear()
        out.append('  %s: %s%s' % (addr, ins, ('   ; ' + note) if note else ''))
print('\n'.join(out))
