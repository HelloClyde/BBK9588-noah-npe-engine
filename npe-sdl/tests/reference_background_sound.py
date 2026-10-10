"""Unchanged background/scene sound slices with original request arguments.

Playback and RNG are capture boundaries. Scene uses a diagnostic LINK frame
and returns before its animation continuation; no full animation/file proof.
"""
import argparse,base64,hashlib,json,re,struct,sys,time
from pathlib import Path
sys.path.insert(0,str(Path('scripts').resolve()));sys.path.insert(0,str(Path('scripts/tests').resolve()))
from web.np560_web import Emulator
from test_np560_dtu import write_word
p=argparse.ArgumentParser();p.add_argument('--qemu',type=Path,required=True);args=p.parse_args()
out=Path('tmp/background-sound-native');out.mkdir(exist_ok=True);original=Path('tmp/NP560.PRG').read_bytes()
inputs=[]
for slot in range(6):inputs.append((f'battle-slot-{slot}',1,1,slot*10,0))
for word in (0,1,2,0x100,0x101,0xffff):
    inputs.append((f'battle-word-{word}',1,word,139,0));inputs.append((f'replay-word-{word}',2,word,0,0))
for group,roll in ((1,0),(2,0),(2,1),(2,2)):inputs.append((f'scene-group-{group}-roll-{roll}',0,1,roll,group))
for word in (0,1,2,0x100,0x101,0xffff):
    for group in (1,2):inputs.append((f'scene-word-{word}-group-{group}',0,word,1,group))
assert len(inputs)==34;cases=[]
for name,phase,enabled,roll,group in inputs:
    initial={0x7a50:enabled,0x7996:0x8123,0x7998:0x4567,0x799a:0xaabb,0x799c:0xccdd}
    for count,size_address,offset_address in ((4,0x78c4,0x78d4),(7,0x7856,0x7872)):
        for i in range(count):initial.update({size_address+4*i:0x1122+i,size_address+4*i+2:0x3344+i,
                                             offset_address+4*i:0x5566+i,offset_address+4*i+2:0x7788+i})
    code=[0x6800,2,0x68ff,0x6000,0x6810,0]
    for address,value in ((0x1600,0),(0x1620,0)):
        write_word(code,address,value)
    for address in range(0x1602,0x161a,2):write_word(code,address,0xdead)
    for address,value in sorted(initial.items()):
        code+=[0x6810,0xa4+(address>>15)];write_word(code,address&0x7fff,value,banked=True)
    if phase==0:code+=[0x68f0,group,0x58f0,0xbd03,0x2000,0x50f2]
    else:code+=[0xbd30 if phase==1 else 0xbd32,0x058a if phase==1 else 0x649c]
    code+=[0x68fc,0x600d];pc=len(code)*2;code+=[0xd0ff]
    caller=bytearray(original);caller[:len(code)*2]=struct.pack('<%dH'%len(code),*code)
    play=[0x2e00,0x58f0,0x3f10,0x88f0,0x1600,0x5001,0x3f10,0xa8f0,0x1600]
    for i in range(12):play+=[0x780b,6+2*i,0x3f10,0xa8f0,0x1602+2*i]
    play+=[0x380f,0x2f00,0xc620]
    rng=[0x58f1,0x3f10,0x88f1,0x1620,0x5011,0x3f10,0xa8f1,0x1620,0x381f,0x68f0,roll,0xc620]
    # LINK reserves all scene locals; group is its original parameter+6.
    patches=[(0x438c,[0xad03,0x1800]),(0x4124,[0xad03,0x2100]),(0x1800,play),(0x2100,rng),
             (0x2000,[0x2e18,0xbd24,0xaefa,0x2f00,0xc620]),(0x21afac,[0xc620])]
    for address,body in patches:caller[address:address+len(body)*2]=struct.pack('<%dH'%len(body),*body)
    firmware=out/(name+'.prg');firmware.write_bytes(caller);em=None
    try:
        em=Emulator(qemu=args.qemu,firmware=firmware,boot_mode='firmware-vma03',start_paused=True)
        for cpu,start,end in ((0x30058a,0x2d058a,0x2d06a4),(0x32649c,0x2f649c,0x2f6508),(0x24aefa,0x21aefa,0x21afac)):
            assert base64.b64decode(em.snapshot('code',cpu,end-start,diagnostics=False)['frame'])==original[start:end]
        em.qmp.execute('cont');deadline=time.monotonic()+8;state=''
        while time.monotonic()<deadline:
            state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=03:{pc:04x}'.lower() in state.lower():break
            time.sleep(.01)
        em.qmp.execute('stop');(out/(name+'.cpu.txt')).write_text(state,encoding='utf8')
        assert f'PC=03:{pc:04x}'.lower() in state.lower() and all(k+'=none' in state for k in ('ROM_MISSING','UNSUPPORTED','INVALID_CONTROL')),state[:1500]
        regs={int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})','\n'.join(state.splitlines()[:5]),re.I)}
        assert regs[12]==0x600d and regs[15]==0x6000,regs
        def memory(a,n):return base64.b64decode(em.snapshot('data',a,n,diagnostics=False)['frame'])
        calls,*arguments=struct.unpack('<13H',memory(0x1600,26));rng_calls=struct.unpack('<H',memory(0x1620,2))[0]
        expected_calls=(enabled==1) if phase==0 else (enabled!=0)
        expected_rng=(group!=1) if phase==0 else (enabled!=0 and phase==1)
        assert calls==int(expected_calls) and rng_calls==int(expected_rng),(name,calls,rng_calls)
        ram=b''.join(memory(a,0x8000) for a in range(0x510000,0x530000,0x8000))
        guest=bytearray(ram);guest[0::2],guest[1::2]=ram[1::2],ram[0::2]
        (out/(name+'.ram.bin')).write_bytes(guest)
        row=dict(name=name,phase=phase,enabled=enabled,roll=roll,group=group,initial_words=sorted(initial.items()),
                 calls=calls,arguments=arguments,rng_calls=rng_calls,passed=True,stack_restored=True,host_guest_memory_writes=0)
        cases.append(row);print(json.dumps({k:row[k] for k in ('name','calls','rng_calls')}),flush=True)
    finally:
        if em:em.close()
(out/'report.json').write_text(json.dumps(dict(passed=len(cases)==34,cases=cases,
 original_prg_sha256=hashlib.sha256(original).hexdigest(),qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),
 scope='unchanged battle start/replay and scene request slice,RNG/playback capture;no full scene/file/codec/IRQ proof'),indent=2)+'\n',encoding='utf8')
