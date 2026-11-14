# annot.py EXE C_FILE STRINGS FUNCADDR... : objdump of each function, annotated with part names / text ids / helper names
import re, struct, subprocess, sys, bisect
exe, cfile, strfile = sys.argv[1:4]
exec(open('' + __import__('os').path.join(__import__('os').path.dirname(__file__), 'pe.py') + '').read())
p = PE(exe); b = p.b
texts = {}
for l in open(strfile):
    m = re.match(r"(\d+) (.*)", l)
    if m: texts[int(m.group(1))] = m.group(2)[:60]
def s_at(va):
    try:
        o = p.va2off(va); e = b.index(b'\0', o); t = b[o:e]
        return t.decode() if 2 < len(t) < 40 and re.match(rb'^[A-Za-z_][A-Za-z0-9_]*$', t) else None
    except Exception: return None
data = [s for s in p.secs if s[0] in ('DATA', '.data')]
names = {}
for s in p.secs:
    if s[0] in ('CODE', '.text'): continue
    for off in range(s[3], s[3] + s[4] - 4, 4):
        v = struct.unpack_from('<I', b, off)[0]
        n = s_at(v) if v else None
        if n:
            va = p.off2va(off)
            names[va + 4] = n
names[0x440dc4] = 'TABLE'
names.pop(0x440dc4 + 4, None)
helpers = {}
try:
    for l in (x for x in open('' + __import__('os').path.join(__import__('os').path.dirname(__file__), 'alien_helpers.txt') + '') if x.strip() and not x.startswith('#')):
        a, n = l.split(None, 1); helpers[int(a, 16)] = n.strip()
except FileNotFoundError: pass
c = open(cfile, encoding='latin1').read()
starts = sorted(set(int(x, 16) for x in re.findall(r'\bFUN_([0-9a-f]{8})\s*\(', c)))
def end_of(f):
    i = bisect.bisect_right(starts, f); return starts[i] if i < len(starts) else f + 0x400
for fa in [int(x, 16) for x in sys.argv[4:]]:
    out = subprocess.run(['objdump', '-d', '-M', 'intel', '--start-address=%#x' % fa, '--stop-address=%#x' % end_of(fa), exe],
                         capture_output=True, text=True).stdout
    print('==== FUN_%08x %s' % (fa, helpers.get(fa, '')))
    for line in out.splitlines():
        m = re.match(r'\s+([0-9a-f]+):\s+(?:[0-9a-f]{2} )+\s*(.*)', line)
        if not m: continue
        ins = m.group(2).strip()
        ins = re.sub(r'\s+', ' ', ins)
        def rep(mm):
            v = int(mm.group(1), 16)
            if v in names: return '[%s]' % names[v]
            if v - 4 in names: return '[%s+4]' % names[v - 4]
            return mm.group(0)
        ins = re.sub(r'ds:0x([0-9a-f]+)', rep, ins)
        ins = re.sub(r'(DWORD PTR )?\[0x([0-9a-f]+)\]', lambda mm: rep(re.match(r'.*?(?:0x)?([0-9a-f]+)$', mm.group(0)[:-1])) if int(mm.group(2), 16) in names else mm.group(0), ins)
        note = ''
        mc = re.search(r'call\s+0x([0-9a-f]+)', ins)
        if mc and int(mc.group(1), 16) in helpers: note = '  ; ' + helpers[int(mc.group(1), 16)]
        mp = re.search(r'push\s+0x([0-9a-f]+)$', ins)
        if mp:
            v = int(mp.group(1), 16)
            if v in texts: note = '  ; text %d %s' % (v, texts[v])
            elif 0x3f000000 <= v <= 0x47000000: note = '  ; %.2ff' % struct.unpack('<f', struct.pack('<I', v))[0]
            elif v < 0x100000: note = '  ; %d' % v
        print('  %s: %s%s' % (m.group(1), ins, note))
