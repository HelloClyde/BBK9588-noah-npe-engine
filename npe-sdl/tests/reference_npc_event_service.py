"""Unchanged original18:4360 NPC loader and23:6798 conditions.
Only the file seek/read boundaries use the established diagnostic IO adapter.
Synthetic cases change NPE record bytes, never the loader/condition program.
"""
import argparse,base64,hashlib,json,re,struct,sys,time
from pathlib import Path
sys.path.insert(0,str(Path('scripts').resolve()))
from web.np560_web import Emulator
p=argparse.ArgumentParser();p.add_argument('--qemu',required=True,type=Path);p.add_argument('--case');args=p.parse_args()
out=Path('tmp/npc-event-native');out.mkdir(exist_ok=True)
prg=Path('tmp/NP560.PRG').read_bytes();original=Path('tmp/tjfy-world-probe.npe').read_bytes()
directory=struct.unpack_from('>I',original,0x3a+5*8)[0]
cases=[dict(name='original-npc1',id=1),dict(name='original-clamp',id=1,state=255),dict(name='original-three-success',id=33,state=0),dict(name='original-three-money-fail',id=33,money=0),dict(name='type0-retains-guards',id=1,guards=[1,1]),dict(name='directory-next-entry',id=1,base=directory+5)]
for kind in range(1,7):cases.append(dict(name='synthetic-type'+str(kind),id=1,kind=kind))
cases += [dict(name='six-last-fail',id=1,kind=6,conditions=[(3,0)]*5+[(1,65535)]),dict(name='six-first-fail',id=1,kind=6,conditions=[(1,65535)]+[(6,32)]*5),dict(name='six-paired-guards',id=1,kind=6,conditions=[(6,32),(6,1),(7,45),(7,2),(4,1),(8,60)]),dict(name='six-unpaired-guard',id=1,kind=6,conditions=[(3,0)]*5+[(6,32)]),dict(name='unknown-condition-true',id=1,kind=1,conditions=[(255,0)]),dict(name='level-fail',id=1,kind=1,conditions=[(4,21)]),dict(name='money-negative',id=1,kind=1,conditions=[(8,50)],money=0x80000064),dict(name='money-overflow',id=1,kind=1,conditions=[(8,1)],money=0x80000000),dict(name='condition5-alias',id=1,kind=1,conditions=[(5,200)]),dict(name='condition9-word-wrap',id=1,kind=1,conditions=[(9,0)],extra=0xa5)]
cases += [dict(name='condition2-high-index-empty',id=1,kind=1,conditions=[(2,65535)],extra54=0),dict(name='condition2-high-index-owned',id=1,kind=1,conditions=[(2,65535)],extra54=1),dict(name='condition7-high-index-pair',id=1,kind=2,conditions=[(7,65535),(7,1)],extra54=1),dict(name='condition6-high-index-pair',id=1,kind=2,conditions=[(6,65535),(6,1)]),dict(name='condition5-high-index',id=1,kind=1,conditions=[(5,65535)])]
if args.case:cases=[c for c in cases if c['name']==args.case]
rows=[]
for cfg in cases:
    npe=bytearray(original);base=cfg.get('base',directory);id_=cfg['id'];state_=cfg.get('state',0)
    if 'kind' in cfg:
        record=directory+struct.unpack_from('>I',npe,directory+1)[0]
        npe[record]=cfg['kind']
        for i,(kind,value) in enumerate(cfg.get('conditions',[(3,0)]*6)):
            npe[record+2+3*i]=kind;struct.pack_into('>H',npe,record+3+3*i,value)
        # Distinct known text offsets make success/failure selection observable.
        for i,npc in enumerate([1,24,25]):
            raw=directory+struct.unpack_from('>I',original,directory+(npc-1)*5+1)[0]
            npe[record+26+4*i:record+30+4*i]=original[raw+26:raw+30]
    path=out/(cfg['name']+'.npe');path.write_bytes(npe)
    def signed(v):return v-65536 if v>=32768 else v
    displacement=signed((5*id_-5)&65535);first=(base+displacement)&0xffffffff
    count=npe[first];offset=struct.unpack_from('>I',npe,first+1)[0];index=min(state_,count-1)
    second=(base+offset+signed((38*index)&65535))&0xffffffff
    requests=[1,4,1,1]+[n for i in range(6) for n in [1,2]]+[1,2,1,2,4,4,4]
    chunks=[bytes(npe[first:first+1]),bytes(npe[first+1:first+5])];cursor=second
    for n in requests[2:]:chunks.append(bytes(npe[cursor:cursor+n]));cursor+=n
    seeks=[first,second]
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
    fill=0xa5a5
    for address in [0x510000,0x518000,0x520000,0x528000,0x530000,0x538000,0x540000,0x548000]:
        code += [0x6810,address>>15,0x4810,0x68f0,fill,0xbf00,0x3fff,0x1a10]
    initial=bytearray(fill.to_bytes(2,'big')*131072)
    def seed(address,value):
        initial[address-0x510000:address-0x510000+2]=value.to_bytes(2,'big');put(code,address,value)
    def byte(address,value):
        off=(address&~1)-0x510000;word=int.from_bytes(initial[off:off+2],'big')
        seed(address&~1,(word&0xff00)|value if address&1 else (word&255)|(value<<8))
    seed(0x5279f2,base>>16);seed(0x5279f4,base&65535);seed(0x527936,id_);seed(0x527a66,30)
    money=cfg.get('money',100);seed(0x52799e,money>>16);seed(0x5279a0,money&65535)
    for a,v in [(0x526fba+id_,state_),(0x526fba+32,1),(0x5269de,20),(0x527a94,cfg.get('guards',[0,0])[0]),(0x527a92,cfg.get('guards',[0,0])[1])]:byte(a,v)
    seed(0x5267c8+45*2,2)
    if 'extra' in cfg:seed(0x53742a,cfg['extra'])
    if 'extra54' in cfg:seed(0x5467c6,cfg['extra54'])
    for address in range(0x1400,0x1820,2):code += [0x4800,0x3f10,0xa8f0,address]
    code += [0xbd18,0x4360,0x68fc,0x600d];pc=len(code)*2;code.append(0xd0ff)
    install(0x30000,code);root=out/cfg['name'];root.mkdir(exist_ok=True)
    fw=root/'caller.prg';rom=root/'adapter.rom';fw.write_bytes(caller);rom.write_bytes(resident)
    em=None
    try:
        em=Emulator(qemu=args.qemu,firmware=fw,soc_rom=rom,soc_rom_skeleton=True,boot_mode='firmware-vma03',start_paused=True)
        for address,size in [(0x184360,0xb02),(0x236798,0x4a4)]:
            raw=base64.b64decode(em.snapshot('code',address,size,diagnostics=False)['frame']);assert raw==prg[address-0x30000:address-0x30000+size]
        em.qmp.execute('cont');deadline=time.monotonic()+15;state=''
        while time.monotonic()<deadline:
            state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=03:{pc:04x}'.lower() in state.lower():break
            time.sleep(.01)
        em.qmp.execute('stop');(root/'cpu.txt').write_text(state,encoding='utf-8')
        assert f'PC=03:{pc:04x}'.lower() in state.lower(),state[:1600]
        assert all(k+'=none' in state for k in ['ROM_MISSING','UNSUPPORTED','INVALID_CONTROL']),state[:1600]
        regs={int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})','\n'.join(state.splitlines()[:5]),re.I)}
        assert regs[12]==0x600d and regs[15]==0x6000,regs
        def memory(address,size):
            raw=bytearray()
            for o in range(0,size,65536):raw.extend(base64.b64decode(em.snapshot('data',address+o,min(size-o,65536),diagnostics=False)['frame']))
            raw[0::2],raw[1::2]=raw[1::2],raw[0::2];return bytes(raw)
        ram=memory(0x510000,0x40000);trace=memory(0x1400,0x420)
        (root/'bank53.bin').write_bytes(memory(0x530000,65536))
        nreads,nseeks=struct.unpack_from('>2H',trace);assert nreads==len(chunks) and nseeks==2,(nreads,nseeks)
        actual_seeks=list(struct.unpack_from('>2I',trace,0x400));assert actual_seeks==seeks,(actual_seeks,seeks)
        reads=[struct.unpack_from('>4H',trace,0x100+8*i) for i in range(nreads)]
        assert [r[0] for r in reads]==requests and [r[1] for r in reads]==list(map(len,chunks))
        (root/'before.bin').write_bytes(initial);(root/'after.bin').write_bytes(ram);(root/'trace.bin').write_bytes(trace)
        row=dict(input=cfg,npe=path.as_posix(),root=root.as_posix(),seeks=seeks,requests=requests,cursor=cursor,passed=True,program_unchanged=True,stack_restored=True,synthetic_resource='kind' in cfg)
        (root/'report.json').write_text(json.dumps(row,indent=2),encoding='utf-8');rows.append(row)
        print(cfg['name'],nreads,nseeks,'passed',flush=True)
    finally:
        if em:em.close()
(out/('report-'+args.case+'.json' if args.case else 'report.json')).write_text(json.dumps(dict(passed=True,cases=rows,original_prg_sha256=hashlib.sha256(prg).hexdigest(),original_npe_sha256=hashlib.sha256(original).hexdigest(),scope='Unchanged original loader and condition functions with adapted file IO; no NAND/stack-alias/short-read/rendering equivalence claim'),indent=2),encoding='utf-8')
