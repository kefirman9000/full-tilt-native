# annc.py EXE C STRINGS HELPERS OUT : label an angr C dump with part names, texts and helper names
import sys, re, struct
sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
exe, cfile, strfile, helpfile, out = sys.argv[1:6]
exec(open(__import__('os').path.join(__import__('os').path.dirname(__import__('os').path.abspath(__file__)), 'pe.py')).read())
p = PE(exe); b = p.b
texts = {}
for l in open(strfile):
    m = re.match(r"(\d+) '(.*)'$", l.rstrip())
    if m: texts[int(m.group(1))] = m.group(2)[:70]
def s_at(va):
    try:
        o = p.va2off(va); e = b.index(b'\0', o); t = b[o:e]
        return t.decode() if 2 < len(t) < 40 and re.match(rb'^[A-Za-z_][A-Za-z0-9_]*$', t) else None
    except Exception: return None
names = {}
for s in p.secs:
    if s[0] in ('CODE', '.text'): continue
    for off in range(s[3], s[3] + s[4] - 4, 4):
        v = struct.unpack_from('<I', b, off)[0]
        n = s_at(v) if v else None
        if n: names[p.off2va(off) + 4] = n
helpers = {}
for l in open(helpfile):
    if l.strip() and not l.startswith('#'):
        a, n = l.split(None, 1); helpers[int(a, 16)] = n.strip()
c = open(cfile).read()
def g(m):
    v = int(m.group(1), 16)
    if v in names: return 'P_' + names[v]
    return m.group(0)
c = re.sub(r'\bg_([0-9a-f]+)\b', g, c)
c = re.sub(r'\bsub_([0-9a-f]+)\b', lambda m: helpers.get(int(m.group(1), 16), m.group(0)) if int(m.group(1),16) in helpers else m.group(0), c)
def t(m):
    line = m.group(0); ids = [int(x) for x in re.findall(r'\b(3\d{3})\b', line) if int(x) in texts]
    return line + ('  // ' + ' | '.join('%d "%s"' % (i, texts[i]) for i in ids) if ids else '')
c = re.sub(r'^.*\b3\d{3}\b.*$', t, c, flags=re.M)
c = re.sub(r'\b0x([0-9a-f]{4,7})\b(?![0-9a-f])', lambda m: m.group(0) + ('/*%d*/' % int(m.group(1), 16)) if int(m.group(1),16) % 1000 == 0 else m.group(0), c)
open(out, 'w').write(c)
print(len(names), 'names', len(texts), 'texts')
