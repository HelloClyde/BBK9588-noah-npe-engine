"""Unmodified original25:3082 type1, full128KiB RAM. No physical timing claim."""
import argparse,base64,hashlib,json,struct,sys,time
from pathlib import Path
sys.path.insert(0,str(Path('scripts').resolve()))
from web.np560_web import Emulator
p=argparse.ArgumentParser();p.add_argument('--qemu',type=Path,required=True);p.add_argument('--case');args=p.parse_args()
out=Path('tmp/map-object-clear-native');out.mkdir(exist_ok=True)
original=Path('tmp/NP560.PRG').read_bytes();cases=[]
def add(name,objects=(),flags=(),width=32,slot=0xffff,extra=()):
    cases.append(dict(name=name,objects=list(objects),flags=list(flags),width=width,slot=slot,extra=list(extra)))
add('empty');add('id100',[(100,26,20)],[(100,1)])
add('flag2',[(6,1,1)],[(6,2)]);add('flag255',[(6,1,1)],[(6,255)])
add('id101',[(101,1,1)],[(101,1)])
add('duplicates',[(6,1,1),(0,0,0),(6,2,2)],[(6,1)])
add('zero-hole',[(0,0,0),(6,1,1)],[(6,1)])
add('alias-flags',[(2,2,1),(1,70,148)],[(1,1),(2,1)],100)
add('alias-width',[(2,3,2),(1,59,156)],[(1,1),(2,1)],100)
add('alias-coordinates',[(2,9,9),(1,58,150)],[(1,1),(2,1)],100)
add('alias-pointer-low',[(2,2,0),(1,252,60)],[(1,1),(2,1)],255)
add('alias-id',[(2,50,1),(1,55,150)],[(1,1),(2,1),(53,1)],100)
add('signed-index',[(6,255,255)],[(6,1)],255)
add('negative-slot',[(6,1,1)],[(6,1)],slot=0x8000)
add('single-zero-id',[(0,7,8)],slot=0)
add('single-slot8',slot=8,extra=[(0x5275e4,bytes([9,10]))])
add('single-slot255',slot=255,extra=[(0x527d9c,bytes([9,10]))])
rows=[]
for cfg in cases:
    if args.case and cfg['name']!=args.case:continue
    root=out/cfg['name'];root.mkdir(exist_ok=True)
    ram=bytearray().join(struct.pack('>H',(0x9e37+73*i)&65535) for i in range(65536))
    def put(a,data):ram[a-0x510000:a-0x510000+len(data)]=data
    def word(a,v):put(a,struct.pack('>H',v))
    word(0x52797e,0x52);word(0x527980,0);word(0x527a56,cfg['width']*256+0x91)
    put(0x52742c,bytes(200));put(0x52759e,bytes(64))
    for id,flag in cfg['flags']:put(0x52742c+id-1,bytes([flag]))
    for i,(id,x,y) in enumerate(cfg['objects']):
        word(0x52759e+8*i,id);put(0x5275a4+8*i,bytes([x,y]))
    for a,data in cfg['extra']:put(a,data)
    caller=bytearray(original);code=[0x6800,2,0x68ff,0x6000,0x6810,0,0x6811,0,
        0x68f0,cfg['slot'],0x58f0,0x4801,0x58f0,0xbd25,0x3082,0x50f4,0x68fc,0x600d]
    pc=len(code)*2;code+=[0xd0ff];caller[:len(code)*2]=struct.pack('<%dH'%len(code),*code)
    firmware=root/'caller.prg';firmware.write_bytes(caller);em=None
    try:
        em=Emulator(qemu=args.qemu,firmware=firmware,boot_mode='firmware-vma03',start_paused=True)
        actual=base64.b64decode(em.snapshot('code',0x253082,0x400,diagnostics=False)['frame'])
        assert actual==original[0x223082:0x223482]
        for i in range(0,len(ram),0x1000):
            data=bytearray(ram[i:i+0x1000]);data[0::2],data[1::2]=data[1::2],data[0::2];em.write_data(0x510000+i,data)
        def snapshot():
            data=bytearray()
            for a in range(0x510000,0x530000,0x8000):
                block=bytearray(base64.b64decode(em.snapshot('data',a,0x8000,diagnostics=False)['frame']))
                block[0::2],block[1::2]=block[1::2],block[0::2];data+=block
            return bytes(data)
        before=snapshot();assert before==ram;(root/'before.bin').write_bytes(before)
        em.qmp.execute('cont');deadline=time.monotonic()+10
        while time.monotonic()<deadline:
            state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=03:{pc:04x}'.lower() in state.lower():break
            time.sleep(.001)
        em.qmp.execute('stop');(root/'cpu.txt').write_text(state,encoding='utf-8')
        assert f'PC=03:{pc:04x}'.lower() in state.lower(),state[:1400]
        assert all(k+'=none' in state for k in ['ROM_MISSING','UNSUPPORTED','INVALID_CONTROL']),state[:1400]
        assert 'r12=600d' in state.replace(' ','').lower() and 'r15=6000' in state.replace(' ','').lower()
        after=snapshot();(root/'after.bin').write_bytes(after)
        clean={k:v for k,v in cfg.items() if k!='extra'}
        row=dict(**clean,passed=True,changed_bytes=sum(a!=b for a,b in zip(before,after)))
        (root/'report.json').write_text(json.dumps(row,indent=2)+'\n',encoding='utf-8')
        rows.append(row);print(cfg['name'],row['changed_bytes'],flush=True)
    finally:
        if em:em.close()
(out/'report.json').write_text(json.dumps(dict(passed=True,cases=rows,original_prg_sha256=hashlib.sha256(original).hexdigest(),
    qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),original_region=dict(cpu=0x253082,file=0x223082,size=0x400)),indent=2)+'\n',encoding='utf-8')
