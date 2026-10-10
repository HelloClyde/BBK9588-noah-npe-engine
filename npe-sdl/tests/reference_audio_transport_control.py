"""Run original405A/4070 and3994/40C0 using explicit transport boundaries.

24:98FC/9AB8/9B02 and16:EDFA are recording adapters. No physical DTU,
codec, filesystem or IRQ timing is certified by these software paths.
"""
import argparse,base64,hashlib,json,re,struct,sys,time
from pathlib import Path
sys.path.insert(0,str(Path('scripts').resolve()))
sys.path.insert(0,str(Path('scripts/tests').resolve()))
from web.np560_web import Emulator
from test_np560_dtu import write_word,read_word,read_sfr
parser=argparse.ArgumentParser();parser.add_argument('--qemu',type=Path,required=True)
args=parser.parse_args();out=Path('tmp/audio-transport-control-native-v2');out.mkdir(exist_ok=True)
original=Path('tmp/NP560.PRG').read_bytes();image=bytearray(0x30000)+original
inputs=[]
for mode in [0,0x10,0xffff]:
    inputs.append((f'suspend-mode-{mode:04x}',0,0x1070,3,mode,0,0x8320,0x8340,-1,-1,0x8000))
for eof in [0,0x800]:
    for busy in range(4):
        inputs.append((f'resume-busy-{eof:03x}-{busy}',1,0x1410|eof|busy,3,0,0,0x8000,0x8000,2,-1,0))
for left,right in [(0x8000,0x8000),(0,0x8000),(0x8000,0),(0x8001,0x8000),(0x8000,0x8001),(0xffff,0xffff)]:
    for starved in [0,0x40]:
        inputs.append((f'counts-{left:04x}-{right:04x}-{starved:02x}',1,0x1410,3|starved,0,0,left,right,2,-1,0))
for name,flags89,flags8e in [('no-registration',0x410,0),('no-output-request',0x1400,0),('demand-active',0x1410,2)]:
    inputs.append((name,1,flags89,3,0,flags8e,0x8000,0x8000,2,-1,0))
for after in [0,2,0xffff]:
    inputs.append((f'live-refill-{after:04x}',1,0x1410,3,0,0,0x8000,0x8000,after,-1,0x8000))
for before,after in [(3,0x43),(0x43,3)]:
    inputs.append((f'live-starved-{before:02x}-{after:02x}',1,0x1400,before,0,0,0x8000,0x8000,-1,after,0))
for mask in range(32):
    flags89=0x4040|(4 if mask&1 else 0)
    if mask&2:flags89&=~0x4000
    if mask&4:flags89&=~0x40
    guard=(1 if mask&8 else 0)|(4 if mask&16 else 0)
    inputs.append((f'poll-mask-{mask:02d}',2,flags89,3,guard,0,0x8000,0x8000,-1,-1,0x10))
for name,flags89,flags8a in [('poll-starved',0x4040,0x43),('poll-resume',0x4440,3),('poll-eof',0x4840,3)]:
    inputs.append((name,2,flags89,flags8a,0,0,0x8000,0x8000,-1,-1,0x10))
