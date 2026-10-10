"""Original foreground start/stop return order and nominal Timer2 counts.

No codec/file call is replaced or claimed: the isolated caller executes only
08:31AC/3262 and their original driver/Timer2 services, with zero DAC targets.
"""
import argparse
import base64
import hashlib
import json
import re
import struct
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path('scripts').resolve()))
sys.path.insert(0, str(Path('scripts/tests').resolve()))
from web.np560_web import Emulator
from test_np560_dtu import write_word

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--qemu', type=Path, required=True)
args = p.parse_args()
out = Path('tmp/audio-foreground-wait-native')
out.mkdir(parents=True, exist_ok=True)
original = Path('tmp/NP560.PRG').read_bytes()
image = bytearray(0x30000) + original
rom = out / 'original-services.rom'
rom.write_bytes(image)
cases = []
for name, idle, mute, calls, expected in [
    ('start', True, False, [0x31AC], 200),
    ('replace', False, False, [0x3262, 0x31AC], 250),
    ('two-requests', True, False, [0x31AC, 0x3262, 0x31AC], 450),
    ('stop', False, False, [0x3262], 50),
    ('idle-stop', True, False, [0x3262], 0),
    ('mute-start', True, True, [0x31AC], 0),
]:
    code = [0x6800, 2, 0x68FF, 0x6000, 0x68E9, 0x8000, 0xEE00,
            0x682E, 0, 0xFE00, 0x6810, 0, 0x6831, 0x0040,
            0x6870, 0, 0x6871, 0, 0x6872, 1]
    write_word(code, 0xF010, 4 if idle else 1)
    write_word(code, 0xF014, 0x80 if mute else 0)
    for i, call in enumerate(calls):
        code += [0xBD08, call]
        write_word(code, 0x1B00, i + 1)
    code += [0x68F4, 0x600D]
    pc = len(code) * 2
    code += [0xD0FF]
    caller = bytearray(image[0x200000:])
    caller[:len(code) * 2] = struct.pack('<%dH' % len(code), *code)
    firmware = out / (name + '.prg')
    firmware.write_bytes(caller)
    em = None
    try:
        em = Emulator(qemu=args.qemu, firmware=firmware, soc_rom=rom,
                      soc_rom_skeleton=True, start_paused=True, icount_shift=0)
        em.qmp.execute('qom-set', {'path': '/machine', 'property': 'instruction-clock', 'value': True})
        for address, size in [(0x0831AC, 0x62), (0x083262, 0x30), (0x084702, 0x5C)]:
            observed = base64.b64decode(em.snapshot('code', address, size, diagnostics=False)['frame'])
            source = address - 0x30000
            assert observed == original[source:source + size], hex(address)
        em.qmp.execute('cont')
        deadline = time.monotonic() + 12
        state = ''
        while time.monotonic() < deadline:
            state = em.qmp.execute('qom-get', {'path': '/machine', 'property': 'cpu-state'})
            if f'PC=20:{pc:04x}'.lower() in state.lower():
                break
            time.sleep(.02)
        em.qmp.execute('stop')
        (out / (name + '.cpu.txt')).write_text(state, encoding='utf8')
        assert f'PC=20:{pc:04x}'.lower() in state.lower(), state[:1500]
        assert all(k + '=none' in state for k in ['ROM_MISSING', 'UNSUPPORTED', 'INVALID_CONTROL']), state[:1500]
        registers = {int(r): int(v, 16) for r, v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})', state, re.I)}
        assert registers[15] == 0x6000
        count = struct.unpack('<H', base64.b64decode(em.snapshot('data', 0x1B00, 2, diagnostics=False)['frame']))[0]
        assert count == len(calls)
        timer = em.qmp.execute('qom-get', {'path': '/machine', 'property': 'timer2-state'})
        ticks = int(re.search(r'EXPIRED:(\d+)', timer)[1])
        assert ticks == expected and 'RUNNING:0' in timer, timer
        events = em.audio()['control_events']
        cases.append(dict(name=name, calls=calls, timer2_ticks=ticks,
                          returned_calls=count, events=events, passed=True))
        print(f'{name}: {count} original calls returned after {ticks} Timer2 ticks', flush=True)
    finally:
        if em:
            em.close()
report = dict(passed=True, original_prg_sha256=hashlib.sha256(original).hexdigest(),
              qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(), cases=cases,
              scope='Original foreground driver/Timer2 calls and return order, zero DAC targets; no codec, whole file playback or physical timing claim.')
(out / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
