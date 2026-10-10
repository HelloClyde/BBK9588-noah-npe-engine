"""Original map replay/settings slices and unchanged4E:A3F4.

Only playback/stop and two slice-return boundaries are replaced. No codecs.
"""
import argparse,base64,hashlib,json,re,struct,sys,time
from pathlib import Path
sys.path.insert(0,str(Path('scripts').resolve()))
sys.path.insert(0,str(Path('scripts/tests').resolve()))
from web.np560_web import Emulator
from test_np560_dtu import write_word
parser=argparse.ArgumentParser();parser.add_argument('--qemu',type=Path,required=True)
args=parser.parse_args();out=Path('tmp/map-sound-native');out.mkdir(exist_ok=True)
original=Path('tmp/NP560.PRG').read_bytes();cases=[]
inputs=[]
for enabled in (0,1,2,0x100,0x101,0xffff):
    inputs += [(f'resume-{enabled}',0,enabled,1,2),(f'on-{enabled}',1,enabled,1,2),(f'off-{enabled}',2,enabled,1,2)]
for pending in (0,2,0x100,0xffff):inputs += [(f'pending-{pending}',0,1,pending,1)]
for slot in (0,1,3,255):inputs += [(f'slot-{slot}',0,1,1,slot)]
for enabled in (0,2,0x101):inputs += [(f'direct-{enabled}',3,enabled,0,1)]
for name,operation,enabled,pending,slot in inputs:
    initial={0x7a50:enabled,0x7ab4:pending,0x7a04:(slot<<8)|0x81,0x7a8a:(1 if operation==1 else 2)<<8|0x92,
             0x7934:0xa6b7}
    for i in range(3):
        initial.update({0x78f2+4*i:0x1122+i,0x78f4+4*i:0x3344+i,
                        0x78fe+4*i:0x5566+i,0x7900+4*i:0x7788+i})
    code=[0x6800,2,0x68ff,0x6000,0x6810,0,0x6811,0]
    for a,v in [(0x1600,0),(0x161a,0),(0x161c,0xdead),(0x161e,0xdead)]:write_word(code,a,v)
    for a in range(0x1602,0x161a,2):write_word(code,a,0xdead)
    for a,v in sorted(initial.items()):
        code += [0x6810,0xa4+(a>>15)];write_word(code,a&0x7fff,v,banked=True)
    entry=(0x1a,0x1324) if operation==0 else (0x31,0xa3f4) if operation==3 else (0x1a,0xcfde)
    code += [0xbd00|entry[0],entry[1],0x68fc,0x600d]
    pc=len(code)*2;code += [0xd0ff]
    caller=bytearray(original);caller[:len(code)*2]=struct.pack('<%dH'%len(code),*code)
    play=[0x2e00,0x58f0,0x3f10,0x88f0,0x1600,0x5001,0x3f10,0xa8f0,0x1600]
    for i in range(12):play += [0x780b,6+2*i,0x3f10,0xa8f0,0x1602+2*i]
    play += [0x6810,0xa4,0x88f0,0x7a50,0x3f10,0xa8f0,0x161c,0x380f,0x2f00,0xc620]
    stop=[0x58f0,0x3f10,0x88f0,0x161a,0x5001,0x3f10,0xa8f0,0x161a,
          0x6810,0xa4,0x88f0,0x7a50,0x3f10,0xa8f0,0x161e,0x380f,0xc620]
    # Adjacent vector entries use jumps to diagnostic bodies, not overlapping bodies.
    patches=[(0x438c,[0xad03,0x1800]),(0x4398,[0xad03,0x1900]),
             (0x1800,play),(0x1900,stop),(0x171344,[0xc620]),
             (0x17136e,[0xc620]),(0x17d57c,[0xc620])]
    for a,body in patches:caller[a:a+len(body)*2]=struct.pack('<%dH'%len(body),*body)
    firmware=out/(name+'.prg');firmware.write_bytes(caller);em=None
    try:
        em=Emulator(qemu=args.qemu,firmware=firmware,boot_mode='firmware-vma03',start_paused=True)
        for cpu,start,end in [(0x31a3f4,0x2ea3f4,0x2ea498),(0x1a1324,0x171324,0x171344),(0x1acfde,0x17cfde,0x17d02c)]:
            raw=base64.b64decode(em.snapshot('code',cpu,end-start,diagnostics=False)['frame'])
            assert raw==original[start:end]
        em.qmp.execute('cont');deadline=time.monotonic()+8;state=''
        while time.monotonic()<deadline:
            state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=03:{pc:04x}'.lower() in state.lower():break
            time.sleep(.01)
        em.qmp.execute('stop');(out/(name+'.cpu.txt')).write_text(state,encoding='utf8')
        assert f'PC=03:{pc:04x}'.lower() in state.lower() and all(k+'=none' in state for k in ('ROM_MISSING','UNSUPPORTED','INVALID_CONTROL')),state[:1500]
        regs={int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})','\n'.join(state.splitlines()[:5]),re.I)}
        assert regs[12]==0x600d and regs[15]==0x6000
        def memory(a,n):return base64.b64decode(em.snapshot('data',a,n,diagnostics=False)['frame'])
        calls,*captured=struct.unpack('<16H',memory(0x1600,32))
        ram=b''.join(memory(a,0x8000) for a in range(0x510000,0x530000,0x8000))
        guest=bytearray(ram);guest[0::2],guest[1::2]=ram[1::2],ram[0::2]
        (out/(name+'.ram.bin')).write_bytes(guest)
        row=dict(name=name,operation=operation,enabled=enabled,pending=pending,slot=slot,initial_words=sorted(initial.items()),
                 calls=calls,arguments=captured[:12],stops=captured[12],play_observed_word=captured[13],stop_observed_word=captured[14],
                 after_enabled=struct.unpack_from('>H',guest,0x17a50)[0],passed=True,stack_restored=True,host_guest_memory_writes=0)
        cases.append(row);print(json.dumps({k:row[k] for k in ('name','calls','stops','play_observed_word','stop_observed_word','after_enabled')}),flush=True)
    finally:
        if em:em.close()
report=dict(passed=len(cases)==len(inputs),cases=cases,original_prg_sha256=hashlib.sha256(original).hexdigest(),
            qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),
            scope='unchanged map replay service/caller gates/settings slices with playback and stop capture boundaries; no full UI/file/codec/IRQ proof')
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
