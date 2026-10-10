"""Original mode18 numeric service for zero-distance NPC moves.

CN1916884A PDF page28 defines DIV-by-zero as setting V. The numeric
service never uses its quotient when the signed step count is zero.
Run unchanged runtime1F:8BA2..9170 with its original LINK/save prefix.
Exclude the entry map renderer; ROM03:4224 is a diagnostic RET stub for
the drawing-plane selector. No movement/drawing loop may be entered.
This is a model-assisted state check, not physical hardware or original LCD.
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', type=Path, required=True)
    parser.add_argument('--xab', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=Path('tmp/npc-zero-div/native'))
    args = parser.parse_args()
    out = args.output
    out.mkdir(parents=True, exist_ok=True)
    original = Path('tmp/NP560.PRG').read_bytes()
    assert hashlib.sha256(original).hexdigest() == 'd555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5'
    image = bytearray(0x30000)+original
    data = args.xab.read_bytes()
    base = struct.unpack_from('>I', data, 0x3a+8*8)[0]
    record = base+struct.unpack_from('>I', data, base)[0]
    tail = record+17+data[record+2]*data[record+3]*2
    counts = [data[tail+654+i*3] for i in range(16)]
    offset = tail+703+counts[1]*13
    raw = data[offset:offset+10]
    assert counts[2] == 1 and raw.hex() == '00000017001515000000'
    actual = dict(name='xab-map1-event1', slot=6, npc=struct.unpack_from('>H', raw, 2)[0],
                  x=raw[5], y=raw[6], input_event=1, next_mode=raw[7], next_id=struct.unpack_from('>H', raw, 8)[0])
    cases = [actual,
             dict(name='first-slot', slot=0, npc=14, x=6, y=9, input_event=1, next_mode=1, next_id=77),
             dict(name='patrolling-slot-fifth-cache', slot=8, npc=2, x=255, y=255, input_event=261, next_mode=18, next_id=2)]
    # Only the excluded drawing-plane selector is stubbed. All numeric
    # service and math instructions are the bytes from the original PRG.
    image[0x34224:0x34226] = struct.pack('<H', 0xc620)
    rom = out/'original-numeric.rom'
    rom.write_bytes(image)
    rows = []
    for case in cases:
        code = [0x6800, 2, 0x68ff, 0x6000,
                0x6810, 0xa4, 0x4810, 0x4800, 0xbf00, 0x3fff, 0x1a10]
        def put(address, value):
            code.extend([0x6810, address >> 15, 0x68f0, value, 0xa8f0, address & 0x7fff])
        put(0x527934, 18)
        put(0x527936, case['input_event'])
        put(0x527ab4, 0)
        for slot in range(20):
            put(0x526ca2+14*slot, case['npc'] if slot == case['slot'] else 100+slot)
        # MOVB uses high-first byte lanes; snapshot('data') returns the
        # backing little-endian words. Direction precedes X, then Y.
        put(0x526caa+14*case['slot'], (3 << 8)+case['x'])
        put(0x526cac+14*case['slot'], case['y'] << 8)
        cache = 0x527652+((case['input_event'] & 255)-1)*10
        for delta, value in [(0, case['npc']), (2, case['next_mode'] << 8), (4, case['next_id']),
                             (6, case['x']), (8, case['y'])]:
            put(cache+delta, value)
        code += [0xbd03, 0x1000, 0x68fc, 0x600d]
        pc = len(code)*2
        code += [0xd0ff]
        caller = bytearray(original)
        caller[:len(code)*2] = struct.pack('<%dH' % len(code), *code)
        wrapper = image[0x1f8b8c:0x1f8b9e]+struct.pack('<2H', 0xad1f, 0x8ba2)
        caller[0x1000:0x1000+len(wrapper)] = wrapper
        caller[0x4224:0x4226] = struct.pack('<H', 0xc620)
        firmware = out/(case['name']+'.prg')
        firmware.write_bytes(caller)
        emulator = None
        try:
            emulator = Emulator(qemu=args.qemu, firmware=firmware, soc_rom=rom,
                                soc_rom_skeleton=True, boot_mode='firmware-vma03', start_paused=True)
            regions = []
            for address, size in [(0x1f8ba2, 0x5d0), (0x331724, 0x2e), (0x3307b0, 0x80)]:
                actual_bytes = base64.b64decode(emulator.snapshot('code', address, size, diagnostics=False)['frame'])
                assert actual_bytes == image[address:address+size], hex(address)
                regions.append(dict(address=address, size=size, sha256=hashlib.sha256(actual_bytes).hexdigest()))
            emulator.qmp.execute('cont')
            deadline = time.monotonic()+8
            state = ''
            while time.monotonic() < deadline:
                state = emulator.qmp.execute('qom-get', {'path': '/machine', 'property': 'cpu-state'})
                if f'PC=03:{pc:04x}'.lower() in state.lower():
                    break
                time.sleep(.02)
            emulator.qmp.execute('stop')
            (out/(case['name']+'.cpu.txt')).write_text(state, encoding='utf-8')
            assert f'PC=03:{pc:04x}'.lower() in state.lower(), state[:1800]
            assert all(marker+'=none' in state for marker in ['ROM_MISSING', 'UNSUPPORTED', 'INVALID_CONTROL']), state[:1800]
            registers = {int(k): int(v, 16) for k, v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})', '\n'.join(state.splitlines()[:5]), re.I)}
            assert registers[15] == 0x6000 and registers[12] == 0x600d
            control = next(line for line in state.splitlines() if line.startswith('CONTROL_TRACE='))
            assert control.count('1f:90ca/bd03>03:4224') == 1, control
            assert all(marker not in control for marker in ['1f:8f7e', '1f:8f9a', '1f:9096']), control
            def memory(address, size):
                return base64.b64decode(emulator.snapshot('data', address, size, diagnostics=False)['frame'])
            def word(address):
                return struct.unpack('<H', memory(address, 2))[0]
            npc_bytes = memory(0x526caa+case['slot']*14, 4)
            after = dict(mode=word(0x527934), event=word(0x527936), map_init=word(0x527ab4),
                         direction=npc_bytes[1], x=npc_bytes[0], y=npc_bytes[3])
            assert after == dict(mode=case['next_mode'], event=case['next_id'], map_init=1,
                                 direction=0, x=case['x'], y=case['y']), after
            rows.append(dict(input=case, after=after, code_regions=regions, stack_restored=True, passed=True))
            fields = [case['slot'], case['npc'], case['x'], case['y'], case['input_event'],
                      case['next_mode'], case['next_id'], after['direction'], after['map_init']]
            (out/(case['name']+'.case')).write_text(' '.join(map(str, fields))+'\n', encoding='ascii')
            print(case['name'], after, flush=True)
        finally:
            if emulator:
                emulator.close()
    result = dict(passed=True, scope=__doc__, cases=rows, xab_move_record=raw.hex(),
                  original_prg_sha256=hashlib.sha256(original).hexdigest(),
                  qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),
                  xab_sha256=hashlib.sha256(data).hexdigest(),
                  patent_source='https://patents.google.com/patent/CN1916884A/zh', patent_pdf_page=28,
                  physical_hardware_checked=False, original_lcd_checked=False,
                  excluded_services=['entry renderer2F:6AB8', 'drawing-plane selector03:4224'])
    (out/'report.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')


if __name__ == '__main__':
    main()
