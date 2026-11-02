# Dump a Full Tilt 1 (16-bit NE) table exe's rule table: component name, rule function (seg:off), scores
import sys, struct
sys.path.insert(0, __import__("os").path.dirname(__file__)); from ne import NE, far_ptrs, cstr
n=NE(sys.argv[1]); D=n.autodata; d=n.seg(D); fp=far_ptrs(n,D)
def name_of_tag(t):
    p=fp.get(t)
    if not p or p[0]!=D: return None
    s=cstr(n,D,p[1])
    return s if s and all(32<c<127 for c in s.encode('latin1')) else None
rows=[]; simple=[]
for L in sorted(fp):
    t=fp[L]
    if t[0]!=D: continue
    nm=name_of_tag(t[1])
    if nm is None: continue
    f=fp.get(L+4)
    if f and f[0]!=D:
        cnt=struct.unpack_from('<h',d,L+8)[0]
        sp=fp.get(L+10); scores=[]
        if sp and sp[0]==D and 0<=cnt<64:
            scores=list(struct.unpack_from('<%di'%cnt,d,sp[1]))
        elif cnt and not sp: scores=None
        rows.append((L,nm,f,cnt,scores))
    else:
        simple.append((L,nm))
for L,nm,f,cnt,sc in rows:
    print('0x%04x %-28s func %2d:%04x scores %s'%(L,nm,f[0],f[1],sc))
print('# simple components (tags without a rule entry): %d'%len(simple))
print('# '+' '.join(nm for L,nm in simple))
