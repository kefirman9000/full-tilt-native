# hitwall.py DAT x y0 y1 : list wall segments (attr 600 / ramp 1300 etc) crossing vertical segment x, y0..y1
import struct, importlib.util, sys
f, X, Y0, Y1 = sys.argv[1], float(sys.argv[2]), float(sys.argv[3]), float(sys.argv[4])
spec = importlib.util.spec_from_file_location('datx', '/home/claude/tools/datx.py')
sys.argv=['x',f,'0','0']; m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m); G=m.groups
def name(g):
    for t,d in G[g]:
        if t==3: return d.rstrip(b'\0').decode('latin1')
listed={}
for g in range(len(G)):
    if name(g)=='table_objects':
        for t,d in G[g]:
            if t==10:
                a=struct.unpack(f'<{len(d)//2}h',d)[1:]
                for i in range(0,len(a),2): listed[a[i+1]]=a[i]
for g in range(len(G)):
    sh=[struct.unpack(f'<{len(d)//2}h',d) for t,d in G[g] if t==10]
    for t,d in G[g]:
        if t!=11: continue
        a=struct.unpack(f'<{len(d)//4}f',d)
        if a[0]!=600: continue
        n=int(a[1]); pts=[(a[2+2*i],a[3+2*i]) for i in range(n)]
        if n==1: segs=[]  # circle
        else: segs=list(zip(pts,pts[1:]))
        if n==1:
            cx,cy=pts[0]; r=a[4]
            if abs(cx-X)<r+3.3 and Y0-r<cy<Y1+r: print('circle',g,name(g),listed.get(g),pts[0],r,sh)
        for (x1,y1),(x2,y2) in segs:
            if (x1-X)*(x2-X)<=0 and x1!=x2:
                yy=y1+(y2-y1)*(X-x1)/(x2-x1)
                if Y0<=yy<=Y1: print('seg',g,name(g),'type',listed.get(g),'y=%.1f'%yy,(x1,y1),(x2,y2),sh)
