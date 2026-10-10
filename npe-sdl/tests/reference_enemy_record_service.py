"""Execute unchanged CODE14:C76E enemy loader. Only file seek/read are adapters.

Read supplies are taken from the real NPE with explicit diagnostic short-read
caps. Compare IO metadata separately; no enemy loader/multiply code is replaced.
"""
import argparse,base64,hashlib,json,re,struct,sys,time
from pathlib import Path
sys.path.insert(0,str(Path('scripts').resolve()))
from web.np560_web import Emulator

p=argparse.ArgumentParser();p.add_argument('--qemu',required=True,type=Path);p.add_argument('--case');args=p.parse_args()
out=Path('tmp/enemy-record-native');out.mkdir(exist_ok=True)
prg=Path('tmp/NP560.PRG').read_bytes();npe=Path('tmp/tjfy-world-probe.npe').read_bytes()
directory=struct.unpack_from('>I',npe,0x3a+4*8)[0]
cases=[]
def case(name,ids,party=3,num=None,base=None,caps=None,scripted=1):
    cases.append(dict(name=name,ids=ids,party=party,num=len(ids) if num is None else num,
                      base=directory if base is None else base,caps=caps or {},scripted=scripted))
case('ordinary-three',[1,2,51]);case('party-zero',[1,2],0);case('party-one',[1],1);case('party-two',[2,1],2)
case('nonpositive-preserve',[0,32768,65535]);case('mixed-preserve',[1,65535,2])
case('high-256',[256]);case('wrap-16385',[16385]);case('signed-offset-8193',[8193]);case('max-positive',[32767])
for cap in range(4):case('directory-short-'+str(cap),[1],caps={0:cap})
case('directory-real-eof',[1],base=len(npe)-1)
case('payload-short-name',[1],caps={1:5})
case('locals-retain-across-enemies',[1,2,1],caps={22:0,30:0,31:0,32:0,41:0,49:0,50:0,51:0})
case('num-zero',[],num=0)
# NUM255 reaches additional RAM aliases and requires a generic IO adapter.
# Earlier fixed-supply diagnostic failures are retained and do not prove its
# original outcome. It is outside these completed loader/full-entry cases.
case('ordinary-loader-three',[1,2,51],scripted=0)
case('ordinary-loader-mixed',[1,65535,2],scripted=2)
case('ordinary-loader-zero',[0,1,0],scripted=0)
case('ordinary-loader-negative',[32768,65535],scripted=255)
case('ordinary-loader-short',[1],scripted=255,caps={6:1,7:0})
if args.case:cases=[c for c in cases if c['name']==args.case]
assert cases
rows=[]
for cfg in cases:
    chunks=[];seeks=[];requests=[];cursor=0
    # Expected file boundary sequence is independent of resident record fields.
    for selection in cfg['ids'][:cfg['num']]:
        if cfg['scripted']==1:
            if not 0<selection<32768:continue
        elif not selection:continue
        displacement=(selection*4-4)&65535
        if displacement>=32768:displacement-=65536
        pos=(cfg['base']+displacement)&0xffffffff;seeks.append(pos)
        local=bytearray(pos.to_bytes(4,'big'))
        n=min(4,cfg['caps'].get(str(len(chunks)),cfg['caps'].get(len(chunks),65535)))
        if pos<=len(npe):cursor=pos
        data=npe[cursor:cursor+n];cursor+=len(data)
        local[:len(data)]=data;chunks.append(data);requests.append(4)
        pos=(cfg['base']+int.from_bytes(local,'big'))&0xffffffff;seeks.append(pos)
        if pos<=len(npe):cursor=pos
        for count in [12,2,1,2,2,2,2,2,2,2,1,1,1,1,1,2,2,1]:
            n=min(count,cfg['caps'].get(str(len(chunks)),cfg['caps'].get(len(chunks),65535)))
            data=npe[cursor:cursor+n];chunks.append(data);requests.append(count);cursor+=len(data)
    resident=bytearray(bytes(0x30000)+prg);caller=bytearray(prg)
    def install(address,words):
        raw=struct.pack('<%dH'%len(words),*words)
        resident[address:address+len(raw)]=raw;caller[address-0x30000:address-0x30000+len(raw)]=raw
    def put(words,address,value):words.extend([0x6810,address>>15,0x68f2,value,0xa8f2,address&0x7fff])
    def asset(address,data):
        raw=bytearray(data+bytes(len(data)&1))
        for i in range(0,len(raw),2):raw[i],raw[i+1]=raw[i+1],raw[i]
        resident[address:address+len(raw)]=raw;caller[address-0x30000:address-0x30000+len(raw)]=raw
    for i,data in enumerate(chunks):asset(0x400000+32*i,data)
    copy=[0x2e00,0x1f10]+[0x58f0|r for r in range(5,8)]+[0x5640,0xac01,0]
    done=len(copy)-1;loop=0x9800+len(copy)*2
    copy += [0x0862,0x0873,0x4c71,0x5c61,0x58f6,0x1f90,0x4d71,0x29a7,
             0x0860,0x0871,0x4c71,0x5c61,0x58f6,0x1f90,0x4d71,0x0b7a,
             0x5011,0x5100,0x5031,0x5120,0x5241,0xac02,loop]
    copy[done]=0x9800+len(copy)*2
    copy += [0x380f|(r<<4) for r in reversed(range(5,8))]+[0x1f90,0x2f00,0xc620]
    install(0x39800,copy)
    # Low RAM diagnostic trace: counts, read (request,supply,destination), seeks.
    seek=[0x2e00,0x1f10]+[0x58f0|r for r in range(3)]+[0x3f10,0x88f2,0x1402]
    branches=[];joins=[]
    for i in range(len(seeks)):
        seek += [0x5620|i,0xac02,0];branches.append(len(seek)-1)
        seek += [0x780b,0xa,0x781b,0xc,0x3f10,0xa8f0,0x1800+4*i,0x3f10,0xa8f1,0x1802+4*i,0xac00,0]
        joins.append(len(seek)-1);seek[branches[-1]]=0x8000+len(seek)*2
    for at in joins:seek[at]=0x8000+len(seek)*2
    seek += [0x5021,0x3f10,0xa8f2,0x1402]+[0x380f|(r<<4) for r in reversed(range(3))]+[0x1f90,0x2f00,0xc620]
    install(0x38000,seek)
    read=[0x2e00,0x1f10]+[0x58f0|r for r in range(7)]+[0x3f10,0x88f6,0x1400]
    joins=[]
    for i,data in enumerate(chunks):
        read += [0x76f6,i] if i>=16 else [0x5660|i]
        read += [0xac02,0];branch=len(read)-1
        read += [0x782b,0xe,0x3f10,0xa8f2,0x1500+8*i,
                 0x68f4,len(data),0x3f10,0xa8f4,0x1502+8*i,
                 0x780b,0xa,0x781b,0xc,0x3f10,0xa8f0,0x1504+8*i,0x3f10,0xa8f1,0x1506+8*i,
                 0x68f2,0x40,0x68f3,32*i,0xac00,0]
        joins.append(len(read)-1);read[branch]=0x8100+len(read)*2
    # Any unexpected read is counted; it supplies zero rather than inventing data.
    read += [0x68f4,0,0x68f2,0,0x68f3,0]
    for at in joins:read[at]=0x8100+len(read)*2
    read += [0x5061,0x3f10,0xa8f6,0x1400,0x780b,0xa,0x781b,0xc,0xbd03,0x9800]
    read += [0x380f|(r<<4) for r in reversed(range(7))]+[0x1f90,0x2f00,0xc620]
    assert 0x8100+len(read)*2<0x9800
    install(0x38100,read);install(0x3401c,[0xad03,0x8000]);install(0x34010,[0xad03,0x8100])
    code=[0x6800,2,0x68ff,0x6000]
    fill=0xff00 if cfg['num']==255 else 0xa5a5
    for address in [0x510000,0x518000,0x520000,0x528000]:
        code += [0x6810,address>>15,0x4810,0x68f0,fill,0xbf00,0x3fff,0x1a10]
    initial=bytearray(fill.to_bytes(2,'big')*65536)
    def seed(address,value):
        initial[address-0x510000:address-0x510000+2]=value.to_bytes(2,'big');put(code,address,value)
    pad=0 if cfg['num']==255 else 0xa5
    seed(0x527af0,(cfg['num']<<8)|pad)
    seed(0x527afa,(cfg['party']<<8)|pad)
    seed(0x527a9a,(cfg['scripted']<<8)|pad)
    seed(0x5279b6,cfg['base']>>16);seed(0x5279b8,cfg['base']&65535)
    for i,id_ in enumerate(cfg['ids']):seed(0x52792e+2*i,id_)
    for address in range(0x1400,0x1820,2):code += [0x4800,0x3f10,0xa8f0,address]
    code += [0xbd14,0xc76e,0x68fc,0x600d] if cfg['scripted']==1 else [0xbd15,0x8a72,0x68fc,0x600d]
    pc=len(code)*2;code.append(0xd0ff);assert len(code)*2<0x8000
    install(0x30000,code)
    fw=out/(cfg['name']+'.prg');rom=out/(cfg['name']+'.rom');fw.write_bytes(caller);rom.write_bytes(resident)
    em=None
    try:
        em=Emulator(qemu=args.qemu,firmware=fw,soc_rom=rom,soc_rom_skeleton=True,boot_mode='firmware-vma03',start_paused=True)
        for address,size in [(0x14c76e,0x1000),(0x158a72,0xeee),(0x1607b0,0x80)]:
            raw=base64.b64decode(em.snapshot('code',address,size,diagnostics=False)['frame'])
            assert raw==prg[address-0x30000:address-0x30000+size]
        em.qmp.execute('cont');deadline=time.monotonic()+15;state=''
        while time.monotonic()<deadline:
            state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=03:{pc:04x}'.lower() in state.lower():break
            time.sleep(.01)
        em.qmp.execute('stop');(out/(cfg['name']+'.cpu.txt')).write_text(state,encoding='utf8')
        assert f'PC=03:{pc:04x}'.lower() in state.lower(),state[:2000]
        assert 'ROM_MISSING=none' in state and 'UNSUPPORTED=none' in state,state[:2000]
        regs={int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})','\n'.join(state.splitlines()[:5]),re.I)}
        assert regs[12]==0x600d and regs[15]==0x6000,regs
        def memory(address,size):
            raw=bytearray()
            for o in range(0,size,65536):raw.extend(base64.b64decode(em.snapshot('data',address+o,min(size-o,65536),diagnostics=False)['frame']))
            for i in range(0,len(raw),2):raw[i],raw[i+1]=raw[i+1],raw[i]
            return bytes(raw)
        ram=memory(0x510000,0x20000);trace=memory(0x1400,0x420)
        nreads,nseeks=struct.unpack_from('>2H',trace)
        assert nreads==len(chunks) and nseeks==len(seeks),(cfg,nreads,nseeks,len(chunks),len(seeks))
        actual_seeks=list(struct.unpack_from('>%dI'%len(seeks),trace,0x400)) if seeks else []
        assert actual_seeks==seeks,(cfg,actual_seeks,seeks)
        reads=[struct.unpack_from('>4H',trace,0x100+8*i) for i in range(nreads)]
        assert [r[0] for r in reads]==requests and [r[1] for r in reads]==list(map(len,chunks)),(cfg,reads,requests)
        path=out/(cfg['name']+'-ram.bin');path.write_bytes(ram)
        (out/(cfg['name']+'-initial.bin')).write_bytes(initial);(out/(cfg['name']+'-trace.bin')).write_bytes(trace)
        row=dict(input=cfg,ram=path.as_posix(),initial=(out/(cfg['name']+'-initial.bin')).as_posix(),
                 seeks=seeks,requests=requests,supplies=[d.hex() for d in chunks],read_destinations=[r[2]*65536+r[3] for r in reads],
                 ram_sha256=hashlib.sha256(ram).hexdigest(),passed=True,stack_restored=True)
        rows.append(row);print(json.dumps(dict(name=cfg['name'],reads=nreads,seeks=nseeks,passed=True)),flush=True)
    finally:
        if em:em.close()
report=dict(passed=len(rows)==len(cases),cases=rows,original_prg_sha256=hashlib.sha256(prg).hexdigest(),npe_sha256=hashlib.sha256(npe).hexdigest(),
            num255_proven=False,num255_failure='tmp/enemy-record-native-verified.log',
            boundaries=['file seek: past EOF retains cursor, validated separately on original public entries','file read supplied from real NPE with explicit short-read caps'],
            scope='Unchanged original enemy loader and multiply, full128KiB RAM and IO trace; no NAND, full battle entry, rendering or exception handling proof')
(out/('report-'+args.case+'.json' if args.case else 'report.json')).write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
