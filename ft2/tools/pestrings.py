import struct, sys
b=open(sys.argv[1],'rb').read()
pe=struct.unpack_from('<I',b,0x3c)[0]
nsec=struct.unpack_from('<H',b,pe+6)[0]; optsz=struct.unpack_from('<H',b,pe+20)[0]
opt=pe+24
rsrc_rva=struct.unpack_from('<I',b,opt+96+2*8)[0]
secs=[]
s=opt+optsz
for i in range(nsec):
    vs,va,raw_sz,raw=struct.unpack_from('<IIII',b,s+i*40+8)
    secs.append((va,max(vs,raw_sz),raw))
def r2o(rva):
    for va,sz,raw in secs:
        if va<=rva<va+sz: return rva-va+raw
base=r2o(rsrc_rva)
def entries(off):
    n=sum(struct.unpack_from('<HH',b,off+12))
    for i in range(n):
        name,ptr=struct.unpack_from('<II',b,off+16+8*i)
        yield name,ptr
for name,ptr in entries(base):
    if name!=6: continue
    for bid,p2 in entries(base+(ptr&0x7fffffff)):
        for lang,p3 in entries(base+(p2&0x7fffffff)):
            rva,size=struct.unpack_from('<II',b,base+p3)
            d=b[r2o(rva):r2o(rva)+size]; p=0
            for k in range(16):
                n=struct.unpack_from('<H',d,p)[0]; p+=2
                t=d[p:p+2*n].decode('utf-16le','replace'); p+=2*n
                if n: print((bid-1)*16+k, repr(t))
