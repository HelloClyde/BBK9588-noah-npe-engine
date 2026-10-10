"""Original DAC stop/start state, including 31AC's first-word target.

The transfer-start entry 24:9A1E is an explicit RETS boundary so no physical
DTU races the captured DAC state. Original DAC, Timer2 and cleanup services
are unchanged. No codec, NPE route, host RAM injection or audible-time claim.
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

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--qemu', type=Path, required=True)
parser.add_argument('--output', type=Path, default=Path('tmp/audio-dac-state-native'))
args = parser.parse_args()
out = args.output
out.mkdir(parents=True, exist_ok=True)
original = Path('tmp/NP560.PRG').read_bytes()
assert hashlib.sha256(original).hexdigest() == 'd555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5'
rows = []
configs = [
    # name, driver flags, mute, first PCM word, saved L/R, live L/R, calls
    ('first-positive', 4, 0, 5, [17, -17], [3, -3], [0x31ac]),
    ('first-negative', 4, 0, -4, [17, -17], [3, -3], [0x31ac]),
    ('preserved-targets', 0x14, 0, 9, [3, -2], [3, -3], [0x31ac]),
    ('muted-first', 4, 0x80, 9, [17, -17], [3, -3], [0x31ac]),
    ('muted-preserved', 0x14, 0x80, 9, [3, -2], [3, -3], [0x31ac]),
    ('stop-opposite', 1, 0, 5, [17, -17], [3, -3], [0x3262]),
    ('stop-muted', 1, 0x80, 5, [17, -17], [3, -3], [0x3262]),
    ('idle-stop', 4, 0, 5, [17, -17], [3, -3], [0x3262]),
    ('stop-first-start', 1, 0, -2, [17, -17], [3, -3], [0x3262, 0x31ac]),
    ('resume-targets', 2, 0, 9, [3, -2], [3, -3], [0x320e]),
]
for name, flags88, flags8a, first, saved, current, calls in configs:
    image = bytearray(0x30000) + original
    # Clear backend boundary only; the tested original routines are checked
    # byte-for-byte on CODE before execution.
    image[0x249a1e:0x249a20] = struct.pack('<H', 0xc620)
    image[0x249b02:0x249b04] = struct.pack('<H', 0xc620)
    code = [0x6800, 2, 0x68ff, 0x6000, 0x68e9, 0x8000, 0xee00,
            0x682e, 0, 0xfe00, 0x6810, 0, 0x6831, 0x2a40,
            0x6870, current[0] & 65535, 0x6871, current[1] & 65535, 0x6872, 1]
    for address, value in [(0xf010, flags88), (0xf012, 0), (0xf014, flags8a),
                           (0x4a68, saved[0] & 65535), (0x4a6a, saved[1] & 65535),
                           (0x1784, first & 65535)]:
        write_word(code, address, value)
    for call in calls:
        code += [0xbd08, call]
    code += [0x1f31, 0x385f, 0x68f4, 0x600d]
    pc = len(code) * 2
    code += [0xd0ff]
    caller = bytearray(image[0x200000:])
    caller[:len(code) * 2] = struct.pack('<%dH' % len(code), *code)
    rom, firmware = out / (name + '.rom'), out / (name + '.prg')
    rom.write_bytes(image)
    firmware.write_bytes(caller)
    em = None
    try:
        em = Emulator(qemu=args.qemu, firmware=firmware, soc_rom=rom,
                      soc_rom_skeleton=True, start_paused=True, icount_shift=0)
        em.qmp.execute('qom-set', {'path': '/machine', 'property': 'instruction-clock', 'value': True})
        for address, size in [(0x831ac, 0x62), (0x8320e, 0x28), (0x83262, 0x92),
                              (0x833fa, 0xca), (0x84702, 0x100)]:
            actual = base64.b64decode(em.snapshot('code', address, size, diagnostics=False)['frame'])
            assert actual == image[address:address + size], hex(address)
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
        assert f'PC=20:{pc:04x}'.lower() in state.lower(), state[:1400]
        assert all(marker + '=none' in state for marker in ['ROM_MISSING', 'UNSUPPORTED', 'INVALID_CONTROL']), state[:1400]
        regs = {int(k): int(v, 16) for k, v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})', state, re.I)}
        assert regs[15] == 0x6000 and regs[4] == 0x600d

        def word(address):
            return struct.unpack('<H', base64.b64decode(em.snapshot('data', address, 2, diagnostics=False)['frame']))[0]

        packet = em.audio()
        timer = em.qmp.execute('qom-get', {'path': '/machine', 'property': 'timer2-state'})
        signed = lambda v: v if v < 32768 else v - 65536
        after = dict(current=list(map(signed, packet['registers'][:2])),
                     saved=[signed(word(0x4a68)), signed(word(0x4a6a))],
                     control=packet['registers'][2],
                     gpio=regs[5],
                     flags88=word(0xf010), flags8a=word(0xf014),
                     ticks=int(re.search(r'EXPIRED:(\d+)', timer)[1]))
        rows.append(dict(name=name, flags88=flags88, flags8a=flags8a, first=first,
                         saved=saved, current=current, gpio=0x2a40, control=1,
                         calls=calls, after=after, passed=True))
        print(name, after, flush=True)
    finally:
        if em:
            em.close()
report = dict(passed=len(rows) == len(configs), cases=rows, scope=__doc__,
              original_prg_sha256=hashlib.sha256(original).hexdigest(),
              qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest())
(out / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
