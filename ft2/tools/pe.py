import struct, sys
class PE:
    def __init__(self, path):
        self.b = open(path,'rb').read()
        b=self.b
        pe = struct.unpack_from('<I', b, 0x3c)[0]
        nsec = struct.unpack_from('<H', b, pe+6)[0]
        optsz = struct.unpack_from('<H', b, pe+20)[0]
        self.base = struct.unpack_from('<I', b, pe+24+28)[0]
        self.secs=[]
        off = pe+24+optsz
        for i in range(nsec):
            name,vsz,va,rsz,raw = struct.unpack_from('<8sIIII', b, off+i*40)
            self.secs.append((name.rstrip(b'\0').decode(), va+self.base, vsz, raw, rsz))
    def off2va(self, off):
        for n,va,vsz,raw,rsz in self.secs:
            if raw <= off < raw+rsz: return va + off - raw
    def va2off(self, v):
        for n,va,vsz,raw,rsz in self.secs:
            if va <= v < va+rsz: return raw + v - va
    def cstr(self, v):
        o=self.va2off(v)
        if o is None: return None
        e=self.b.find(b'\0', o)
        return self.b[o:e].decode('latin1')
    def dw(self, v): return struct.unpack_from('<I', self.b, self.va2off(v))[0]
