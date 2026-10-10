"""Unmodified25:3082 type2 and real2F:E15A; complete128KiB RAM.
Diagnostic initial RAM only. No IO/LCD or physical timing equivalence.
"""
import argparse,base64,hashlib,json,struct,sys,time
from pathlib import Path
sys.path.insert(0,str(Path('scripts').resolve()))
from web.np560_web import Emulator
p=argparse.ArgumentParser();p.add_argument('--qemu',type=Path,required=True);p.add_argument('--case');p.add_argument('--banks',action='store_true');p.add_argument('--seed-directory',type=Path);p.add_argument('--output-directory',type=Path,default=Path('tmp/map-tile-update-native'));args=p.parse_args()
out=args.output_directory;out.mkdir(exist_ok=True);original=Path('tmp/NP560.PRG').read_bytes();cases=[]
def slot(threshold=0,pairs=(),flag=0):return dict(threshold=threshold,pairs=list(pairs),flag=flag)
def add(name,slots,progress=100):cases.append(dict(name=name,slots=slots,progress=progress))
add('empty',[]);add('zero-threshold',[slot(0,[(1,0x1234)])]);add('below',[slot(101,[(1,0x1234)])])
add('equal',[slot(100,[(1,0x1234)])]);add('unsigned-progress',[slot(0xffff,[(1,0x1234)])],0xffff)
for flag in [1,2,255]:add('flag-'+str(flag),[slot(1,[(1,0x1234)],flag)])
for index in [0,0xffff,0x8000]:add('stop-'+str(index),[slot(1,[(index,0x1234),(1,0x2222)])])
add('zero-mid',[slot(1,[(1,0x1111),(0,0x2222),(2,0x3333)])])
add('negative-mid',[slot(1,[(1,0x1111),(0xffff,0x2222),(2,0x3333)])])
add('full15',[slot(1,[(i+1,0x1200+i) for i in range(15)])])
add('three-slots',[slot(100,[(1,0x1111)]),slot(101,[(2,0x2222)]),slot(100,[(3,0x3333)])])
add('signed-max',[slot(1,[(0x7fff,0x4444)])])
add('alias-progress',[slot(100,[(0x3d33,0),(1,0x2222)]),slot(100,[(2,0x3333)])])
add('alias-threshold',[slot(1,[(0x3927,0xffff),(1,0x2222)])])
add('alias-next-threshold',[slot(1,[(0x3947,0xffff)]),slot(1,[(1,0x2222)])])
add('alias-next-index',[slot(1,[(0x3929,0),(1,0x2222)])])
add('alias-next-tile',[slot(1,[(0x3938,0x4444),(1,0x2222)])])
add('alias-flag',[slot(1,[(0x3946,0xff99),(1,0x2222)])])
add('alias-pointer',[slot(1,[(0x3cc0,0x4000),(1,0x2222)])])
if args.banks:
    cases=[]
    for name,address,tile in [
        ('battle-hp',0x5264a0,0x1111),('map-hp',0x526a16,0x2222),
        ('battle-id',0x52645c,0xabcd),('map-id',0x5269d2,0xabcd),
        ('battle-attack',0x5264ac,0x0090),('map-equipment',0x5269d8,0x3040),
        ('map-tail-active',0x526b18,0),('battle-prefix',0x5267c6,0xaaaa),
        ('map-name',0x5269c6,0xccdd),('battle-name',0x526450,0xeeff)]:
        add(name,[slot(100,[((address-0x520000)//2,tile)])])
rows=[]
for cfg in cases:
    if args.case and cfg['name']!=args.case:continue
    root=out/cfg['name'];root.mkdir(exist_ok=True)
    ram=bytearray().join(struct.pack('>H',(0x9e37+73*i)&65535) for i in range(65536))
    def word(a,v):ram[a-0x510000:a-0x510000+2]=struct.pack('>H',v)
    word(0x52797e,0x52);word(0x527980,0);word(0x527a66,cfg['progress'])
    word(0x527a56,0x0f91);word(0x527a2c,0x0092);word(0x527a2e,0x0093)
    for i in range(3):
        row=cfg['slots'][i] if i<len(cfg['slots']) else slot();a=0x52724e+64*i
        ram[a-0x510000:a-0x510000+64]=bytes(64);word(a,row['threshold']);word(a+62,row['flag']*256+0x99)
        for j,(index,tile) in enumerate(row['pairs']):word(a+2+2*j,index);word(a+32+2*j,tile)
    if args.seed_directory:
        ram=bytearray((args.seed_directory/(cfg['name']+'.bin')).read_bytes());assert len(ram)==131072
    caller=bytearray(original);code=[0x6800,2,0x68ff,0x6000,0x6810,0,0x6811,0,
        0x68f0,0xffff,0x58f0,0x4802,0x58f0,0xbd25,0x3082,0x50f4,0x68fc,0x600d]
    pc=len(code)*2;code+=[0xd0ff];caller[:len(code)*2]=struct.pack('<%dH'%len(code),*code)
    firmware=root/'caller.prg';firmware.write_bytes(caller);em=None
    try:
        em=Emulator(qemu=args.qemu,firmware=firmware,boot_mode='firmware-vma03',start_paused=True)
        for cpu,file,size in [(0x253082,0x223082,0x400),(0x2fe15a,0x2ce15a,0x126)]:
            actual=base64.b64decode(em.snapshot('code',cpu,size,diagnostics=False)['frame'])
            assert actual==original[file:file+size]
        for i in range(0,len(ram),0x1000):
            data=bytearray(ram[i:i+0x1000]);data[0::2],data[1::2]=data[1::2],data[0::2];em.write_data(0x510000+i,data)
        def snapshot():
            result=bytearray()
            for a in range(0x510000,0x530000,0x8000):
                data=bytearray(base64.b64decode(em.snapshot('data',a,0x8000,diagnostics=False)['frame']))
                data[0::2],data[1::2]=data[1::2],data[0::2];result+=data
            return bytes(result)
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
        (root/'after.bin').write_bytes(snapshot());row=dict(**cfg,passed=True,stack_restored=True)
        (root/'report.json').write_text(json.dumps(row,indent=2)+'\n',encoding='utf-8');rows.append(row);print(cfg['name'],flush=True)
    finally:
        if em:em.close()
(out/'report.json').write_text(json.dumps(dict(passed=True,cases=rows,original_regions=[
    dict(cpu=0x253082,file=0x223082,size=0x400),dict(cpu=0x2fe15a,file=0x2ce15a,size=0x126)],
    original_prg_sha256=hashlib.sha256(original).hexdigest(),qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),scope=__doc__),indent=2)+'\n',encoding='utf-8')