cases=[]
for name,operation,before89,before8a,mode,before8e,left,right,refill8e,resume8a,callback_or in inputs:
    # For polling, mode is the file/SPI guard word; other modes are88.
    flags8d=mode if operation==2 else 0
    prepared=bytearray(image)
    patches=[(0x16edfa,0x1200),(0x249ab8,0x1400),(0x2498fc,0x1600),(0x249b02,0x1800)]
    patches += [(0x83c22,0x1c00),(0x83d70,0x1d00)] if operation!=2 else [(0x84070,0x1e00)]
    for address,target in patches:prepared[address:address+4]=struct.pack('<2H',0xad20,target)
    prepared[0x100d0:0x100d4]=struct.pack('<2H',0xad08 if operation==2 else 0xad20,0x40c0 if operation==2 else 0x1000)
    rom=out/(name+'.rom');rom.write_bytes(prepared)
    code=[0x6800,2,0x68ff,0x6000,0x6810,0,0x6811,0]
    for address,value in [(0xf010,mode if operation!=2 else 0),(0xf012,before89),(0xf014,before8a),
                          (0xf01a,flags8d),(0xf01c,before8e),(0x1300,0),(0x13a0,0),
                          (0x4aa4,0x20),(0x4aa6,0x1a00)]:write_word(code,address,value)
    for slot in range(5):
        for delta in range(0,10,2):write_word(code,0x1310+slot*10+delta,0)
    code += [0x6810,0xa2];write_word(code,0x2ae0,0x1234,banked=True)
    code += [0x6810,0,0x6811,0,0x6821,left,0x6822,right,
             0x68f1,0x1357,0x68f2,0x2468,0x68f3,0x369c,0x68f7,0x789a]
    code += [0xbd08,0x3994] if operation==2 else [0x0f34]
    read_sfr(code,0x21,4);read_sfr(code,0x22,5)
    code += [0x68fc,0x600d];pc=len(code)*2;code += [0xd0ff]
    caller=bytearray(prepared[0x200000:]);caller[:len(code)*2]=struct.pack('<%dH'%len(code),*code)
    bodies={}
    if operation!=2:
        bodies[0x1000]=[0x1f09,0x1f10,0x58f0,0x6810,0,0xad08,0x405a if operation==0 else 0x4070]
    for slot,offset in enumerate([0x1200,0x1400,0x1600,0x1800,0x1a00]):
        body=[0x1f10,0x58f0,0x58f1,0x6810,0]
        for delta,address in [(0,0xf012),(2,0xf014),(4,0xf01c)]:
            read_word(body,address,0);body += [0x3f10,0xa8f0,0x1310+slot*10+delta]
        body += [0x3f10,0x88f0,0x1300,0x4c03,0x75f0,slot+1,0x3f10,0xa8f0,0x1300]
        read_word(body,0x1318+slot*10,0);body += [0x5001,0x3f10,0xa8f0,0x1318+slot*10]
        body += [0x3f10,0x88f0,0xf012,0x75f0,callback_or,0x3f10,0xa8f0,0xf012]
        if slot==2 and refill8e>=0:write_word(body,0xf01c,refill8e)
        if slot==3 and resume8a>=0:write_word(body,0xf014,resume8a)
        body += [0x381f,0x380f,0x1f90,0xc620];bodies[offset]=body
    for offset,route in [(0x1c00,1),(0x1d00,2),(0x1e00,3)]:
        body=[];write_word(body,0x13a0,route)
        body += [0x380f,0x1f90,0x1f89,0xc610];bodies[offset]=body
    for offset,body in bodies.items():caller[offset:offset+len(body)*2]=struct.pack('<%dH'%len(body),*body)
    firmware=out/(name+'.prg');firmware.write_bytes(caller)
    em=None
    try:
        em=Emulator(qemu=args.qemu,firmware=firmware,soc_rom=rom,soc_rom_skeleton=True,start_paused=True,icount_shift=0)
        spans=[(0x8405a,0x840c0)] if operation!=2 else [(0x83994,0x839b6),(0x840c0,0x840cc),(0x83ef4,0x84052)]
        for start,end in spans:
            assert base64.b64decode(em.snapshot('code',start,end-start,diagnostics=False)['frame'])==image[start:end]
        for offset,body in bodies.items():
            assert base64.b64decode(em.snapshot('code',0x200000+offset,len(body)*2,diagnostics=False)['frame'])==struct.pack('<%dH'%len(body),*body)
        em.qmp.execute('cont');deadline=time.monotonic()+10;state=''
        while time.monotonic()<deadline:
            state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=20:{pc:04x}'.lower() in state.lower():break
            time.sleep(.01)
        em.qmp.execute('stop');(out/(name+'.cpu.txt')).write_text(state,encoding='utf8')
        assert f'PC=20:{pc:04x}'.lower() in state.lower(),state[:1200]
        assert 'ROM_MISSING=none' in state and 'UNSUPPORTED=none' in state,state[:1200]
        regs={int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})','\n'.join(state.splitlines()[:5]),re.I)}
        assert [regs[r] for r in [1,2,3,7,12,15]]==[0x1357,0x2468,0x369c,0x789a,0x600d,0x6000],regs
        def memory(address,count):return base64.b64decode(em.snapshot('data',address,count,diagnostics=False)['frame'])
        raw=memory(0x1300,0xa2);flags=memory(0xf010,14)
        (out/(name+'.trace.bin')).write_bytes(raw);(out/(name+'.flags.bin')).write_bytes(flags)
        word=lambda address:struct.unpack('<H',raw[address-0x1300:address-0x1300+2])[0]
        entries=[list(struct.unpack('<5H',raw[0x10+i*10:0x1a+i*10])) for i in range(5)]
        after=struct.unpack('<7H',flags)
        row=dict(name=name,operation=operation,before89=before89,before8a=before8a,flags88=mode if operation!=2 else 0,
                 flags8d=flags8d,before8e=before8e,controls=[left,right],refill8e=refill8e,resume8a=resume8a,
                 callback_or=callback_or,entries=entries,trace=word(0x1300),route=word(0x13a0),
                 after89=after[1],after8a=after[2],after8e=after[6],after_controls=[regs[4],regs[5]],
                 file_status=struct.unpack('<H',memory(0x512ae0,2))[0],foreground_restored=True,host_guest_memory_writes=0,passed=True)
        assert row['file_status']==0x1234
        cases.append(row);print(name,[e[4] for e in entries],row['route'],flush=True)
    finally:
        if em:em.close()
report=dict(passed=len(cases)==len(inputs),cases=cases,original_prg_sha256=hashlib.sha256(original).hexdigest(),
            qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),
            scope='original suspend/resume controller and foreground producer dispatch/return; transport/adjust calls captured boundaries, no physical DTU/codec/file/IRQ timing')
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
