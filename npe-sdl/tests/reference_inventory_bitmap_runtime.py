"""Original allocations -> real NPE reads -> unmodified role interpolation/draw -> RAM.
Only file IO, allocation prompt and allocation-prefix returns are adapters.
"""
import argparse,base64,hashlib,json,re,struct,sys,time
from pathlib import Path
sys.path.insert(0,str(Path('scripts').resolve()))
from web.np560_web import Emulator

parser=argparse.ArgumentParser();parser.add_argument('--qemu',type=Path,required=True);parser.add_argument('--case')
args=parser.parse_args();out=Path('tmp/inventory-bitmap-runtime-native');out.mkdir(exist_ok=True)
prg=Path('tmp/NP560.PRG').read_bytes();npe=Path('tmp/tjfy-world-probe.npe').read_bytes()
sections=[struct.unpack_from('>2I',npe,0x3a+8*i) for i in range(100)]
vectors=struct.unpack_from('>17H',npe,0x18)
cases=[dict(name=name,service='battle',number=n,kind=k,x=3,y=3) for name,n,k in [
    ('resident-1',1,0),('resident-29',29,2),('external-514',514,0),
    ('external-31',31,2),('external-34',34,1),('external-512',512,1)]]
cases += [dict(name='viewer-'+str(n),service='battle',number=n,kind=0,x=0,y=0) for n in [514,1]]
def add(name,index,kind=0,x=0,y=4,**extra):
    cases.append(dict(name=name,service='indexed',number=index,kind=kind,x=x,y=y,**extra))
