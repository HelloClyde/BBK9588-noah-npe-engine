"""Original CPU2F:E15A / PRG4C:E15A viewport copy; no IO/LCD adapters.

Entire original function remains unchanged. Initial RAM is diagnostic
input, including overlapping buffers and out-of-grid signed positions.
"""
import argparse,base64,hashlib,json,re,struct,sys,time
from pathlib import Path
sys.path.insert(0,str(Path('scripts').resolve()))
from web.np560_web import Emulator
p=argparse.ArgumentParser();p.add_argument('--qemu',type=Path,required=True);p.add_argument('--case')
args=p.parse_args();out=Path('tmp/map-viewport-cache-native');out.mkdir(exist_ok=True)
original=Path('tmp/NP560.PRG').read_bytes();cases=[]
def add(name,width,x,y,pointer=0x520000):
    cases.append(dict(name=name,width=width,x=x,y=y,pointer=pointer))
for width,x,y in [(0,241,0),(0,241,255),(0,0,0),(1,0,0),(1,243,251),
                  (14,255,255),(15,0,0),(15,7,4),(25,10,8),(25,255,255),
                  (128,0,255),(128,128,255),(255,0,127),(255,255,128),
                  (255,255,255),(255,0,255)]:
    add('w%d-x%d-y%d'%(width,x,y),width,x,y)
for name,pointer,width in [('same',0x5270b4,15),('backward',0x5270b2,15),
                           ('forward',0x5270b6,15),('row-overlap',0x5270b2,1)]:
    add('overlap-'+name,width,0,0,pointer)
rows=[]
for cfg in cases:
    if args.case and cfg['name']!=args.case:continue
    root=out/cfg['name'];root.mkdir(exist_ok=True)
    ram=bytearray().join(struct.pack('>H',(0x9e37+73*i)&65535) for i in range(65536))
    def word(a,v):ram[a-0x510000:a-0x510000+2]=struct.pack('>H',v)
    word(0x52797e,cfg['pointer']>>16);word(0x527980,cfg['pointer']&65535)
    for a,value in [(0x527a56,cfg['width']),(0x527a2e,cfg['x']),(0x527a2c,cfg['y'])]:
        ram[a-0x510000]=value
    caller=bytearray(original);code=[0x6800,2,0x68ff,0x6000,0x6810,0,0x6811,0,
                                    0xbd2f,0xe15a,0x68fc,0x600d]
    pc=len(code)*2;code+=[0xd0ff];caller[:len(code)*2]=struct.pack('<%dH'%len(code),*code)
    firmware=root/'caller.prg';firmware.write_bytes(caller);em=None
    try:
        em=Emulator(qemu=args.qemu,firmware=firmware,boot_mode='firmware-vma03',start_paused=True)
        observed=base64.b64decode(em.snapshot('code',0x2fe15a,0x126,diagnostics=False)['frame'])
        assert observed==original[0x2ce15a:0x2ce280]
        for i in range(0,len(ram),0x1000):
            data=bytearray(ram[i:i+0x1000]);data[0::2],data[1::2]=data[1::2],data[0::2]
            em.write_data(0x510000+i,data)
        def snapshot():
            result=bytearray()
            for a in range(0x510000,0x530000,0x8000):
                data=bytearray(base64.b64decode(em.snapshot('data',a,0x8000,diagnostics=False)['frame']))
                data[0::2],data[1::2]=data[1::2],data[0::2];result+=data
            return bytes(result)
        before=snapshot();assert before==ram
        (root/'before.bin').write_bytes(before)
        em.qmp.execute('cont');deadline=time.monotonic()+10
        while time.monotonic()<deadline:
            state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=03:{pc:04x}'.lower() in state.lower():break
            time.sleep(.001)
        em.qmp.execute('stop');(root/'cpu.txt').write_text(state,encoding='utf-8')
        assert f'PC=03:{pc:04x}'.lower() in state.lower(),state[:1600]
        assert all(k+'=none' in state for k in ['ROM_MISSING','UNSUPPORTED','INVALID_CONTROL']),state[:1600]
        assert 'r12=600d' in state.replace(' ','').lower() and 'r15=6000' in state.replace(' ','').lower()
        (root/'after.bin').write_bytes(snapshot());row=dict(**cfg,passed=True,stack_restored=True)
        (root/'report.json').write_text(json.dumps(row,indent=2)+'\n');rows.append(row);print(cfg['name'],flush=True)
    finally:
        if em:em.close()
(out/'report.json').write_text(json.dumps(dict(passed=True,cases=rows,
    original_region=dict(cpu=0x2fe15a,file=0x2ce15a,size=0x126),
    original_prg_sha256=hashlib.sha256(original).hexdigest(),qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),scope=__doc__),indent=2)+'\n')
