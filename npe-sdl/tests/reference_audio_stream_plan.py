"""Execute original26:8D2A around controlled B816/B7AA diagnostic boundaries.

The original three probes and all range arithmetic run unchanged. Positions
and sample returns are supplied by guest adapters, not an original codec.
"""
import argparse,base64,hashlib,json,re,struct,sys,time
from pathlib import Path
sys.path.insert(0,str(Path('scripts').resolve()))
sys.path.insert(0,str(Path('scripts/tests').resolve()))
from web.np560_web import Emulator
from test_np560_dtu import write_word
parser=argparse.ArgumentParser();parser.add_argument('--qemu',type=Path,required=True)
args=parser.parse_args();out=Path('tmp/audio-stream-plan-native');out.mkdir(exist_ok=True)
original=Path('tmp/NP560.PRG').read_bytes();image=bytearray(0x30000)+original
# All original arithmetic stays present. Only the missing codec boundaries
# are diagnostic jumps; install their adapters on both possible code backs.
position=[0x2e00,0x1f10,0x58f2,0x58f3,0x58f4,0x782b,8,0x6810,0,
          0x3f10,0x88f3,0x16a2,0x3803,0x3813,0x3843,0x0a24,
          0x3f10,0xa8f3,0x16a2,0x3f10,0x88f2,0x16a0,0x5021,0x3f10,0xa8f2,0x16a0,
          0x384f,0x383f,0x382f,0x1f90,0x2f00,0xc620]
decode=[0x1f10,0x58f2,0x58f3,0x6810,0,0x3f10,0x88f3,0x16a6,0x3803,
        0x3f10,0xa8f3,0x16a6,0x3f10,0x88f2,0x16a4,0x5021,0x3f10,0xa8f2,0x16a4,
        0x383f,0x382f,0x1f90,0xc620]
resident=bytearray(image)
for address,entry in [(0x1b816,0x1000),(0x1b7aa,0x1100)]:resident[address:address+4]=struct.pack('<2H',0xad03,entry)
for entry,body in [(0x1000,position),(0x1100,decode)]:resident[0x30000+entry:0x30000+entry+len(body)*2]=struct.pack('<%dH'%len(body),*body)
rom=out/'original-with-codec-boundaries.rom';rom.write_bytes(resident)
npe=Path('tmp/tjfy-world-probe.npe').read_bytes();inputs=[]
for section in range(16,38):
    offset,size=struct.unpack_from('>2I',npe,0x3a+section*8);count=int.from_bytes(npe[offset+2:offset+4],'big')
    inputs.append((f'npe-section-{section}',count,8400,32,[168]*3,[160]*3,0,0,True))
