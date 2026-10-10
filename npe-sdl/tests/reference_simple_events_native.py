"""Run unchanged original mode23/24 functions and cross-check the WASM inputs/results.

Only the diagnostic bootstrap is written. Functions have no external calls;
their code, sparse data inputs, complete role/NPC outputs and stack are checked.
"""
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

out = Path('tmp/simple-events/native')
out.mkdir(parents=True, exist_ok=True)
prg = Path('tmp/NP560.PRG').read_bytes()
assert hashlib.sha256(prg).hexdigest() == 'd555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5'
rows = json.loads(Path('tmp/native-simple-events.json').read_text(encoding='utf-8'))
assert len(rows) == 18
images = {}
for mode, segment, entry, source, length in [(23, 0x27, 0xc8b6, 0x44c8b6, 0x32e),
                                            (24, 0x30, 0x383c, 0x4d383c, 0x10e)]:
    image = bytearray(0x30000) + prg
    code = [0x6800, 2, 0x68ff, 0x6000, 0x6810, 0, 0x6811, 0,
            0xbd00 | segment, entry, 0x68fc, 0x600d]
    final_pc = len(code) * 2
    code += [0xd0ff]
    encoded = struct.pack('<%dH' % len(code), *code)
    image[0x30000:0x30000 + len(encoded)] = encoded
    rom = out / f'mode{mode}.rom'
    firmware = out / f'mode{mode}.prg'
    rom.write_bytes(image)
    firmware.write_bytes(image[0x30000:])
    images[mode] = (rom, firmware, final_pc, (segment << 16) | entry, source, length)

report = {'passed': False, 'scope': 'Unchanged original numeric services; sparse diagnostic data, no LCD/physical timing claim',
          'program_sha256': hashlib.sha256(prg).hexdigest(), 'cases': []}
try:
    for row in rows:
        name, mode, before = row['name'], row['mode'], row['before']
        records = Path(f'tmp/native-simple-events-{name}-before.records').read_bytes()
        expected_records = Path(f'tmp/native-simple-events-{name}-after.records').read_bytes()
        assert len(records) == len(expected_records) == 444
        ram = bytearray(65536)
        ram[0x69c6:0x6b82] = records
        ram[0x6fba:0x6fba + 250] = bytes(before['npcState'])
        struct.pack_into('>HH', ram, 0x7934, mode, row['event'])
        raw = bytearray(ram)
        raw[0::2], raw[1::2] = raw[1::2], raw[0::2]
        path = out / f'{name}.input'
        path.write_bytes(raw)
        rom, firmware, final_pc, runtime, source, length = images[mode]
        emulator = None
        try:
            emulator = Emulator(qemu=Path('E:/qemu-dmcu-win-build/qemu-system-dmcu.exe'),
                                firmware=firmware, soc_rom=rom, soc_rom_skeleton=True,
                                boot_mode='firmware-vma03', start_paused=True)
            emulator.load_data(0x520000, path.resolve())
            actual_code = base64.b64decode(emulator.snapshot('code', runtime, length, diagnostics=False)['frame'])
            assert actual_code == prg[source - 0x200000:source - 0x200000 + length]

            def memory(address, count):
                data = bytearray(base64.b64decode(emulator.snapshot('data', address, count, diagnostics=False)['frame']))
                data[0::2], data[1::2] = data[1::2], data[0::2]
                return bytes(data)

            assert memory(0x520000, 65536) == ram
            emulator.qmp.execute('cont')
            deadline = time.monotonic() + 15
            state = ''
            while time.monotonic() < deadline:
                state = emulator.qmp.execute('qom-get', {'path': '/machine', 'property': 'cpu-state'})
                if f'PC=03:{final_pc:04x}'.lower() in state.lower():
                    break
                if any(k + '=none' not in state for k in ['ROM_MISSING', 'UNSUPPORTED', 'INVALID_CONTROL']):
                    break
                time.sleep(.01)
            emulator.qmp.execute('stop')
            (out / f'{name}.cpu.txt').write_text(state, encoding='utf-8')
            assert f'PC=03:{final_pc:04x}'.lower() in state.lower(), state[:2000]
            assert all(k + '=none' in state for k in ['ROM_MISSING', 'UNSUPPORTED', 'INVALID_CONTROL']), state
            registers = {int(r): int(v, 16) for r, v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})', state, re.I)}
            assert registers[12] == 0x600d and registers[15] == 0x6000
            after = memory(0x520000, 65536)
            assert after[0x69c6:0x6b82] == expected_records, name
            assert after[0x6fba:0x6fba + 250] == bytes(row['after']['npcState']), name
            assert struct.unpack_from('>HH', after, 0x7934) == (1, row['event'])
            assert struct.unpack_from('>H', after, 0x7ab4)[0] == 1
            expected = bytearray(ram)
            expected[0x69c6:0x6b82] = expected_records
            expected[0x6fba:0x6fba + 250] = bytes(row['after']['npcState'])
            struct.pack_into('>H', expected, 0x7934, 1)
            struct.pack_into('>H', expected, 0x7ab4, 1)
            assert after == expected, ('unexpected original data write', name)
            (out / f'{name}.before.bin').write_bytes(ram)
            (out / f'{name}.after.bin').write_bytes(after)
            report['cases'].append({'name': name, 'passed': True, 'mode': mode,
                                    'npc_state': row['after']['npcState'][row['event'] if 1 <= row['event'] < 250 else 60],
                                    'records_sha256': hashlib.sha256(expected_records).hexdigest(),
                                    'full_segment_matches_expected_writes': True})
            print(name, 'original CPU/WASM agree; mode1/init1, full data segment checked', flush=True)
        finally:
            if emulator:
                emulator.close()
    report['passed'] = True
finally:
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
