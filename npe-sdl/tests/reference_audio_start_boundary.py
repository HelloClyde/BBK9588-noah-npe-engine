"""Original 08:31AC publication of a prepared playback's driver flags.

The caller supplies flags and zero DAC targets explicitly. No codec, file,
PCM exhaustion, complete playback or physical ramp timing is claimed.
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=Path('tmp/audio-start-boundary-native'))
    args = parser.parse_args()
    out = args.output
    out.mkdir(parents=True, exist_ok=True)
    original = Path('tmp/NP560.PRG').read_bytes()
    assert hashlib.sha256(original).hexdigest() == 'd555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5'
    image = bytearray(0x30000) + original
    rom = out/'original-services.rom'
    rom.write_bytes(image)
    cases = []
    for name, flags89, mute in [
        ('finite', 0x5050, False),
        ('finite-provisional-end', 0x5850, False),
        ('loop-provisional-end', 0xd850, False),
        ('callback-provisional-end', 0xd858, False),
        ('suspended-provisional-end', 0x5c50, False),
        ('producer-provisional-end', 0x5854, False),
        ('mute-provisional-end', 0x5850, True),
        ('mute-callback-provisional-end', 0xd858, True),
    ]:
        code = [0x6800, 2, 0x68ff, 0x6000, 0x68e9, 0x8000, 0xee00,
                0x682e, 0, 0xfe00, 0x6810, 0, 0x6831, 0x0040,
                0x6870, 0, 0x6871, 0, 0x6872, 1]
        for address, value in [(0xf010, 4), (0xf012, flags89),
                               (0xf014, 0x80 if mute else 0), (0x1b00, 0)]:
            write_word(code, address, value)
        code += [0xbd08, 0x31ac]
        write_word(code, 0x1b00, 1)
        code += [0x68f4, 0x600d]
        pc = len(code)*2
        code += [0xd0ff]
        caller = bytearray(image[0x200000:])
        caller[:len(code)*2] = struct.pack('<%dH' % len(code), *code)
        firmware = out/(name+'.prg')
        firmware.write_bytes(caller)
        emulator = None
        try:
            emulator = Emulator(qemu=args.qemu, firmware=firmware, soc_rom=rom,
                                soc_rom_skeleton=True, start_paused=True, icount_shift=0)
            emulator.qmp.execute('qom-set', {'path': '/machine', 'property': 'instruction-clock', 'value': True})
            for address, size in [(0x0831ac, 0x62), (0x084702, 0x5c), (0x249a1e, 0x9a)]:
                actual = base64.b64decode(emulator.snapshot('code', address, size, diagnostics=False)['frame'])
                assert actual == image[address:address+size], hex(address)
            emulator.qmp.execute('cont')
            deadline = time.monotonic()+12
            state = ''
            while time.monotonic() < deadline:
                state = emulator.qmp.execute('qom-get', {'path': '/machine', 'property': 'cpu-state'})
                if f'PC=20:{pc:04x}'.lower() in state.lower():
                    break
                time.sleep(.02)
            emulator.qmp.execute('stop')
            (out/(name+'.cpu.txt')).write_text(state, encoding='utf-8')
            assert f'PC=20:{pc:04x}'.lower() in state.lower(), state[:1500]
            assert all(marker+'=none' in state for marker in ['ROM_MISSING', 'UNSUPPORTED', 'INVALID_CONTROL']), state[:1500]
            registers = {int(k): int(v, 16) for k, v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})', state, re.I)}
            assert registers[15] == 0x6000 and registers[4] == 0x600d
            def word(address):
                return struct.unpack('<H', base64.b64decode(emulator.snapshot('data', address, 2, diagnostics=False)['frame']))[0]
            after89, after8a, after88 = word(0xf012), word(0xf014), word(0xf010)
            timer = emulator.qmp.execute('qom-get', {'path': '/machine', 'property': 'timer2-state'})
            ticks = int(re.search(r'EXPIRED:(\d+)', timer)[1])
            assert word(0x1b00) == 1 and 'RUNNING:0' in timer
            assert after89 == ((flags89 & ~0x800) | 0x10) and after88 == 1
            assert after8a == (0x80 if mute else 0) and ticks == (0 if mute else 200)
            cases.append(dict(name=name, flags89_before=flags89, mute=mute,
                              flags89_after=after89, flags8a_after=after8a,
                              flags88_after=after88, timer2_ticks=ticks,
                              stack_restored=True, passed=True))
            print(name, f'{flags89:04x}->{after89:04x}', ticks, flush=True)
        finally:
            if emulator:
                emulator.close()
    data = b''.join(struct.pack('<6H', case['flags89_before'], int(case['mute']),
                               case['flags89_after'], case['flags8a_after'],
                               case['flags88_after'], case['timer2_ticks']) for case in cases)
    (out/'states.bin').write_bytes(data)
    result = dict(passed=len(cases) == 8, cases=cases, scope=__doc__,
                  original_prg_sha256=hashlib.sha256(original).hexdigest(),
                  qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),
                  fixture_sha256=hashlib.sha256(data).hexdigest(),
                  original_codec_checked=False, physical_timing_checked=False)
    (out/'report.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')


if __name__ == '__main__':
    main()