for name,count,rate,header,steps,samples,start,length in [
    ('one-ms',1332,8400,32,[168]*3,[160]*3,1,1),
    ('exact-frame',1332,8400,32,[168]*3,[160]*3,20,20),
    ('partial-frame',1332,8400,32,[168]*3,[160]*3,39,39),
    ('at-end',1332,8400,32,[168]*3,[160]*3,26640,0),
    ('past-end',1332,8400,32,[168]*3,[160]*3,26680,0),
    ('range-clamp',100,8400,32,[168]*3,[160]*3,100,9999),
    ('odd-rate',100,8417,37,[167,169,173],[159,161,163],45,47),
    ('changed-step',100,8400,32,[160,184,168],[160,0,160],0,0),
    ('wide-step',100,8400,32,[0x100a8]*3,[160]*3,0,0),
    ('large-start',100,8400,32,[168]*3,[160]*3,0xffffffff,20),
    ('large-range',100,8400,32,[168]*3,[160]*3,0,0xffffffff),
    ('large-count',65535,8400,32,[168]*3,[160]*3,100,0),
    ('small-count',1,8400,32,[168]*3,[160]*3,0,0),
    ('zero-count',0,8400,32,[168]*3,[160]*3,0,0),
]:inputs.append((name,count,rate,header,steps,samples,start,length,False))
cases=[]
for name,count,rate,header,steps,samples,start,length,real in inputs:
    bits=header
    def cursor(value):return (0x8800+(value//16)*2,value%16)
    positions=[cursor(bits)]
    for step in steps:
        positions.append(cursor(bits));bits+=step;positions.append(cursor(bits))
    code=[0x6800,2,0x68ff,0x6000,0x6810,0,0x6811,0]
    for address,value in [(0x16a0,0),(0x16a2,0x1800),(0x16a4,0),(0x16a6,0x1840),
                          (0x1700,start>>16),(0x1702,start&65535),(0x1704,length>>16),(0x1706,length&65535)]:write_word(code,address,value)
    for i,(pointer,remaining) in enumerate(positions):
        for j,value in enumerate([pointer>>16,pointer&65535,remaining]):write_word(code,0x1800+i*6+j*2,value)
    for i,value in enumerate(samples):write_word(code,0x1840+i*2,value)
    code += [0x6810,0xa2]
    for address,value in [(0x282c,count),(0x283a,rate),(0x29d0,0),(0x29d2,0),(0x2838,0x5aa5),
                          (0x283e,0x5aa5),(0x2840,0x5aa5),(0x2842,0x5aa5),(0x2844,0x5aa5),(0x2afa,0x5aa5)]:write_word(code,address,value,banked=True)
    code += [0x68f0,0x1704,0x58f0,0x4800,0x58f0,0x68f0,0x1700,0x58f0,0x4800,0x58f0,
             0xbd26,0x8d2a,0x50f8,0x68fc,0x600d]
    pc=len(code)*2;code += [0xd0ff]
    caller=bytearray(original);caller[:len(code)*2]=struct.pack('<%dH'%len(code),*code)
    for entry,body in [(0x1000,position),(0x1100,decode)]:caller[entry:entry+len(body)*2]=struct.pack('<%dH'%len(body),*body)
    firmware=out/(name+'.prg');firmware.write_bytes(caller);em=None
    try:
        em=Emulator(qemu=args.qemu,firmware=firmware,soc_rom=rom,soc_rom_skeleton=True,boot_mode='firmware-vma03',start_paused=True)
        for address,entry in [(0x1b816,0x1000),(0x1b7aa,0x1100)]:
            assert base64.b64decode(em.snapshot('code',address,4,diagnostics=False)['frame'])==struct.pack('<2H',0xad03,entry)
        em.qmp.execute('cont');deadline=time.monotonic()+8;state=''
        while time.monotonic()<deadline:
            state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=03:{pc:04x}'.lower() in state.lower():break
            time.sleep(.01)
        em.qmp.execute('stop');(out/(name+'.cpu.txt')).write_text(state,encoding='utf8')
        assert f'PC=03:{pc:04x}'.lower() in state.lower(),state[:1600]
        assert 'ROM_MISSING=none' in state and 'UNSUPPORTED=none' in state,state[:1600]
        regs={int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})','\n'.join(state.splitlines()[:5]),re.I)}
        assert regs[12]==0x600d and regs[15]==0x6000,regs
        def memory(address,size):return base64.b64decode(em.snapshot('data',address,size,diagnostics=False)['frame'])
        def word(a):return struct.unpack('<H',memory(a,2))[0]
        def pair(a):h,l=struct.unpack('<2H',memory(a,4));return h<<16|l
        row=dict(name=name,count=count,rate=rate,header_bits_input=header,steps=steps,samples=samples,start=start,length=length,real_npe_parameters=real,
                 positions=positions,header_bits=word(0x512838),frame_bits=pair(0x5129d0),decoded_samples=word(0x512afa),
                 total_ms=pair(0x51283e),elapsed_ms=pair(0x512842),start_bits=pair(0x1700),length_bits=pair(0x1704),
                 position_calls=word(0x16a0),decode_calls=word(0x16a4),stack_restored=True,host_guest_memory_writes=0,passed=True)
        assert row['position_calls']==7 and row['decode_calls']==3,row
        assert row['header_bits']==(header&65535) and row['frame_bits']==(steps[-1]&65535),row
        cases.append(row);print(json.dumps(row),flush=True)
    finally:
        if em:em.close()
report=dict(passed=len(cases)==len(inputs),cases=cases,original_prg_sha256=hashlib.sha256(original).hexdigest(),
            qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),npe_sha256=hashlib.sha256(npe).hexdigest(),
            scope='original three-probe wrapper and all range arithmetic; codec positions and PCM counts supplied by guest boundaries, not original codec proof')
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
