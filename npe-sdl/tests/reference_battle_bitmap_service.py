"""Original28:8ED8 plus heap services, with explicit RAM IO/draw boundaries.

The supplied bytes come from the current NPE. Guest setup and RAM adapters
replace NAND reads, bitmap decoding/drawing and the allocation-error prompt.
This certifies loader control flow and memory writes, not those boundaries.
"""
import argparse,base64,hashlib,json,re,struct,sys,time
from pathlib import Path
sys.path.insert(0,str(Path('scripts').resolve()))
from web.np560_web import Emulator

parser=argparse.ArgumentParser();parser.add_argument('--qemu',type=Path,required=True);parser.add_argument('--case')
args=parser.parse_args();out=Path('tmp/battle-bitmap-service-native');out.mkdir(exist_ok=True)
prg=Path('tmp/NP560.PRG').read_bytes();npe=Path('tmp/tjfy-world-probe.npe').read_bytes()
sections=[struct.unpack_from('>2I',npe,0x3a+8*i) for i in range(100)]
chain=json.loads(Path('tmp/battle-heap-chain-report.json').read_text(encoding='utf8'))['nodes']
inputs=[dict(name=name,number=number,warm=warm,kind=kind,first_cap=first,second_cap=second,payload_cap=payload,draw_handle=changed)
 for name,number,warm,kind,first,second,payload,changed in [
 ('fresh-514',514,False,0,3,3,65535,-1),('fresh-30',30,False,1,3,3,65535,-1),
 ('warm-34',34,True,2,3,3,65535,-1),('warm-31',31,True,255,3,3,65535,-1),
 ('short-payload',514,False,0,3,3,5,-1),('short-first-directory',514,False,1,1,3,65535,-1),
 ('zero-second-directory',514,False,2,3,0,65535,-1),('draw-global-handle',514,False,0,3,3,65535,255),
 ('resident-29',29,True,0,3,3,65535,-1),
 ('resident-0',0,True,1,3,3,65535,-1),('resident-1',1,True,2,3,3,65535,-1),
 ('file-displacement-wrap',10952,True,0,3,3,65535,-1),
 ('file-negative-offset',512,True,1,3,3,65535,-1)]]
