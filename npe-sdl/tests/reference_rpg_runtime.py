"""Original allocations -> real NPE reads -> unmodified loader/draw -> RAM.
Only file IO, allocation prompt and allocation-prefix returns are adapters.
"""
import argparse,base64,hashlib,json,re,struct,sys,time
from pathlib import Path
sys.path.insert(0,str(Path('scripts').resolve()))
from web.np560_web import Emulator

parser=argparse.ArgumentParser();parser.add_argument('--qemu',type=Path,required=True)
args=parser.parse_args();out=Path('tmp/rpg-runtime-native');out.mkdir(exist_ok=True)
prg=Path('tmp/NP560.PRG').read_bytes();npe=Path('tmp/tjfy-world-probe.npe').read_bytes()
sections=[struct.unpack_from('>2I',npe,0x3a+8*i) for i in range(100)]
cases=[dict(name=name,number=n,kind=k,x=3,y=3) for name,n,k in [
    ('resident-1',1,0),('resident-29',29,2),('external-514',514,0),
    ('external-31',31,2),('external-34',34,1),('external-512',512,1)]]
cases.append(dict(name='viewer-514',number=514,kind=0,x=0,y=0))
cases.append(dict(name='viewer-1',number=1,kind=0,x=0,y=0))
rows=[]
for cfg in cases:
    chunks=[npe[sections[9][0]:sum(sections[9])]]
    if cfg['number']>29:
        cursor=sections[0][0]+(6*(cfg['number']-29)&65535)
        first=npe[cursor:cursor+3];cursor+=len(first)
        second=npe[cursor:cursor+3];local=bytearray(5)
        local[:len(first)]=first;local[2:2+len(second)]=second
        count=int.from_bytes(local[2:4],'big')
        position=(sections[1][0]+int.from_bytes(local[:2],'big',signed=True))&0xffffffff
        chunks.extend([first,second,npe[position:position+count]])
    sources=[0x400000,0x410000,0x410010,0x410100]
    resident=bytearray(bytes(0x30000)+prg);caller=bytearray(prg)
    def install(address,words):
        data=struct.pack('<%dH'%len(words),*words)
        resident[address:address+len(data)]=data;caller[address-0x30000:address-0x30000+len(data)]=data
    for source,data in zip(sources,chunks):
        raw=bytearray(data+bytes(len(data)&1))
        for i in range(0,len(raw),2):raw[i],raw[i+1]=raw[i+1],raw[i]
        resident[source:source+len(raw)]=raw;caller[source-0x30000:source-0x30000+len(raw)]=raw
    for address in [0x1b2496,0x29d1fc,0x318bc4]:install(address,[0xc620])
    copy=[0x2e00,0x1f10]+[0x58f0|r for r in range(5,8)]+[0x5640,0xac01,0]
    done=len(copy)-1;loop=0x1800+len(copy)*2
    copy += [0x0862,0x0873,0x4c71,0x5c61,0x58f6,0x1f90,0x4d71,0x29a7,
             0x0860,0x0871,0x4c71,0x5c61,0x58f6,0x1f90,0x4d71,0x0b7a,
             0x5011,0x5100,0x5031,0x5120,0x5241,0xac02,loop]
    copy[done]=0x1800+len(copy)*2
    copy += [0x380f|(r<<4) for r in reversed(range(5,8))]+[0x1f90,0x2f00,0xc620]
    install(0x31800,copy)
    seek=[0x2e00,0x1f10,0x58f0,0x58f1,0x58f2,0x780b,0xa,0x781b,0xc,
          0x3f10,0xa8f0,0x1400,0x3f10,0xa8f1,0x1402,
          0x382f,0x381f,0x380f,0x1f90,0x2f00,0xc620]
    install(0x31000,seek)
    read=[0x2e00,0x1f10]+[0x58f0|r for r in range(7)]+[0x3f10,0x88f6,0x1404]
    joins=[]
    for index,(source,data) in enumerate(zip(sources,chunks)):
        read += [0x5660|index,0xac02,0];branch=len(read)-1
        read += [0x68f4,len(data),0x68f2,source>>16,0x68f3,source&65535,0xac00,0];joins.append(len(read)-1)
        read[branch]=0x1100+len(read)*2
    read += [0x4840,0x4820,0x4830]
    for at in joins:read[at]=0x1100+len(read)*2
    read += [0x5061,0x3f10,0xa8f6,0x1404,0x780b,0xa,0x781b,0xc,
             0x58f4,0xbd03,0x1800,0x384f,
             0x3f10,0x88f0,0x1400,0x3f10,0x88f1,0x1402,0x1014,0x5100,
             0x3f10,0xa8f0,0x1400,0x3f10,0xa8f1,0x1402]
    read += [0x380f|(r<<4) for r in reversed(range(7))]+[0x1f90,0x2f00,0xc620]
    install(0x31100,read)
    failed=[0x2e00,0x1f10,0x58f0,0x3f10,0x88f0,0x1406,0x5001,
            0x3f10,0xa8f0,0x1406,0x380f,0x1f90,0x2f00,0xc620]
    install(0x33197a,failed);install(0x3401c,[0xad03,0x1000]);install(0x34010,[0xad03,0x1100])
    code=[0x6800,2,0x68ff,0x6000]
    for address in [0x510000,0x518000,0x520000,0x528000,0x540000]:
        count=0x3fff if address!=0x540000 else 7199
        code += [0x6810,address>>15,0x4810,0x4800,0xbf00,count,0x1a10]
    for address in range(0x1400,0x1408,2):code += [0x4800,0x3f10,0xa8f0,address]
    code += [0xbd30,0x8c1c,0xbd1b,0x23ce,0xbd29,0xd1c8]
    def word(address,value):code.extend([0x6810,address>>15,0x68f2,value,0xa8f2,address&0x7fff])
    def long(address,value):word(address,value>>16);word(address+2,value&65535)
    for address,value in [(0x5279be,sections[9][1]),(0x5279c2,sections[9][0]),
                          (0x5279ca,sections[0][0]),(0x5279ce,sections[1][0])]:long(address,value)
    code += [0xbd31,0x8b80,0x6811,0,0x68f2,0x54,0xa8f2,0x84fc,0x4820,0xa8f2,0x84fe]
    for value in [cfg['kind'],cfg['number'],cfg['y'],cfg['x']]:code += [0x68f0,value,0x58f0]
    code += [0xbd28,0x8ed8,0x50f8,0x68fc,0x600d];pc=len(code)*2;code.append(0xd0ff)
    assert len(code)*2<0x1000;install(0x30000,code)
    firmware=out/(cfg['name']+'.prg');rom=out/(cfg['name']+'.rom')
    firmware.write_bytes(caller);rom.write_bytes(resident);em=None
    try:
        em=Emulator(qemu=args.qemu,firmware=firmware,soc_rom=rom,soc_rom_skeleton=True,boot_mode='firmware-vma03',start_paused=True)
        for address,size in [(0x288ed8,0x2f6),(0xd7c34,0x29a),(0xd7ece,0x364),(0xd8232,0x32e),
                             (0x1b23ce,0xc8),(0x29d1c8,0x34),(0x318b80,0x44),(0x31def6,0x96),
                             (0x314974,0xbe),(0x2577da,0x3e8),(0x315300,0xba),
                             (0x308c1c,0xf6),(0x308b26,0xf6),(0x303a56,0x10c),
                             (0x276100,0x34a),(0x313a72,0xc2)]:
            assert base64.b64decode(em.snapshot('code',address,size,diagnostics=False)['frame'])==prg[address-0x30000:address-0x30000+size]
        em.qmp.execute('cont');deadline=time.monotonic()+20;state=''
        while time.monotonic()<deadline:
            state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=03:{pc:04x}'.lower() in state.lower():break
            time.sleep(.01)
        em.qmp.execute('stop');(out/(cfg['name']+'.cpu.txt')).write_text(state,encoding='utf8')
        assert f'PC=03:{pc:04x}'.lower() in state.lower(),state[:1600]
        assert 'ROM_MISSING=none' in state and 'UNSUPPORTED=none' in state,state[:1600]
        regs={int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})','\n'.join(state.splitlines()[:5]),re.I)}
        assert regs[12]==0x600d and regs[15]==0x6000,regs
        def memory(address,size):
            raw=bytearray()
            for offset in range(0,size,65536):raw.extend(base64.b64decode(em.snapshot('data',address+offset,min(65536,size-offset),diagnostics=False)['frame']))
            for i in range(0,len(raw),2):raw[i],raw[i+1]=raw[i+1],raw[i]
            return bytes(raw)
        packed=memory(0x540000,14400)
        pixels=bytes((packed[(119-y)*120+x//2]>>(0 if x&1 else 4))&15 for y in range(120) for x in range(240))
        row=dict(input=cfg,passed=True,stack_restored=True,heap_headers_seeded=False,draw_stubs=False)
        for label,data in [('ram',memory(0x510000,131072)),('trace',memory(0x1400,8)),('packed',packed),('pixels',pixels)]:
            path=out/(cfg['name']+'-'+label+'.bin');path.write_bytes(data)
            row[label]=path.as_posix();row[label+'_sha256']=hashlib.sha256(data).hexdigest()
        rows.append(row);print(cfg['name'],'original allocations, file loader and draw complete',flush=True)
    finally:
        if em:em.close()
report=dict(passed=True,cases=rows,original_prg_sha256=hashlib.sha256(prg).hexdigest(),npe_sha256=hashlib.sha256(npe).hexdigest(),
            scope='Original allocation prefixes, resource loader and actual drawing with file IO/prompt boundaries. Zero initialized diagnostic RAM; not full firmware startup, NAND, LCD transport or world execution.')
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
