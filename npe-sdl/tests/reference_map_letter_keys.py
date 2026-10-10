"""Run the unchanged original map key dispatcher with service adapters.

This measures key routing, direction and exit selection. Walking, selectors,
LCD services and the periodic callback are adapters; no game or timing claim.
"""
import argparse, base64, hashlib, json, re, struct, sys, time
from pathlib import Path

sys.path.insert(0, str(Path('scripts').resolve()))
from web.np560_web import Emulator

parser = argparse.ArgumentParser()
parser.add_argument('--qemu', type=Path, required=True)
args = parser.parse_args()
out = Path('tmp/map-system-letter-native')
out.mkdir(exist_ok=True)
original = Path('tmp/NP560.PRG').read_bytes()
resident = bytearray(bytes(0x30000) + original)
caller = bytearray(original)
start, end = 0x1a1312, 0x1a1c92
measured = original[start-0x30000:end-0x30000]

def patch(address, words):
    data = struct.pack('<%dH' % len(words), *words)
    resident[address:address+len(data)] = data
    caller[address-0x30000:address-0x30000+len(data)] = data

# Only these services are reached with the specified diagnostic input.
for address in [0x034154, 0x1d5820, 0x034218, 0x332ff2]:
    patch(address, [0xc620])
patch(0x1e1452, [0x6810,0xa4,0x99f9,0x69d4,0x88fa,0x7a76,0xc620])
for address, tag in [(0x163752,32), (0x29ce22,16)]:
    patch(address, [0x68f9,tag,0x6810,0xa4,0x88fa,0x7a76,0x68f0,65535,0xc620])
assert resident[start:end] == measured

cases = []
for letter, native, direction in [('i',0x82,2),('m',0x83,0),('j',0x80,3),('l',0x81,1)]:
    for key in [ord(letter),ord(letter.upper()),native]:
        cases.append(dict(key=key,choice=0,tag=direction,direction=direction))
for key in [ord('k'),ord('K'),0x86]:
    cases.append(dict(key=key,choice=0,tag=16,direction=0xaa))
for key in [0x20,0x90]:
    cases.append(dict(key=key,choice=0,tag=32,direction=0xaa))
cases.append(dict(key=0x8e,choice=0,tag=65535,direction=0xaa,mode=6,init=1))
for key in [ord(x) for x in 'uUeEqQa'] + [0,0x84,0x85]:
    cases.append(dict(key=key,choice=0,tag=65535,direction=0xaa))
for choice in [1,2]:
    for letter, native, selected in [('i',0x82,1),('m',0x83,2)]:
        for key in [ord(letter),ord(letter.upper()),native]:
            cases.append(dict(key=key,choice=choice,tag=65535,direction=0xaa,selected=selected))
    for key in [ord(x) for x in 'jJlLuUeEqQ'] + [0x80,0x81]:
        cases.append(dict(key=key,choice=choice,tag=65535,direction=0xaa))
code = [0x6800,2,0x68ff,0x6000]

def word(address, value):
    code.extend([0x6810,address>>15,0x68f2,value,0xa8f2,address&0x7fff])

def byte(address, value):
    code.extend([0x6810,address>>15,0x69f2,value,0xa9f2,address&0x7fff])

for index,cfg in enumerate(cases):
    word(0x527ab4,0); word(0x527a86,0); word(0x527a5c,0)
    word(0x527934,1); word(0x527a76,cfg['key'])
    byte(0x527ac4,cfg['choice']); byte(0x5269d4,0xaa)
    code += [0x68f9,65535,0x68fa,65535,0xbd1a,0x1312,
             0x6810,0xa4,0xa8f9,0x1800+index*12,0xa8fa,0x1802+index*12]
    for offset, opcode, source in [(4,0x99f2,0x69d4),(6,0x99f2,0x7ac4),
                                    (8,0x88f2,0x7934),(10,0x88f2,0x7ab4)]:
        code += [opcode,source,0xa8f2,0x1800+index*12+offset]
pc = len(code)*2
code.append(0xc701)
assert len(code)*2 < 0x4154
patch(0x30000, code)
rom, firmware = out/'dispatch.rom', out/'dispatch.prg'
rom.write_bytes(resident); firmware.write_bytes(caller)
emulator = None
try:
    emulator = Emulator(qemu=args.qemu,firmware=firmware,soc_rom=rom,soc_rom_skeleton=True,
                        boot_mode='firmware-vma03',start_paused=True)
    assert base64.b64decode(emulator.snapshot('code',start,end-start)['frame']) == measured
    emulator.qmp.execute('cont')
    deadline = time.monotonic()+5
    while time.monotonic()<deadline:
        state = emulator.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
        if f'PC=03:{pc:04x}'.lower() in state.lower(): break
        time.sleep(.01)
    emulator.qmp.execute('stop')
    assert f'PC=03:{pc:04x}'.lower() in state.lower(),state[:600]
    assert 'ROM_MISSING=none' in state and 'INVALID_CONTROL=none' in state,state[:600]
    regs = {int(k):int(v,16) for k,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]+)',
                                                '\n'.join(state.splitlines()[:5]),re.I)}
    assert regs[15]==0x6000,regs
    raw = base64.b64decode(emulator.snapshot('data',0x521800,12*len(cases))['frame'])
    (out/'results.bin').write_bytes(raw); (out/'cpu.txt').write_text(state,encoding='utf-8')
    for index,cfg in enumerate(cases):
        actual = struct.unpack_from('<6H',raw,index*12)
        expected = (cfg['tag'],cfg['key'] if cfg['tag']!=65535 else 65535,
                    cfg['direction'],cfg.get('selected',cfg['choice']),
                    cfg.get('mode',1),cfg.get('init',0))
        assert actual==expected,(index,cfg,actual,expected)
        cfg.update(actual=list(actual),passed=True)
    report = dict(passed=True,case_count=len(cases),cases=cases,
                  original_prg_sha256=hashlib.sha256(original).hexdigest(),
                  qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),
                  measured_code_sha256=hashlib.sha256(measured).hexdigest(),
                  scope=__doc__,host_guest_memory_writes=0,nand_used=False)
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(f'{len(cases)} original map direction/exit/menu/ignored key cases passed',flush=True)
finally:
    if emulator: emulator.close()