if args.case:inputs=[c for c in inputs if c['name']==args.case]
assert inputs
rows=[]
for cfg in inputs:
    number=cfg['number'];directory_position=(sections[0][0]+(6*(number-29)&65535))&0xffffffff
    locals_=bytearray(5);chunks=[];cursor=directory_position
    for offset,cap in [(0,cfg['first_cap']),(2,cfg['second_cap'])]:
        chunk=npe[cursor:cursor+min(3,cap)];locals_[offset:offset+len(chunk)]=chunk;cursor+=len(chunk);chunks.append(chunk)
    relative=int.from_bytes(locals_[:2],'big',signed=True);count=int.from_bytes(locals_[2:4],'big')
    payload_position=(sections[1][0]+relative)&0xffffffff
    payload=npe[payload_position:payload_position+min(count,cfg['payload_cap'])];chunks.append(payload)
    # Three request-ordered RAM supplies. Guest byte lanes are high-first.
    assets=[(0x100000,chunks[0]),(0x100010,chunks[1]),(0x100100,chunks[2]),(0x10a000,npe[sections[9][0]:sum(sections[9])])]
    resident=bytearray(bytes(0x30000)+prg);caller=bytearray(prg)
    def install(address,words):
        data=struct.pack('<%dH'%len(words),*words)
        resident[address:address+len(data)]=data;caller[address-0x30000:address-0x30000+len(data)]=data
    def put_words(words,address,value):
        words.extend([0x6810,address>>15,0x68f2,value,0xa8f2,address&0x7fff])
    for address,data in assets:
        data=data+bytes(len(data)&1);raw=bytearray(data)
        for i in range(0,len(raw),2):raw[i],raw[i+1]=raw[i+1],raw[i]
        resident[address:address+len(raw)]=raw;caller[address-0x30000:address-0x30000+len(raw)]=raw
    # Shared byte copier: R0/R1 destination, R2/R3 source, R4 count.
    copy=[0x2e00,0x1f10]+[0x58f0|r for r in range(5,8)]+[0x5640,0xac01,0]
    done=len(copy)-1;loop=0x1800+len(copy)*2
    copy += [0x0862,0x0873,0x4c71,0x5c61,0x58f6,0x1f90,0x4d71,0x29a7,
             0x0860,0x0871,0x4c71,0x5c61,0x58f6,0x1f90,0x4d71,0x0b7a,
             0x5011,0x5100,0x5031,0x5120,0x5241,0xac02,loop]
    copy[done]=0x1800+len(copy)*2
    copy += [0x380f|(r<<4) for r in reversed(range(5,8))]+[0x1f90,0x2f00,0xc620]
    install(0x31800,copy)
    seek=[0x2e00,0x1f10,0x58f0,0x58f1,0x58f2,0x780b,0xa,0x781b,0xc,
          0x3f10,0xa8f0,0x1400,0x3f10,0xa8f1,0x1402,0x3f10,0x88f2,0x1406,
          0x5021,0x3f10,0xa8f2,0x1406]
    # Save both seek positions in fixed diagnostics, independent of pointer lanes.
    seek += [0x5621,0xac02,0];branch=len(seek)-1
    seek += [0x3f10,0xa8f0,0x1440,0x3f10,0xa8f1,0x1442,0xac00,0];join=len(seek)-1
    seek[branch]=0x1000+len(seek)*2;seek += [0x3f10,0xa8f0,0x1444,0x3f10,0xa8f1,0x1446]
    seek[join]=0x1000+len(seek)*2;seek += [0x382f,0x381f,0x380f,0x1f90,0x2f00,0xc620]
    install(0x31000,seek)
    read=[0x2e00,0x1f10]+[0x58f0|r for r in range(7)]+[0x3f10,0x88f6,0x1404]
    branches=[];joins=[]
    for index,source in enumerate([0x100000,0x100010,0x100100]):
        read += [0x5660|index,0xac02,0];branches.append(len(read)-1)
        read += [0x782b,0xe,0x3f10,0xa8f2,0x1410+2*index,
                 0x68f4,len(chunks[index]),0x3f10,0xa8f4,0x1416+2*index,
                 0x68f2,source>>16,0x68f3,source&65535,0xac00,0];joins.append(len(read)-1)
        read[branches[-1]]=0x1100+len(read)*2
    read += [0x4800,0x68f1,0] # Unused fourth-read guard path.
    for at in joins:read[at]=0x1100+len(read)*2
    read += [0x5061,0x3f10,0xa8f6,0x1404,0x780b,0xa,0x781b,0xc,
             0x3f10,0xa8f0,0x1430,0x3f10,0xa8f1,0x1432,0x58f4,0xbd03,0x1800,0x384f,
             0x3f10,0x88f0,0x1400,0x3f10,0x88f1,0x1402,0x1014,0x5100,
             0x3f10,0xa8f0,0x1400,0x3f10,0xa8f1,0x1402]
    read += [0x380f|(r<<4) for r in reversed(range(7))]+[0x1f90,0x2f00,0xc620]
    install(0x31100,read)
    for kind in range(3):
        draw=[0x2e00,0x1f10,0x58f0,0x58f1,
              0x780b,6,0x781b,8,0x3f10,0xa8f0,0x1420,0x3f10,0xa8f1,0x1422,
              0x780b,0xa,0x3f10,0xa8f0,0x1424,0x780b,0xc,0x3f10,0xa8f0,0x1426,
              0x68f0,kind,0x3f10,0xa8f0,0x1428,0x3f10,0x88f0,0x142a,0x5001,0x3f10,0xa8f0,0x142a]
        if cfg['draw_handle']>=0:draw += [0x6810,0xa4,0x68f0,cfg['draw_handle'],0xa8f0,0x7a88]
        draw += [0x381f,0x380f,0x1f90,0x2f00,0xc620]
        install(0x31400+0x100*kind,draw);install(0x342c0+4*kind,[0xad03,0x1400+0x100*kind])
    failed=[0x2e00,0x1f10,0x58f0,0x3f10,0x88f0,0x1450,0x5001,0x3f10,0xa8f0,0x1450,
            0x780b,8,0x3f10,0xa8f0,0x1452,0x380f,0x1f90,0x2f00,0xc620]
    install(0x31700,failed);install(0x33197a,[0xad03,0x1700])
    install(0x3401c,[0xad03,0x1000]);install(0x34010,[0xad03,0x1100])
    code=[0x6800,2,0x68ff,0x6000]
    for address in [0x510000,0x518000,0x520000,0x528000]:
        code += [0x6810,address>>15,0x4810,0x68f0,0xa5a5,0xbf00,0x3fff,0x1a10]
    code += [0xbd30,0x8c1c]
    if cfg['warm']:
        for node in chain:
            put_words(code,node['address'],node['flags']);put_words(code,node['address']+2,node['next']>>16);put_words(code,node['address']+4,node['next']&65535)
        put_words(code,0x5127ea,0xfe00)
    for address,value in [(0x5279a6,0x51),(0x5279a8,0x69bc),(0x5279ca,sections[0][0]>>16),
                          (0x5279cc,sections[0][0]&65535),(0x5279ce,sections[1][0]>>16),(0x5279d0,sections[1][0]&65535),(0x527a88,0)]:put_words(code,address,value)
    if number<=29:
        code += [0x68f0,0x51,0x68f1,0x69bc,0x68f2,0x10,0x68f3,0xa000,0x68f4,sections[9][1],0xbd03,0x1800]
    for address in range(0x1400,0x1454,2):code += [0x4800,0x3f10,0xa8f0,address]
    code += [0x68f0,cfg['kind'],0x58f0,0x68f0,number,0x58f0,0x68f0,0xfffd,0x58f0,0x68f0,7,0x58f0,
             0xbd28,0x8ed8,0x50f8,0x68fc,0x600d]
    pc=len(code)*2;code.append(0xd0ff);assert len(code)*2<0x1000
    install(0x30000,code)
    firmware=out/(cfg['name']+'.prg');rom=out/(cfg['name']+'.rom');firmware.write_bytes(caller);rom.write_bytes(resident)
    em=None
    try:
        em=Emulator(qemu=args.qemu,firmware=firmware,soc_rom=rom,soc_rom_skeleton=True,boot_mode='firmware-vma03',start_paused=True)
        for address,size in [(0x288ed8,0x2f6),(0x2577da,0x3e8),(0x315300,0xba),(0x308b26,0xf6),(0x303a56,0x10c),(0x276100,0x34a),(0x313a72,0xc2)]:
            raw=base64.b64decode(em.snapshot('code',address,size,diagnostics=False)['frame'])
            assert raw==prg[address-0x30000:address-0x30000+size],(cfg['name'],hex(address),'original service changed')
        em.qmp.execute('cont');deadline=time.monotonic()+10;state=''
        while time.monotonic()<deadline:
            state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=03:{pc:04x}'.lower() in state.lower():break
            time.sleep(.01)
        em.qmp.execute('stop');(out/(cfg['name']+'.cpu.txt')).write_text(state,encoding='utf8')
        assert f'PC=03:{pc:04x}'.lower() in state.lower(),state[:1800]
        assert 'ROM_MISSING=none' in state and 'UNSUPPORTED=none' in state,state[:1800]
        regs={int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})','\n'.join(state.splitlines()[:5]),re.I)}
        assert regs[12]==0x600d and regs[15]==0x6000,regs
        def memory(address,size):
            data=bytearray()
            for offset in range(0,size,65536):
                data.extend(base64.b64decode(em.snapshot('data',address+offset,min(65536,size-offset),diagnostics=False)['frame']))
            for i in range(0,len(data),2):data[i],data[i+1]=data[i+1],data[i]
            return bytes(data)
        ram=memory(0x510000,0x20000);trace=memory(0x1400,0x54)
        ram_path=out/(cfg['name']+'-ram.bin');trace_path=out/(cfg['name']+'-trace.bin');ram_path.write_bytes(ram);trace_path.write_bytes(trace)
        trace_words=struct.unpack('>42H',trace)
        row=dict(input=cfg,ram=ram_path.as_posix(),trace=trace_path.as_posix(),ram_sha256=hashlib.sha256(ram).hexdigest(),
                 trace_sha256=hashlib.sha256(trace).hexdigest(),directory_position=directory_position if number>29 else None,
                 payload_position=payload_position if number>29 else None,
                 request_count=trace_words[10],supplied_counts=list(trace_words[11:14]),draw_address=trace_words[16]*65536+trace_words[17],
                 reads=trace_words[2],seeks=trace_words[3],draw_kind=trace_words[20],failures=trace_words[40],
                 stack_restored=True,original_loader_and_heap=True,passed=True)
        rows.append(row);print(json.dumps({k:row[k] for k in ['input','draw_address','request_count','supplied_counts','reads','seeks','failures','passed']}),flush=True)
    finally:
        if em:em.close()
report=dict(passed=len(rows)==len(inputs),cases=rows,original_prg_sha256=hashlib.sha256(prg).hexdigest(),npe_sha256=hashlib.sha256(npe).hexdigest(),
            qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),heap_chain=chain,
            boundaries=['file seek','file read with NPE bytes in RAM','all three bitmap draw services','allocation failure prompt'],
            scope='original loader and heap effects, complete128KiB RAM and IO/draw metadata; no actual NAND, bitmap decode/LCD, or startup allocation ownership proof')
(out/('report-'+args.case+'.json' if args.case else 'report.json')).write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
