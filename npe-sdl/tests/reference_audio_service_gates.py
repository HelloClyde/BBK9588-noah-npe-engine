"""Execute unmodified callback gates/returns with recording callbacks.

The input trampoline supplies the original IRQ saved-register frame.
405A/4070 are explicit captured hardware boundaries, not simulated devices.
"""
import argparse, base64, hashlib, json, re, struct, sys, time
from pathlib import Path
sys.path.insert(0, str(Path('scripts').resolve()))
sys.path.insert(0, str(Path('scripts/tests').resolve()))
from web.np560_web import Emulator
from test_np560_dtu import write_word, read_word

parser = argparse.ArgumentParser()
parser.add_argument('--qemu', type=Path, required=True)
args = parser.parse_args()
out = Path('tmp/audio-service-gates-native'); out.mkdir(exist_ok=True)
original = Path('tmp/NP560.PRG').read_bytes()
image = bytearray(0x30000) + original
resident = bytearray(image)
resident[0x100d0:0x100d4] = struct.pack('<2H', 0xad20, 0x1000)
for address, target in [(0x8405a, 0x1800), (0x84070, 0x1900)]:
    resident[address:address+4] = struct.pack('<2H', 0xad20, target)
rom = out/'gates-with-captured-hardware.rom'; rom.write_bytes(resident)
inputs = []
for service in range(3):
    for name, flags89, flags8a, flags8d in [
        ('ready',0x7070,3,0), ('all-busy',0x7077,3,0),
        ('no-registration',0x70,3,0), ('no-request',0x7000,3,0),
        ('other-busy',0x7070 | (7 ^ (1<<service)),3,0),
        ('storage-busy',0x7070,3,1), ('spi-busy',0x7070,3,4),
        ('unrelated-storage',0x7070,3,0xfffa),
        ('resume',0x7470,3,0), ('starved',0x7070,0x43,0),
        ('starved-active',0x7074,0x43,0),
        ('starved-active-resume',0x7474,0x43,0),
    ]:
        inputs.append((f'{service}-{name}',service,flags89,flags8a,flags8d,0,0))
for mask in range(32):
    flags89=0x4040 | (4 if mask&1 else 0)
    if mask&2:flags89&=~0x4000
    if mask&4:flags89&=~0x40
    inputs.append((f'producer-mask-{mask:02d}',2,flags89,3,
                   (1 if mask&8 else 0)|(4 if mask&16 else 0),0,0))
# Callback changes must be observed by later gates; return clears current
# activity only. The producer also changes its file-status word, which the
# original return restores independently of these flags.
inputs += [('pcm-requests-producer',1,0x2020,3,0,0x4040,0),
           ('pcm-cancels-producer',1,0x6060,3,0,0,0x4040),
           ('producer-return-preserves-change',2,0x4040,3,0,0x8020,0),
           ('output-requests-pcm',0,0x1000,3,0,0x2020,0)]
