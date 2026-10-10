"""Run original battle/item/magic key dispatchers with recorded boundaries.

Measured dispatch code is unchanged. Downstream UI/action services are return
adapters recording the selected service and original key; no game is played.
"""
import argparse, base64, hashlib, json, re, struct, sys, time
from pathlib import Path

sys.path.insert(0, str(Path('scripts').resolve()))
from web.np560_web import Emulator

parser = argparse.ArgumentParser()
parser.add_argument('--qemu', type=Path, required=True)
args = parser.parse_args()
out = Path('tmp/battle-letter-native')
out.mkdir(exist_ok=True)
original = Path('tmp/NP560.PRG').read_bytes()
resident = bytearray(bytes(0x30000) + original)
caller = bytearray(original)
services = {
    'battle': (0x2a5246, 0x2a54ba, {1:0x2ca5f2, 2:0x2a8a54, 4:0x317e66, 8:0x2f516c, 16:0x31f552, 64:0x19dbe0}),
    'item': (0x2eb5a0, 0x2eb70e, {1:0x19337a, 2:0x1947a2, 4:0x297caa, 8:0x29619a, 16:0x1acc72, 64:0x3315a8}),
    'magic': (0x2f312e, 0x2f327c, {1:0x2cd710, 2:0x2cf118, 4:0x308170, 8:0x303c6e, 16:0x121570, 64:0x32b4ec}),
}
measured = {name: original[start-0x30000:end-0x30000] for name,(start,end,_) in services.items()}
for name, (_, _, targets) in services.items():
    for tag, address in targets.items():
        # Battle pushes its raw argument; the list dispatchers read7A76.
        words = [0x2e00, 0x68f9, tag]
        words += [0x78ab, 6] if name == 'battle' else [0x6810, 0xa4, 0x88fa, 0x7a76]
        words += [0x2f00, 0xc620]
        data = struct.pack('<%dH' % len(words), *words)
        resident[address:address+len(data)] = data
        caller[address-0x30000:address-0x30000+len(data)] = data
for name,(start,end,_) in services.items():
    assert resident[start:end] == measured[name], name
rom = out/'dispatch.rom'

cases = []
for name in services:
    for letter, native, tag in [('i',0x82,1),('j',0x80,4),('k',0x86,16),('l',0x81,8),('m',0x83,2)]:
        for key in [ord(letter),ord(letter.upper()),native]:
            cases.append(dict(service=name,key=key,expected=tag,blocked=0,list_enabled=1))
    for key,tag in [(0x8e,64),(ord('a'),65535),(0,65535),(0x20,65535)]:
        cases.append(dict(service=name,key=key,expected=tag,blocked=0,list_enabled=1))
cases += [dict(service='battle',key=k,expected=65535,blocked=0,list_enabled=1) for k in [0x84,0x85]]
cases += [dict(service='battle',key=ord(k),expected=65535,blocked=1,list_enabled=1) for k in 'ijklm']
cases += [dict(service='magic',key=ord(k),expected=65535,blocked=0,list_enabled=0) for k in 'jl']
code = [0x6800,2,0x68ff,0x6000]
def word(address,value):
    code.extend([0x6810,address>>15,0x68f2,value,0xa8f2,address&0x7fff])
def byte(address,value):
    code.extend([0x6810,address>>15,0x69f2,value,0xa9f2,address&0x7fff])
for index,cfg in enumerate(cases):
    byte(0x527aa8,cfg['blocked']); byte(0x527ac8,10)
    byte(0x527a40,cfg['list_enabled']); byte(0x527a54,0)
    word(0x527ab4,0); word(0x527a76,cfg['key'])
    code += [0x68f9,65535,0x68fa,65535,0x68f0,cfg['key'],0x58f0]
    address = services[cfg['service']][0]
    code += [0xbd00|(address>>16),address&65535,0x50f2,
             0x6810,0xa4,0xa8f9,0x1800+index*4,0xa8fa,0x1802+index*4]
pc = len(code)*2
code.append(0xc701)
assert len(code)*2 < 0x8000
firmware = out/'dispatch.prg'
driver = struct.pack('<%dH' % len(code),*code)
resident[0x30000:0x30000+len(driver)] = driver
caller[:len(driver)] = driver
rom.write_bytes(resident)
firmware.write_bytes(caller)
emulator = None
try:
    emulator = Emulator(qemu=args.qemu,firmware=firmware,soc_rom=rom,soc_rom_skeleton=True,
                        boot_mode='firmware-vma03',start_paused=True)
    for name,(start,end,_) in services.items():
        packet = emulator.snapshot('code',start,end-start)
        assert base64.b64decode(packet['frame']) == measured[name],name
    emulator.qmp.execute('cont')
    deadline = time.monotonic()+5
    while time.monotonic()<deadline:
        state = emulator.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
        if f'PC=03:{pc:04x}'.lower() in state.lower(): break
        time.sleep(.01)
    emulator.qmp.execute('stop')
    assert f'PC=03:{pc:04x}'.lower() in state.lower(),state[:500]
    assert 'ROM_MISSING=none' in state and 'INVALID_CONTROL=none' in state,state[:500]
    regs = {int(k):int(v,16) for k,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]+)','\n'.join(state.splitlines()[:5]),re.I)}
    assert regs[15]==0x6000,regs
    raw = base64.b64decode(emulator.snapshot('data',0x521800,4*len(cases))['frame'])
    (out/'results.bin').write_bytes(raw)
    (out/'cpu.txt').write_text(state,encoding='utf-8')
    for index,cfg in enumerate(cases):
        # debug-data-save contains physical native words, low byte first.
        tag,key = struct.unpack_from('<2H',raw,index*4)
        assert tag==cfg['expected'],(index,cfg,tag,key)
        assert key==(cfg['key'] if tag!=65535 else 65535),(index,cfg,tag,key)
        cfg.update(actual=tag,raw_key=key,passed=True)
    report = dict(passed=True,cases=cases,case_count=len(cases),
                  original_prg_sha256=hashlib.sha256(original).hexdigest(),
                  qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),
                  measured_code_sha256={k:hashlib.sha256(v).hexdigest() for k,v in measured.items()},
                  scope='Original key routing and raw argument/global preservation; downstream services are adapters, no LCD/game/timing claim.',
                  host_guest_memory_writes=0,nand_used=False)
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(f'{len(cases)} original battle/item/magic key dispatch cases passed',flush=True)
finally:
    if emulator: emulator.close()
