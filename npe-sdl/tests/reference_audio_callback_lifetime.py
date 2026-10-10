"""Capture BIT89.3 across the original EOF callback's cleanup/start/return.

EOF is a diagnostic input. This measures unchanged IRQ and driver services,
not codec exhaustion, a game callback's file reopen, or physical audio time.
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
from test_np560_dtu import read_word, write_word


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--qemu', type=Path, required=True)
    args = parser.parse_args()
    out = Path('tmp/audio-callback-lifetime-native')
    out.mkdir(exist_ok=True)
    original = Path('tmp/NP560.PRG').read_bytes()
    image = bytearray(0x30000) + original
    resident = bytearray(image)
    resident[0x100d0:0x100d4] = struct.pack('<2H', 0xad08, 0x3b48)
    rom = out / 'original-services.rom'
    rom.write_bytes(resident)
    regions = [(0x839b6, 0x83ba4), (0x83262, 0x83314),
               (0x84702, 0x8479c)]
    cases = []
    for name, mute, reopen in [('normal-reopen', 0, True),
                               ('muted-reopen', 0x80, True),
                               ('callback-return', 0, False)]:
        code = [0x6800, 2, 0x68ff, 0x6000, 0x68e9, 0x8000, 0xee00,
                0x682e, 0, 0xfe00, 0x6810, 0,
                0x6831, 0x40, 0x6870, 3, 0x6871, 0xfffd, 0x6872, 1]
        for address, value in [(0xf010, 1), (0xf012, 0xf800),
                               (0xf014, mute), (0xf01a, 0),
                               (0xd0, 0xad08), (0xd2, 0x3b48),
                               (0x4aa8, 0x20), (0x4aaa, 0x1000),
                               (0x1200, 0), (0x1202, 0), (0x1204, 0)]:
            write_word(code, address, value)
        code += [0x68f1, 0x1234, 0x68f2, 0x2345, 0x68f3, 0x3456,
                 0x68f7, 0x789a, 0x0f34]
        read_word(code, 0xf012, 8)
        code += [0x68fc, 0x600d]
        pc = len(code) * 2
        code += [0xd0ff]
        body = [0x58f0, 0x58f1]

        def capture(destination):
            read_word(body, 0xf012, 0)
            body.extend([0x3f10, 0xa8f0, destination])

        capture(0x1200)
        if reopen:
            body += [0xbd08, 0x3262]
            capture(0x1202)
            body += [0xbd08, 0x4702]
            capture(0x1204)
        body += [0x381f, 0x380f, 0xc620]
        caller = bytearray(image[0x200000:])
        caller[:len(code) * 2] = struct.pack('<%dH' % len(code), *code)
        caller[0x1000:0x1000 + len(body) * 2] = struct.pack('<%dH' % len(body), *body)
        firmware = out / (name + '.prg')
        firmware.write_bytes(caller)
        em = None
        try:
            em = Emulator(qemu=args.qemu, firmware=firmware, soc_rom=rom,
                          soc_rom_skeleton=True, start_paused=True, icount_shift=0)
            for start, end in regions:
                actual = base64.b64decode(em.snapshot('code', start, end-start,
                                                       diagnostics=False)['frame'])
                assert actual == image[start:end], hex(start)
            em.qmp.execute('qom-set', {'path': '/machine',
                                      'property': 'instruction-clock', 'value': True})
            em.qmp.execute('cont')
            deadline = time.monotonic() + 12
            state = ''
            while time.monotonic() < deadline:
                state = em.qmp.execute('qom-get', {'path': '/machine', 'property': 'cpu-state'})
                if f'PC=20:{pc:04x}'.lower() in state.lower():
                    break
                time.sleep(.02)
            em.qmp.execute('stop')
            (out / (name + '.cpu.txt')).write_text(state, encoding='utf-8')
            assert f'PC=20:{pc:04x}'.lower() in state.lower(), state[:1500]
            assert 'ROM_MISSING=none' in state and 'UNSUPPORTED=none' in state, state[:1500]
            regs = {int(r): int(v, 16) for r, v in re.findall(
                r'\bR(\d+)\s*=([0-9a-f]{4})', '\n'.join(state.splitlines()[:5]), re.I)}
            assert [regs[r] for r in (1, 2, 3, 7, 12, 15)] == [
                0x1234, 0x2345, 0x3456, 0x789a, 0x600d, 0x6000], regs

            def word(address):
                return struct.unpack('<H', base64.b64decode(em.snapshot(
                    'data', address, 2, diagnostics=False)['frame']))[0]

            entry, cleanup, started = [word(a) for a in (0x1200, 0x1202, 0x1204)]
            assert entry == 0x8808
            assert (cleanup, started, regs[8]) == ((8, 8, 0) if reopen else (0, 0, 0x8800))
            timer = em.qmp.execute('qom-get', {'path': '/machine', 'property': 'timer2-state'})
            ticks = int(re.search(r'EXPIRED:(\d+)', timer)[1])
            assert ticks == (100 if mute else 300) if reopen else ticks == 50
            row = dict(name=name, mute=mute, reopen=reopen,
                       entry_flags89=entry, cleanup_flags89=cleanup,
                       started_flags89=started, returned_flags89=regs[8],
                       timer2_ticks=ticks, original_service_bytes_unchanged=True,
                       foreground_registers_restored=True, stack_restored=True,
                       host_guest_memory_writes=0, passed=True)
            cases.append(row)
            print(json.dumps(row), flush=True)
        finally:
            if em:
                em.close()
    report = dict(passed=len(cases) == 3, cases=cases,
                  original_prg_sha256=hashlib.sha256(original).hexdigest(),
                  qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),
                  measured_regions=[dict(start=start, end=end,
                      sha256=hashlib.sha256(image[start:end]).hexdigest()) for start, end in regions],
                  scope='original EOF callback activity across driver cleanup/start and IRQ return; diagnostic EOF and callback, no codec or game file reopen proof')
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
