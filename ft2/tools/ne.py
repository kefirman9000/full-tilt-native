import struct, sys
class NE:
    def __init__(self, path):
        d=self.d=open(path,'rb').read()
        ne=struct.unpack_from('<I',d,0x3c)[0]; self.ne=ne
        assert d[ne:ne+2]==b'NE'
        (self.segcnt,)=struct.unpack_from('<H',d,ne+0x1c)
        segtab=ne+struct.unpack_from('<H',d,ne+0x22)[0]
        self.align=struct.unpack_from('<H',d,ne+0x32)[0]
        self.autodata=struct.unpack_from('<H',d,ne+0x0e)[0]
        self.cs_ip=struct.unpack_from('<HH',d,ne+0x14)
        self.segs=[]
        for i in range(self.segcnt):
            off,ln,fl,minalloc=struct.unpack_from('<HHHH',d,segtab+i*8)
            fo=off<<self.align; ln=ln or 0x10000
            self.segs.append(dict(n=i+1,fo=fo,len=ln if off else 0,flags=fl,min=minalloc or 0x10000,data=(fl&1)==1,reloc=(fl&0x100)!=0))
    def seg(self,n):
        s=self.segs[n-1]; return self.d[s['fo']:s['fo']+s['len']]
    def relocs(self,n):
        s=self.segs[n-1]
        if not s['reloc'] or not s['len']: return []
        p=s['fo']+s['len']; cnt=struct.unpack_from('<H',self.d,p)[0]; p+=2; out=[]
        for i in range(cnt):
            atype,rtype,off,a,b=struct.unpack_from('<BBHHH',self.d,p+i*8)
            out.append((atype,rtype,off,a,b))
        return out
if __name__=='__main__':
    n=NE(sys.argv[1]); print('segs',n.segcnt,'autodata',n.autodata,'entry',n.cs_ip)
    for s in n.segs: print(s['n'], 'DATA' if s['data'] else 'CODE', hex(s['fo']), hex(s['len']), hex(s['min']), 'reloc' if s['reloc'] else '')

def far_ptrs(n, segno):
    """Resolve internal far-pointer relocations of a segment: {location: (seg, off)}"""
    d=n.seg(segno); out={}
    for atype,rtype,off,a,b in n.relocs(segno):
        kind=rtype&3; additive=rtype&4
        if kind!=0 or atype!=3: continue
        tgt=(a,b)
        if additive:
            out[off]=tgt; continue
        loc=off; seen=0
        while loc!=0xFFFF and loc+4<=len(d) and seen<5000:
            nxt=struct.unpack_from('<H',d,loc)[0]
            out[loc]=tgt; loc=nxt; seen+=1
    return out
def cstr(n, seg, off):
    d=n.seg(seg); e=d.find(b'\0',off); return d[off:e].decode('latin1')
