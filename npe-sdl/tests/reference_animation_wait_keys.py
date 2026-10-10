"""Run the original frame wait key filter with explicit queued events.

Timer arming is observed, and event retrieval supplies a key then a timeout.
The original wait, parameter conversion and key translation stay unchanged.
This checks service dispatch, not physical key scanning or timer duration.
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
from web.np560_web import Emulator

PROGRAM_SHA256 = 'd555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=Path('tmp/animation-wait-keys'))
    args = parser.parse_args()
    out = args.output
    out.mkdir(parents=True, exist_ok=True)
    original = Path('tmp/NP560.PRG').read_bytes()
    assert hashlib.sha256(original).hexdigest() == PROGRAM_SHA256
    image = bytearray(0x30000) + original

    def install(address, words):
        image[address:address + len(words) * 2] = struct.pack('<%dH' % len(words), *words)

    # Timer-arm boundary records the argument supplied by the original wrapper.
    timer = [0x2e00, 0x58f0, 0x780b, 6, 0x3f10, 0xa8f0, 0x1904,
             0x380f, 0x2f00, 0xc620]
    install(0x3e400, timer)
    install(0x33463e, [0xad03, 0xe400])

    # All caller pointers refer to the diagnostic internal-RAM stack.
    reader = [0x2e00, 0x1f10, 0x58f1, 0x58f2, 0x58f3, 0x6810, 0,
              0x88f0, 0x1902, 0x5001, 0xa8f0, 0x1902, 0x5601, 0xac01, 0]
    key_branch = len(reader) - 1
    reader += [0x4823, 0x4830, 0xac00, 0]
    write_branch = len(reader) - 1
    reader[key_branch] = 0xe000 + len(reader) * 2
    reader += [0x4821, 0x88f3, 0x1900]
    reader[write_branch] = 0xe000 + len(reader) * 2
    reader += [0x781b, 8, 0x0a13, 0x781b, 12, 0x0a12, 0x4801,
               0x383f, 0x382f, 0x381f, 0x1f90, 0x2f00, 0xc620]
    install(0x3e000, reader)
    install(0x1ef734, [0xad03, 0xe000])

    code = [0x6800, 2, 0x68ff, 0x6000, 0x6810, 0, 0x6811, 0]

    def word(address, value):
        code.extend([0x6810, address >> 15, 0x68f0, value, 0xa8f0, address & 0x7fff])

    def capture(source, target):
        code.extend([0x6810, source >> 15, 0x88f0, source & 0x7fff,
                     0x3f10, 0xa8f0, target])

    inputs = [(parameter, key) for parameter in [15, 20] for key in range(256)]
    for index, (parameter, key) in enumerate(inputs):
        word(0x1900, key)
        word(0x1902, 0)
        word(0x50708a, 0)  # Event flag plus translation-enable byte.
        word(0x50708c, 0)
        code += [0x68f0, parameter, 0x58f0, 0xbd33, 0x317e, 0x50f2,
                 0x3f10, 0xa8f0, 0x2000 + index * 8]
        capture(0x1902, 0x2002 + index * 8)
        capture(0x1904, 0x2004 + index * 8)
        capture(0x50708a, 0x2006 + index * 8)
    code += [0x68fc, 0x600d]
    final_pc = len(code) * 2
    code += [0xd0ff]
    assert len(code) * 2 < 0xe000
    install(0x30000, code)
    rom, firmware = out / 'resident.rom', out / 'caller.prg'
    rom.write_bytes(image)
    firmware.write_bytes(image[0x30000:])
    emulator = None
    try:
        emulator = Emulator(qemu=args.qemu, firmware=firmware, soc_rom=rom,
                             soc_rom_skeleton=True, boot_mode='firmware-vma03', start_paused=True)
        checked = [(0x33317e, 0x50317e, 0x24), (0x2efc6a, 0x4bfc6a, 0x15a),
                   (0x2cd350, 0x49d350, 0x1e0)]
        for address, physical, size in checked:
            observed = base64.b64decode(emulator.snapshot('code', address, size, diagnostics=False)['frame'])
            assert observed == original[physical - 0x200000:physical - 0x200000 + size], hex(address)
        emulator.qmp.execute('cont')
        deadline = time.monotonic() + 45
        state = ''
        while time.monotonic() < deadline:
            state = emulator.qmp.execute('qom-get', {'path': '/machine', 'property': 'cpu-state'})
            if f'PC=03:{final_pc:04x}'.lower() in state.lower():
                break
            time.sleep(.02)
        emulator.qmp.execute('stop')
        (out / 'cpu.txt').write_text(state, encoding='utf8')
        assert f'PC=03:{final_pc:04x}'.lower() in state.lower(), state[:1800]
        assert all(key + '=none' in state for key in ['ROM_MISSING', 'UNSUPPORTED', 'INVALID_CONTROL']), state
        registers = {int(r): int(v, 16) for r, v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})', state, re.I)}
        assert registers[15] == 0x6000
        raw = base64.b64decode(emulator.snapshot('data', 0x2000, len(inputs) * 8, diagnostics=False)['frame'])
        accepted = {0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x8d, 0x8e}
        cases = []
        for index, (parameter, key) in enumerate(inputs):
            result, reads, armed_ms, flags = struct.unpack_from('<4H', raw, index * 8)
            assert armed_ms == parameter * 10
            if key in accepted:
                assert (result, reads, flags) == (key, 1, 0), (key, result, reads, flags)
            else:
                assert (result, reads, flags) == (0xf7, 2, 0x0400), (key, result, reads, flags)
            cases.append(dict(parameter=parameter, key=key, result=result, event_reads=reads, timer_arm_ms=armed_ms,
                              event_flag=flags >> 8, accepted=key in accepted))
        fixture = b'NPEWKEY1' + struct.pack('<I', len(inputs)) + raw
        (out / 'keys.bin').write_bytes(fixture)
        report = dict(passed=True, original_key_cases=len(inputs), accepted_keys=sorted(accepted),
                      cases=cases, checked_code=checked, adapters=['event retrieval', 'timer arm observation'],
                      parameters=[15, 20], translation_flags=0, physical_scanning_checked=False,
                      physical_timing_checked=False, program_sha256=hashlib.sha256(original).hexdigest(),
                      fixture_sha256=hashlib.sha256(fixture).hexdigest())
        (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
        print('Original animation wait:512 parameter/key cases checked,9 keys accepted,Backspace and letters ignored')
    finally:
        if emulator:
            emulator.close()


if __name__ == '__main__':
    main()
