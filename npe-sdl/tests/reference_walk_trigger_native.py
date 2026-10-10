"""Execute unchanged original mode20, including its original multiply helper."""
import base64
import hashlib
import json
from pathlib import Path
import re
import struct
import sys
import time

sys.path.insert(0, str(Path('scripts').resolve()))
from web.np560_web import Emulator

out = Path('tmp/walk-trigger/native')
out.mkdir(parents=True, exist_ok=True)
prg = Path('tmp/NP560.PRG').read_bytes()
assert hashlib.sha256(prg).hexdigest() == 'd555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5'
rows = json.loads(Path('tmp/native-walk-trigger-resident.json').read_text())['services']
image = bytearray(0x30000) + prg
code = [0x6800, 2, 0x68ff, 0x6000, 0x6810, 0, 0x6811, 0,
        0xbd30, 0xac04, 0x68fc, 0x600d, 0xd0ff]
image[0x30000:0x30000 + len(code)*2] = struct.pack('<%dH' % len(code), *code)
rom, firmware = out/'mode20.rom', out/'mode20.prg'
rom.write_bytes(image)
firmware.write_bytes(image[0x30000:])
report = {'passed': False, 'program_sha256': hashlib.sha256(prg).hexdigest(),
          'scope': 'Original mode20 numeric service, seeded diagnostic RAM; no LCD or physical timing claim', 'cases': []}
try:
    for row in rows:
        name, before = row['name'], row['before']
        ram = bytearray(65536)
        ram[0x7490:0x74f4] = bytes(before['cache'])
        struct.pack_into('>HH', ram, 0x7934, 20, row['event'])
        raw = bytearray(ram)
        raw[0::2], raw[1::2] = raw[1::2], raw[0::2]
        path = out/f'{name}.input'
        path.write_bytes(raw)
        emulator = None
        try:
            emulator = Emulator(qemu=Path('E:/qemu-dmcu-win-build/qemu-system-dmcu.exe'),
                                firmware=firmware, soc_rom=rom, soc_rom_skeleton=True,
                                boot_mode='firmware-vma03', start_paused=True)
            emulator.load_data(0x520000, path.resolve())
            original = base64.b64decode(emulator.snapshot('code', 0x30ac04, 0xea, diagnostics=False)['frame'])
            assert original == prg[0x2dac04:0x2dacee]
            def memory():
                data = bytearray(base64.b64decode(emulator.snapshot('data', 0x520000, 65536, diagnostics=False)['frame']))
                data[0::2], data[1::2] = data[1::2], data[0::2]
                return bytes(data)
            assert memory() == ram
            emulator.qmp.execute('cont')
            deadline = time.monotonic() + 15
            state = ''
            while time.monotonic() < deadline:
                state = emulator.qmp.execute('qom-get', {'path': '/machine', 'property': 'cpu-state'})
                if 'pc=03:0018' in state.lower(): break
                if any(k+'=none' not in state for k in ['ROM_MISSING','UNSUPPORTED','INVALID_CONTROL']): break
                time.sleep(.01)
            emulator.qmp.execute('stop')
            (out/f'{name}.cpu.txt').write_text(state, encoding='utf-8')
            assert 'pc=03:0018' in state.lower(), state[:2000]
            assert all(k+'=none' in state for k in ['ROM_MISSING','UNSUPPORTED','INVALID_CONTROL']), state
            registers = {int(r): int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})',state,re.I)}
            assert registers[12] == 0x600d and registers[15] == 0x6000
            after = memory()
            mode,event = struct.unpack_from('>HH', after, 0x7934)
            assert struct.unpack_from('>H',after,0x7ab4)[0] == 1
            assert after[0x7490:0x74f4] == bytes(before['cache'])
            # Compare raw original return with its UI dispatch continuation.
            assert row['after']['mode'] == 1
            assert row['after']['event'] == event
            assert (mode if 1<=mode<=24 else 1) == row['after']['mode']
            expected = bytearray(ram)
            struct.pack_into('>HH',expected,0x7934,mode,event)
            struct.pack_into('>H',expected,0x7ab4,1)
            assert after == expected, 'unexpected original data write'
            (out/f'{name}.before.bin').write_bytes(ram)
            (out/f'{name}.after.bin').write_bytes(after)
            report['cases'].append({'name':name,'passed':True,'mode':mode,'event':event,'init':1,'full_segment_checked':True})
            print(name, 'original numeric mode/event/init:', mode,event,1, flush=True)
        finally:
            if emulator: emulator.close()
    report['passed'] = True
finally:
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
