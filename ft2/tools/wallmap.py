# wallmap.py DAT out.svg [xmin xmax ymin ymax] : plot attr-600 walls (blue=listed wall, red=other components), labels
import struct, importlib.util, sys
f, out = sys.argv[1], sys.argv[2]
box = list(map(float, sys.argv[3:7])) if len(sys.argv) > 6 else [-120, 120, -120, 130]
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
x0,x1,y0,y1=box; S=900/max(x1-x0,y1-y0)
def P(x,y): return (x-x0)*S, (y1-y)*S
el=[]
cols={1000:'#36f',1010:'#36f',1016:'#0a0'}
for g in range(len(G)):
    sh=[struct.unpack(f'<{len(d)//2}h',d) for t,d in G[g] if t==10]
    grp=1
    for s in sh:
        for i in range(0,len(s)-1,2):
            if s[i]==602: grp=1<<s[i+1]
    for t,d in G[g]:
        if t!=11: continue
        a=struct.unpack(f'<{len(d)//4}f',d)
        if a[0] not in (600,1300): continue
        if a[0]==1300: continue
        n=int(a[1]); pts=[(a[2+2*i],a[3+2*i]) for i in range(n)]
        col=cols.get(listed.get(g),'#e33') if grp==1 else '#bbb'
        if n==1:
            cx,cy=P(*pts[0]); el.append(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="{a[4]*S:.1f}" fill="none" stroke="{col}"/>')
        else:
            el.append('<polyline fill="none" stroke="%s" points="%s"/>'%(col,' '.join('%.1f,%.1f'%P(*p) for p in pts)))
        lx,ly=P(*pts[0]); el.append(f'<text x="{lx:.0f}" y="{ly:.0f}" font-size="9" fill="#555">{g}{" "+name(g) if name(g) else ""}</text>')
open(out,'w').write(f'<svg xmlns="http://www.w3.org/2000/svg" width="900" height="900"><rect width="900" height="900" fill="white"/>'+''.join(el)+'</svg>')