cases=[]
for name, service, before89, before8a, flags8d, callback_or, callback_clear in inputs:
    code=[0x6800,2,0x68ff,0x6000,0x6810,0,0x6811,0]
    initial=[(0xf012,before89),(0xf014,before8a),(0xf01a,flags8d),
             (0xf01c,0xc002),(0x1300,0),(0x1328,0)]
    for slot, offset in enumerate([0x1200,0x1400,0x1600]):
        initial += [(0x4a9c+slot*4,0x20),(0x4a9e+slot*4,offset)]
        for delta in range(0,10,2):initial.append((0x1308+slot*10+delta,0))
    for address,value in initial:write_word(code,address,value)
    code += [0x6810,0xa2]
    write_word(code,0x2ae0,0x1234,banked=True)
    code += [0x6810,0,0x6811,0,0x68f1,0x1357,0x68f2,0x2468,
             0x68f3,0x369c,0x68f7,0x789a,0x0f34,0x68fc,0x600d]
    pc=len(code)*2;code += [0xd0ff]
    caller=bytearray(image[0x200000:])
    caller[:len(code)*2]=struct.pack('<%dH'%len(code),*code)
    trampoline=[0x1f09,0x1f10,0x58f0,0x6810,0,0xad08,[0x3c22,0x3d70,0x3ec2][service]]
    caller[0x1000:0x1000+len(trampoline)*2]=struct.pack('<%dH'%len(trampoline),*trampoline)
    bodies={0x1000:trampoline}
    for slot, offset in enumerate([0x1200,0x1400,0x1600]):
        body=[0x58f0,0x58f1,0x6810,0]
        for delta,address in [(0,0xf012),(2,0xf014),(4,0xf01a),(6,0xf01c)]:
            read_word(body,address,0);body += [0x3f10,0xa8f0,0x1308+slot*10+delta]
        write_word(body,0x1310+slot*10,1)
        body += [0x3f10,0x88f0,0xf012,0x75f0,callback_or,
                 0x74f0,(~callback_clear)&65535,0x3f10,0xa8f0,0xf012]
        if slot==2:
            body += [0x6810,0xa2]
            write_word(body,0x2ae0,0xabcd,banked=True)
        body += [0x381f,0x380f,0xc620]
        caller[offset:offset+len(body)*2]=struct.pack('<%dH'%len(body),*body)
        bodies[offset]=body
    for offset,route in [(0x1800,2),(0x1900,3)]:
        body=[];write_word(body,0x1328,route)
        body += [0x380f,0x1f90,0x1f89,0xc610]
        caller[offset:offset+len(body)*2]=struct.pack('<%dH'%len(body),*body)
        bodies[offset]=body
    firmware=out/(name+'.prg');firmware.write_bytes(caller)
    em=None
    try:
        em=Emulator(qemu=args.qemu,firmware=firmware,soc_rom=rom,
                    soc_rom_skeleton=True,start_paused=True,icount_shift=0)
        for start,end in [(0x83c22,0x8405a)]:
            assert base64.b64decode(em.snapshot('code',start,end-start,diagnostics=False)['frame'])==image[start:end]
        for offset,body in bodies.items():
            assert base64.b64decode(em.snapshot('code',0x200000+offset,len(body)*2,diagnostics=False)['frame'])==struct.pack('<%dH'%len(body),*body)
        em.qmp.execute('cont');deadline=time.monotonic()+10;state=''
        while time.monotonic()<deadline:
            state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=20:{pc:04x}'.lower() in state.lower():break
            time.sleep(.01)
        em.qmp.execute('stop');(out/(name+'.cpu.txt')).write_text(state,encoding='utf8')
        assert f'PC=20:{pc:04x}'.lower() in state.lower(),state[:1300]
        assert 'ROM_MISSING=none' in state and 'UNSUPPORTED=none' in state,state[:1300]
        regs={int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})','\n'.join(state.splitlines()[:5]),re.I)}
        assert [regs[r] for r in [1,2,3,7,12,15]]==[0x1357,0x2468,0x369c,0x789a,0x600d,0x6000],regs
        def memory(address,count):return base64.b64decode(em.snapshot('data',address,count,diagnostics=False)['frame'])
        raw=memory(0x1308,32);entries=[list(struct.unpack('<5H',raw[i*10:i*10+10])) for i in range(3)]
        after=struct.unpack('<6H',memory(0xf012,12));route=struct.unpack('<H',memory(0x1328,2))[0]
        file_status=struct.unpack('<H',memory(0x512ae0,2))[0]
        assert file_status==0x1234,hex(file_status)
        (out/(name+'.entries.bin')).write_bytes(raw)
        (out/(name+'.flags.bin')).write_bytes(memory(0xf012,12))
        row=dict(name=name,service=service,before89=before89,before8a=before8a,
                 flags8d=flags8d,before8e=0xc002,callback_or=callback_or,callback_clear=callback_clear,
                 entries=entries,after89=after[0],after8a=after[1],after8d=after[4],after8e=after[5],
                 route=route,file_status=file_status,foreground_restored=True,host_guest_memory_writes=0,passed=True)
        cases.append(row);print(name,[e[4] for e in entries],route,flush=True)
    finally:
        if em:em.close()
report=dict(passed=len(cases)==len(inputs),cases=cases,
            original_prg_sha256=hashlib.sha256(original).hexdigest(),
            scope='original three callback gates and callback dispatch/return; captured405A/4070 hardware boundaries, recording callbacks, no original PCM/file IO/IRQ timing')
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
