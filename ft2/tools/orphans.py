import struct, sys, importlib.util
spec = importlib.util.spec_from_file_location('datx', '/home/claude/tools/datx.py')
f = sys.argv[1]; sys.argv = [sys.argv[0], f, '0', '0']
m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m); G = m.groups
def name(g):
    for t, d in G[g]:
        if t == 3: return d.rstrip(b'\0').decode('latin1')
listed = {}
for g in range(len(G)):
    if name(g) == 'table_objects':
        for t, d in G[g]:
            if t == 10:
                a = struct.unpack(f'<{len(d)//2}h', d)[1:]
                for i in range(0, len(a), 2): listed[a[i+1]] = a[i]
for g in range(len(G)):
    has600 = any(t == 11 and struct.unpack('<f', d[:4])[0] == 600 for t, d in G[g])
    if has600 and g not in listed:
        prev = max(k for k in listed if k < g); nxt = min((k for k in listed if k > g), default=-1)
        print(f"orphan {g} {name(g)} prev={prev}:{name(prev)}({listed[prev]}) next={nxt}:{name(nxt) if nxt>=0 else ''}({listed.get(nxt)})")
