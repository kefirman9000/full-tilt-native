import struct, sys, importlib.util
spec = importlib.util.spec_from_file_location('datx', __import__('os').path.join(__import__('os').path.dirname(__import__('os').path.abspath(__file__)), 'datx.py'))
f = sys.argv[1]; pts = [tuple(map(int, p.split(','))) for p in sys.argv[2:]]
sys.argv = [sys.argv[0], f, '0', '0']
m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m); G = m.groups
def name(g):
    for t, d in G[g]:
        if t == 3: return d.rstrip(b'\0').decode('latin1')
lastname = None
for g in range(len(G)):
    n = name(g) or lastname
    if name(g): lastname = name(g)
    for t, d in G[g]:
        if t == 1:
            res, w, h, x, y, bsz, fl = struct.unpack_from('<BHHhhIB', d, 0)
            if res != 2: continue
            for px, py in pts:
                if x <= px < x + w and y <= py < y + h:
                    s0 = [struct.unpack('<h', dd)[0] for tt, dd in G[g] if tt == 0]
                    print(f"pt {px},{py}: group {g} ({n}) S0={s0} {w}x{h} @{x},{y} flags={fl}")