add('tile-0',0)
add('actor-1',vectors[1]+1,2,16,20)
add('enemy-1',vectors[4]+1,0,42,38)
add('enemy-odd-background',vectors[4]+1,1,43,38)
add('actor-odd-transparent',vectors[1]+1,2,17,20)
add('negative-index',-1)
add('negative-kind',0,-1)
add('kind-3',0,3)
add('directory-bound',sections[0][1]//8)
add('wrapped-4095',4095)
add('wrapped-4096',4096)
add('wrapped-8192',8192)
add('wrapped-32767',32767)
add('negative-x',0,2,-1,4)
add('short-directory-2',0,directory=len(npe)-2)
add('eof-directory',0,directory=len(npe))
add('negative-directory-length',0,directory_size=0x80000000)
add('directory-size-8-first',0,directory_size=8)
add('directory-size-8-next',1,directory_size=8)
add('payload-eof',0,payload=len(npe))
add('payload-short-2',0,payload=len(npe)-2)
for name,group,kind,x in [('opening',1,0,0),('local-opaque',2,0,60),
    ('local-background',2,1,60),('local-transparent',2,2,60),
    ('local-odd-background',2,1,61),('local-odd-transparent',2,2,61),('ending',3,0,0)]:
    cases.append(dict(name=name,service='scene',number=group,kind=kind,x=x,y=0))
for name,group in [('scene-zero',0),('scene-negative',-1),('scene-empty',4)]:
    cases.append(dict(name=name,service='scene',number=group,kind=0,x=0,y=0))
cases.append(dict(name='scene-directory-eof',service='scene',number=1,kind=0,x=0,y=0,scene_directory=len(npe)))
cases.append(dict(name='scene-directory-short',service='scene',number=1,kind=0,x=0,y=0,scene_directory=len(npe)-2))
cases.append(dict(name='scene-payload-eof',service='scene',number=2,kind=0,x=60,y=0,scene_payload=len(npe)))
def role(name,group,kind=2,x=60,y=40,end_x=None,end_y=None,**extra):
    cases.append(dict(name=name,service='role',number=group,kind=kind,x=x,y=y,
                      end_x=x if end_x is None else end_x,end_y=y if end_y is None else end_y,**extra))
role('role-cast',2)
role('role-flee',1)
role('role-combine',15,0)
role('role-background-odd',2,1,x=61)
role('role-transparent-odd',2,2,x=61)
role('role-forward',2,end_x=53,end_y=35)
role('role-backward',2,end_x=67,end_y=45)
role('role-ties',16,end_x=59,end_y=41)
role('role-small-fractions',15,end_x=59,end_y=41)
role('role-single-stationary',57)
role('role-single-moving',57,x=80,y=64,end_x=16,end_y=32)
role('role-single-mixed',57,x=80,y=32,end_x=16,end_y=65)
role('role-single-reversed',57,x=16,y=32,end_x=80,end_y=64)
role('role-single-small',57,x=32,y=32,end_x=25,end_y=25)
role('role-zero',0)
role('role-negative',-1)
role('role-empty',51)
role('role-directory-eof',2,role_directory=len(npe))
role('role-directory-short',2,role_directory=len(npe)-2)
role('role-payload-eof',2,role_payload=len(npe))

def ui(name,offset,x,y,kind=0,group=7,**extra):
    base=extra.get('ui_base',vectors[7]) if group==7 else extra.get('portrait_base',vectors[6])
    add('ui-'+name,(base+offset)&65535,kind,x,y,ui_group=group,ui_offset=offset,**extra)
for name,offset,x,y,kind in [
    ('money',1,0,7,0),('parent',2,0,37,0),('property',3,42,37,0),
    ('item',4,42,70,0),('system',5,42,37,0),('music',6,105,70,0),
    ('quantity',9,30,5,0),('equipment',10,0,0,0),('selector-first',11,7,78,2),
    ('selector-last',11,197,78,2),('save',12,105,46,0),('title',15,0,0,0),
    ('member',18,0,0,0),('story',19,0,0,0)]:ui(name,offset,x,y,kind)
for member in range(3):
    char=sections[12][0]+struct.unpack_from('>I',npe,sections[12][0]+member*4)[0]
    portrait=struct.unpack_from('>H',npe,char+33)[0]
    ui('portrait-'+str(member+1),portrait,14,9,group=6)
for name,offset,x,y in [('drop',31,40,30),('learn',32,36,40),('level',33,10,14)]:
    ui(name,offset,x,y,group=6)
ui('base-shift',6,105,70,ui_base=vectors[7]+1)
ui('base-wrap',11,7,78,2,ui_base=65530)

def inventory(name,group,offset,x,y,**extra):
    base=extra.get('image_base',vectors[group])
    add('inventory-'+name,(base+offset)&65535,0,x,y,
        image_group=group,inventory_offset=offset,**extra)
for column,x in enumerate([25,98,171]):
    inventory('item-cursor-'+str(column),7,17,x,18 if column<2 else 66)
    inventory('magic-cursor-'+str(column),7,17,x,13 if column<2 else 45)
inventory('item-list',5,45,10,72)
inventory('item-target',5,45,11,63)
inventory('item-empty-255',5,255,10,72)
for member in range(3):
    char=sections[12][0]+struct.unpack_from('>I',npe,sections[12][0]+member*4)[0]
    portrait=struct.unpack_from('>H',npe,char+33)[0]
    inventory('portrait-list-'+str(member),6,portrait,1,52)
    inventory('portrait-item-target-'+str(member),6,portrait,1,1)
    inventory('portrait-magic-target-'+str(member),6,portrait,12,1)
inventory('skill-1',8,1,22,63)
inventory('skill-5',8,5,22,63)
inventory('item-base-shift',5,45,10,72,image_base=vectors[5]+1)
inventory('item-base-wrap',5,11,10,72,image_base=65530)
inventory('skill-base-shift',8,5,22,63,image_base=vectors[8]+1)
inventory('skill-base-wrap',8,11,22,63,image_base=65530)
assert len(cases)==108

def signed_word(value):
    value &=65535
    return value if value<32768 else value-65536
def signed_long(value):
    return value if value<0x80000000 else value-0x100000000
rows=[]
for cfg in cases:
    if args.case and cfg['name']!=args.case:continue
    chunks=[npe[sections[9][0]:sum(sections[9])]]
    if cfg['service']=='battle' and cfg['number']>29:
        cursor=sections[0][0]+(6*(cfg['number']-29)&65535)
        first=npe[cursor:cursor+3];cursor+=len(first)
        second=npe[cursor:cursor+3];local=bytearray(5)
        local[:len(first)]=first;local[2:2+len(second)]=second
        count=int.from_bytes(local[2:4],'big')
        position=(sections[1][0]+int.from_bytes(local[:2],'big',signed=True))&0xffffffff
        chunks.extend([first,second,npe[position:position+count]])
    elif cfg['service']=='indexed':
        valid=cfg['number']>=0 and 0<=cfg['kind']<=2 and signed_word(cfg['number']*8+8)<=signed_long(cfg.get('directory_size',sections[0][1]))
        if valid:
            cursor=(cfg.get('directory',sections[0][0])+signed_word(cfg['number']*8))&0xffffffff
            first=npe[cursor:cursor+4];cursor+=len(first)
            second=npe[cursor:cursor+4]
            relative=int.from_bytes(first+bytes(4-len(first)),'big')
            length=int.from_bytes(second+bytes(4-len(second)),'big')
            chunks.extend([first,second])
            if relative<0x80000000 and length:
                position=(cfg.get('payload',sections[1][0])+relative)&0xffffffff
                chunks.append(npe[position:position+(length&65535)])
    elif cfg['service'] in ('scene','role') and cfg['number']>0:
        directory=cfg.get(cfg['service']+'_directory',sections[13 if cfg['service']=='scene' else 6][0])
        cursor=(directory+signed_word(cfg['number']*5-5))&0xffffffff
        first=npe[cursor:cursor+4];cursor+=len(first);second=npe[cursor:cursor+1]
        chunks.extend([first,second]);relative=int.from_bytes(first+bytes(4-len(first)),'big')
        if relative<0x80000000 and second and second[0]:
            cursor=(directory+relative)&0xffffffff
            offset=npe[cursor:cursor+4];cursor+=len(offset);length=npe[cursor:cursor+4]
            chunks.extend([offset,length]);position=int.from_bytes(offset+bytes(4-len(offset)),'big')
            count=int.from_bytes(length+bytes(4-len(length)),'big')
            if position<0x80000000 and count:
                cursor=(cfg.get(cfg['service']+'_payload',sections[14 if cfg['service']=='scene' else 7][0])+position)&0xffffffff
                for _ in range(second[0]):
                    payload=npe[cursor:cursor+(count&65535)];cursor+=len(payload);chunks.append(payload)
    sources=[0x400000];source=0x100000
    for data in chunks[1:]:
        span=max(0x1000,(len(data)+0xfff)&~0xfff)
        if source<0x1c0000 and source+span>0x1b0000:source=0x1c0000
        if source<0x200000 and source+span>0x1f0000:source=0x200000
        assert source+span<0x240000
        sources.append(source);source+=span
    resident=bytearray(bytes(0x30000)+prg);caller=bytearray(prg)
    def install(address,words):
        data=struct.pack('<%dH'%len(words),*words)
        resident[address:address+len(data)]=data;caller[address-0x30000:address-0x30000+len(data)]=data
    for source,data in zip(sources,chunks):
        raw=bytearray(data+bytes(len(data)&1))
        for i in range(0,len(raw),2):raw[i],raw[i+1]=raw[i+1],raw[i]
        if source+len(raw)>len(resident):resident.extend(bytes(source+len(raw)-len(resident)))
        if source-0x30000+len(raw)>len(caller):caller.extend(bytes(source-0x30000+len(raw)-len(caller)))
        resident[source:source+len(raw)]=raw;caller[source-0x30000:source-0x30000+len(raw)]=raw
    for address in [0x1b2496,0x29d1fc,0x318bc4]:install(address,[0xc620])
    copy=[0x2e00,0x1f10]+[0x58f0|r for r in range(5,8)]+[0x5640,0xac01,0]
    done=len(copy)-1;loop=0x3000+len(copy)*2
    copy += [0x0862,0x0873,0x4c71,0x5c61,0x58f6,0x1f90,0x4d71,0x29a7,
             0x0860,0x0871,0x4c71,0x5c61,0x58f6,0x1f90,0x4d71,0x0b7a,
             0x5011,0x5100,0x5031,0x5120,0x5241,0xac02,loop]
    copy[done]=0x3000+len(copy)*2
    copy += [0x380f|(r<<4) for r in reversed(range(5,8))]+[0x1f90,0x2f00,0xc620]
    install(0x33000,copy)
    seek=[0x2e00,0x1f10,0x58f0,0x58f1,0x58f2,0x780b,0xa,0x781b,0xc,
          0x3f10,0xa8f0,0x1400,0x3f10,0xa8f1,0x1402,
          0x382f,0x381f,0x380f,0x1f90,0x2f00,0xc620]
    install(0x31000,seek)
    read=[0x2e00,0x1f10]+[0x58f0|r for r in range(7)]+[0x3f10,0x88f6,0x1404]
    joins=[]
    for index,(source,data) in enumerate(zip(sources,chunks)):
        read += [0x76f6,index,0xac02,0];branch=len(read)-1
        read += [0x68f4,len(data),0x68f2,source>>16,0x68f3,source&65535,0xac00,0];joins.append(len(read)-1)
        read[branch]=0x1100+len(read)*2
    read += [0x4840,0x4820,0x4830]
    for at in joins:read[at]=0x1100+len(read)*2
    read += [0x5061,0x3f10,0xa8f6,0x1404,0x780b,0xa,0x781b,0xc,
             0x58f4,0xbd03,0x3000,0x384f,
             0x3f10,0x88f0,0x1400,0x3f10,0x88f1,0x1402,0x1014,0x5100,
             0x3f10,0xa8f0,0x1400,0x3f10,0xa8f1,0x1402]
    read += [0x380f|(r<<4) for r in reversed(range(7))]+[0x1f90,0x2f00,0xc620]
    install(0x31100,read)
    failed=[0x2e00,0x1f10,0x58f0,0x3f10,0x88f0,0x1406,0x5001,
            0x3f10,0xa8f0,0x1406,0x380f,0x1f90,0x2f00,0xc620]
    install(0x33197a,failed);install(0x3401c,[0xad03,0x1000]);install(0x34010,[0xad03,0x1100])
    # Presentation adapter pauses after each original draw. Only a diagnostic
    # flag at internal4000 is changed by the host; original scene/draw stay intact.
    for address in [0x34224,0x3422c,0x34120,0x34398]:install(address,[0xc620])
    install(0x34124,[0x4800,0xc620])
    gate=[0x58f0,0x4800,0x3f10,0xa8f0,0x4000]
    gate_pc=0x3800+len(gate)*2
    gate += [0x3f10,0x88f0,0x4000,0x5600,0xac01,gate_pc,0x380f,0xc620]
    install(0x33800,gate);install(0x332ff2,[0xad03,0x3800])
    code=[0x6800,2,0x68ff,0x6000]
    for address in [0x510000,0x518000,0x520000,0x528000,0x540000]:
        count=0x3fff if address!=0x540000 else 7199
        code += [0x6810,address>>15,0x4810,0x4800,0xbf00,count,0x1a10]
    for address in range(0x1400,0x140a,2):code += [0x4800,0x3f10,0xa8f0,address]
    code += [0xbd30,0x8c1c,0xbd1b,0x23ce,0xbd29,0xd1c8]
    def word(address,value):code.extend([0x6810,address>>15,0x68f2,value,0xa8f2,address&0x7fff])
    def long(address,value):word(address,value>>16);word(address+2,value&65535)
    for address,value in [(0x5279be,sections[9][1]),(0x5279c2,sections[9][0]),
                          (0x5279c6,cfg.get('directory_size',sections[0][1])),
                          (0x5279ca,cfg.get('directory',sections[0][0])),(0x5279ce,cfg.get('payload',sections[1][0])),
                          (0x5279de,cfg.get('role_directory',sections[6][0])),(0x5279e2,cfg.get('role_payload',sections[7][0])),
                          (0x5279f6,cfg.get('scene_directory',sections[13][0])),(0x5279fa,cfg.get('scene_payload',sections[14][0]))]:long(address,value)
    word(0x527af8,cfg.get('portrait_base',vectors[6]));word(0x527aec,cfg.get('ui_base',vectors[7]))
    slots={5:0x527ae4,6:0x527af8,7:0x527aec,8:0x527aea}
    word(slots[5],vectors[5]);word(slots[8],vectors[8])
    if 'inventory_offset' in cfg:word(slots[cfg['image_group']],cfg.get('image_base',vectors[cfg['image_group']]))
    code += [0xbd31,0x8b80,0x6811,0,0x68f2,0x54,0xa8f2,0x84fc,0x4820,0xa8f2,0x84fe]
    if cfg['service']=='scene':
        values=[0,cfg['kind'],cfg['y'],cfg['x'],cfg['y'],cfg['x'],cfg['number']]
        call=[0xbd24,0xacba];pop=[0x70ff,14]
    elif cfg['service']=='role':
        values=[0,cfg['kind'],cfg['end_y'],cfg['end_x'],cfg['y'],cfg['x'],cfg['number']]
        call=[0xbd1f,0x43d4];pop=[0x70ff,14]
    elif cfg['service']=='battle':
        values=[cfg['kind'],cfg['number'],cfg['y'],cfg['x']];call=[0xbd28,0x8ed8];pop=[0x50f8]
    else:
        values=[cfg['kind'],cfg['y'],cfg['x'],cfg['number']];call=[0xbd2b,0];pop=[0x50f8]
    for value in values:code += [0x68f0,value&65535,0x58f0]
    code += call+pop+[0x3f10,0xa8f0,0x1408,0x68fc,0x600d]
    pc=len(code)*2;code.append(0xd0ff)
    assert len(code)*2<0x1000;install(0x30000,code)
    firmware=out/(cfg['name']+'.prg');rom=out/(cfg['name']+'.rom')
    firmware.write_bytes(caller);rom.write_bytes(resident);em=None
    try:
        em=Emulator(qemu=args.qemu,firmware=firmware,soc_rom=rom,soc_rom_skeleton=True,boot_mode='firmware-vma03',start_paused=True)
        for address,size in [(0x1f43d4,0x60e),(0x318756,0xae),(0x2c75d6,0x1f2),
                             (0x32bca6,0x50),(0x3252f2,0x80),(0x32d218,0x48),
                             (0x2f45e4,0x80),(0x2f52b4,0x80),(0x2faa44,0x10),(0x24acba,0x432),(0x2b0000,0x248),(0x288ed8,0x2f6),(0xd7c34,0x29a),(0xd7ece,0x364),(0xd8232,0x32e),
                             (0x1b23ce,0xc8),(0x29d1c8,0x34),(0x318b80,0x44),(0x31def6,0x96),
                             (0x314974,0xbe),(0x2577da,0x3e8),(0x315300,0xba),
                             (0x308c1c,0xf6),(0x308b26,0xf6),(0x303a56,0x10c),
                             (0x276100,0x34a),(0x313a72,0xc2)]:
            assert base64.b64decode(em.snapshot('code',address,size,diagnostics=False)['frame'])==prg[address-0x30000:address-0x30000+size]
        def memory(address,size):
            raw=bytearray()
            for offset in range(0,size,65536):raw.extend(base64.b64decode(em.snapshot('data',address+offset,min(65536,size-offset),diagnostics=False)['frame']))
            for i in range(0,len(raw),2):raw[i],raw[i+1]=raw[i+1],raw[i]
            return bytes(raw)
        for source,data in zip(sources,chunks):
            assert memory(source,len(data)+(len(data)&1))==data+bytes(len(data)&1),hex(source)
        captures=[];state='';em.qmp.execute('cont');deadline=time.monotonic()+60
        while time.monotonic()<deadline:
            state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=03:{pc:04x}'.lower() in state.lower():break
            # Gate is a short polling loop, so any PC within it is a pause.
            match=re.search(r'PC=03:([0-9a-f]{4})',state,re.I)
            if match and gate_pc<=int(match[1],16)<0x3800+len(gate)*2-4:
                em.qmp.execute('stop')
                frame=dict(index=len(captures))
                if cfg['service']=='role':
                    paused=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
                    fp=int(re.search(r'\bR11\s*=([0-9a-f]{4})',paused,re.I)[1],16)
                    xy=memory(fp+6,4);frame['x']=int.from_bytes(xy[:2],'big',signed=True);frame['y']=int.from_bytes(xy[2:],'big',signed=True)
                    parent=int.from_bytes(memory(fp,2),'big')
                    frame['role_fp']=parent
                    frame['slopes']=memory((parent-18)&65535,8).hex()
                packed=memory(0x540000,14400)
                pixels=bytes((packed[(119-y)*120+x//2]>>(0 if x&1 else 4))&15 for y in range(120) for x in range(240))
                for label,data in [('ram',memory(0x510000,131072)),('trace',memory(0x1400,8)),('pixels',pixels)]:
                    path=out/(cfg['name']+'-frame-%03d-'%len(captures)+label+'.bin');path.write_bytes(data)
                    frame[label]=path.as_posix();frame[label+'_sha256']=hashlib.sha256(data).hexdigest()
                captures.append(frame);em.write_data(0x4000,bytes([1,0]));em.qmp.execute('cont')
                deadline=time.monotonic()+60
            time.sleep(.01)
        em.qmp.execute('stop');(out/(cfg['name']+'.cpu.txt')).write_text(state,encoding='utf8')
        assert f'PC=03:{pc:04x}'.lower() in state.lower(),state[:1600]
        assert 'ROM_MISSING=none' in state and 'UNSUPPORTED=none' in state,state[:1600]
        regs={int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})','\n'.join(state.splitlines()[:5]),re.I)}
        assert regs[12]==0x600d and regs[15]==0x6000,regs
        packed=memory(0x540000,14400)
        pixels=bytes((packed[(119-y)*120+x//2]>>(0 if x&1 else 4))&15 for y in range(120) for x in range(240))
        row=dict(input=cfg,frames=captures,passed=True,stack_restored=True,heap_headers_seeded=False,draw_stubs=False)
        for label,data in [('ram',memory(0x510000,131072)),('trace',memory(0x1400,10)),('packed',packed),('pixels',pixels)]:
            path=out/(cfg['name']+'-'+label+'.bin');path.write_bytes(data)
            row[label]=path.as_posix();row[label+'_sha256']=hashlib.sha256(data).hexdigest()
        rows.append(row);print(cfg['name'],'original allocations, file loader and draw complete',flush=True)
    finally:
        if em:em.close()
report=dict(passed=True,cases=rows,original_prg_sha256=hashlib.sha256(prg).hexdigest(),npe_sha256=hashlib.sha256(npe).hexdigest(),
            scope='Original allocation prefixes, resource loader and actual drawing with file IO/prompt boundaries. Zero initialized diagnostic RAM; not full firmware startup, NAND, LCD transport or world execution. Role clear=0; original softfloat and coordinate rounding execute unchanged. Includes generic indexed service and its return values, with wrapped directory arithmetic and real-file short reads.')
(out/('report-'+args.case+'.json' if args.case else 'report.json')).write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
