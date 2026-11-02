import re,struct,json,collections
exec(open('/home/claude/re/pe.py').read())
exec(open('/home/claude/re/controls.py').read().split('res={}')[0])
out={}
for exe,key in [('Alien.exe','ALIEN.DAT'),('hero.exe','HERO.DAT'),('mad.exe','MAD.DAT')]:
    p=PE(exe); b=p.b
    tags={}
    for m in re.finditer(rb'\0(wave\d+)\0',b):
        sva=p.off2va(m.start()+1); name=m.group(1).decode()
        for t in re.finditer(re.escape(struct.pack('<I',sva)),b):
            tv=p.off2va(t.start())
            for off in range(0,0x1c,4): tags[tv+off]=name
    rules=extract(exe)
    funcs=sorted({int(r['func'],16) for r in rules})
    code=[s for s in p.secs if s[0]=='CODE'][0]
    # all call targets as possible function starts too
    starts=set(funcs)
    cb=b[code[3]:code[3]+code[4]]
    for m in re.finditer(rb'\xe8(....)',cb,re.S):
        tgt=p.off2va(code[3]+m.start())+5+struct.unpack('<i',m.group(1))[0]
        if code[1]<=tgt<code[1]+code[2]: starts.add(tgt)
    starts=sorted(starts)
    import bisect
    fw={}
    for f in funcs:
        i=bisect.bisect_right(starts,f); end=starts[i] if i<len(starts) else f+0x800
        o=p.va2off(f); body=b[o:o+min(end-f,0x1000)]
        ws=[]
        for k in range(len(body)-3):
            v=struct.unpack_from('<I',body,k)[0]
            if v in tags and tags[v] not in ws: ws.append(tags[v])
        fw[f]=ws
    res=[]
    for r in rules:
        ws=fw[int(r['func'],16)]
        if ws: res.append((r['name'],ws))
    out[key]=res
    print(key,len(tags)//7,'wave tags;',len(res),'components with sounds')
json.dump(out,open('ft2_waves.json','w'),indent=0)
