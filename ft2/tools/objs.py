import struct, sys
sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
import importlib.util
spec = importlib.util.spec_from_file_location('datx', __import__('os').path.join(__import__('os').path.dirname(__import__('os').path.abspath(__file__)), 'datx.py'))
sys.argv = [sys.argv[0], sys.argv[1], '0', '0']; 
m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
G = m.groups
def name(g):
    for t, d in G[g]:
        if t == 3: return d.rstrip(b'\0').decode('latin1')
    return None
def find(n):
    for i in range(len(G)):
        if name(i) == n: return i
T = {1000:'wall',1010:'wall',1001:'plunger',1002:'light',1003:'flipL',1004:'flipR',1005:'bumper',1006:'popup',1007:'drain',1011:'blocker',1012:'kickout',1013:'gate',1014:'kickback',1015:'rollover',1016:'oneway',1017:'sink',1018:'spinner',1019:'solotarget',1020:'lightroll',1021:'ramp',1022:'hole',1023:'demo',1024:'tripwire',1026:'lightgroup',1028:'compgroup',1029:'kickout2',1030:'bargraph',1031:'sound',1032:'timer',1033:'textbox'}
to = find('table_objects')
for t, d in G[to]:
    if t == 10:
        a = struct.unpack(f'<{len(d)//2}h', d)
        a = a[1:]
        from collections import Counter
        c = Counter()
        for i in range(0, len(a), 2):
            ty, g = a[i], a[i+1]
            c[T.get(ty, ty)] += 1
            fl = [struct.unpack('<f', d2[:4])[0] for t2, d2 in G[g] if t2 == 11]
            sh = [struct.unpack(f'<{len(d2)//2}h', d2)[:6] for t2, d2 in G[g] if t2 == 10]
            print(f"{str(T.get(ty,ty)):10s} {g:5d} {name(g)!s:28s} fl={[int(x) for x in fl]} sh={sh}")
        print(c, file=sys.stderr)
