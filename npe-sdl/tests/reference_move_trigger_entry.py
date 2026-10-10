"""Unchanged original 1D:0000 collision on actual NPE walking-trigger rows.

The caller supplies a diagnostic RAM map with other collisions empty. This
checks the fresh-entry flag and raw mode/event publication, not original LCD,
whole-map loading, encounter/chase continuations or natural game completion.
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


def triggers(source):
    data = source.read_bytes()
    base = struct.unpack_from('>I', data, 0x3a+8*8)[0]
    map_count = struct.unpack_from('>I', data, base)[0]//4
    sizes = [0, 13, 10, 7, 8, 10, 7, 13, 11, 9, 10, 7, 8, 8, 11, 7]
    limits = [3, 5, 5, 1, 5, 2, 3, 5, 4, 4, 2, 15, 5, 5, 10, 2]
    for index in range(map_count):
        record = base+struct.unpack_from('>I', data, base+index*4)[0]
        width, height = data[record+2:record+4]
        tail = record+17+width*height*2
        if tail+703 > len(data):
            continue
        counts = [min(data[tail+654+i*3], limits[i]) for i in range(16)]
        offset = tail+703+sum(sizes[i]*counts[i] for i in range(14))
        for slot in range(counts[14]):
            raw = data[offset+slot*11+2:offset+slot*11+11]
            position, = struct.unpack_from('>H', raw)
            mode, event = raw[2], struct.unpack_from('>H', raw, 3)[0]
            minimum, maximum = struct.unpack_from('>2H', raw, 5)
            if not width or position >= width*height:
                continue
            x, y = position % width, position//width
            if y < 2 or y >= height-1 or minimum > maximum:
                continue
            yield dict(source=str(source.resolve()), source_sha256=hashlib.sha256(data).hexdigest(),
                       map=index+1, slot=slot, raw=raw.hex(), width=width, height=height,
                       x=x, y=y-1, progress=minimum, mode=mode, event=event,
                       position=position, minimum=minimum, maximum=maximum)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', type=Path, required=True)
    parser.add_argument('--directory', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=Path('tmp/move-trigger-entry-native'))
    args = parser.parse_args()
    out = args.output
    out.mkdir(parents=True, exist_ok=True)
    selected = {}
    for source in sorted(args.directory.glob('*.npe')):
        for row in triggers(source):
            if row['mode'] in (2, 5, 6, 17) and row['mode'] not in selected:
                selected[row['mode']] = row
    assert 2 in selected and 5 in selected, selected.keys()
    # Include the naturally failing entry, rather than only a similar row.
    mine = next(row for row in triggers(args.directory/'我的世界.npe')
                if row['map'] == 1 and row['mode'] == 2 and row['event'] == 4)
    selected[2] = mine
    original = Path('tmp/NP560.PRG').read_bytes()
    assert hashlib.sha256(original).hexdigest() == 'd555609e03ece17f42c1881beeb88361572f18621161de67bc9dc765857089f5'
    image = bytearray(0x30000)+original
    rom = out/'original-collision.rom'
    rom.write_bytes(image)
    cases = []
    for mode, row in selected.items():
        for before_flag in (0, 0xa5a5):
            name = f'mode-{mode}-flag-{before_flag:04x}'
            code = [0x6800, 2, 0x68ff, 0x6000,
                    0x6810, 0xa4, 0x4810, 0x4800, 0xbf00, 0x3fff, 0x1a10]
            def put(address, value):
                code.extend([0x6810, address >> 15, 0x68f0, value, 0xa8f0, address & 0x7fff])
            # Byte direction0, screenX, screenY; offsets0. Collision itself
            # computes the target; no patch is made to its comparisons.
            put(0x5269d4, row['x'])
            put(0x5269d6, row['y'] << 8)
            put(0x527a56, row['width'] << 8)
            put(0x527a5a, row['height'] << 8)
            put(0x527a66, row['progress'])
            put(0x527934, 1)
            put(0x527936, 0xffff)
            put(0x527ab4, before_flag)
            for address, value in [(0x527490, row['event']), (0x527492, row['minimum']),
                                   (0x527494, row['maximum']), (0x527496, row['position']),
                                   (0x527498, row['mode'] << 8)]:
                put(address, value)
            code += [0xbd1d, 0, 0x68fc, 0x600d]
            pc = len(code)*2
            code += [0xd0ff]
            caller = bytearray(original)
            caller[:len(code)*2] = struct.pack('<%dH' % len(code), *code)
            firmware = out/(name+'.prg')
            firmware.write_bytes(caller)
            emulator = None
            try:
                emulator = Emulator(qemu=args.qemu, firmware=firmware, soc_rom=rom,
                                    soc_rom_skeleton=True, boot_mode='firmware-vma03', start_paused=True)
                code_regions = []
                for address, size in [(0x1d0000, 0x782), (0x3307b0, 0x80)]:
                    actual = base64.b64decode(emulator.snapshot('code', address, size, diagnostics=False)['frame'])
                    assert actual == image[address:address+size], hex(address)
                    code_regions.append(dict(address=address, size=size, sha256=hashlib.sha256(actual).hexdigest()))
                emulator.qmp.execute('cont')
                deadline = time.monotonic()+12
                state = ''
                while time.monotonic() < deadline:
                    state = emulator.qmp.execute('qom-get', {'path': '/machine', 'property': 'cpu-state'})
                    if f'PC=03:{pc:04x}'.lower() in state.lower():
                        break
                    time.sleep(.02)
                emulator.qmp.execute('stop')
                (out/(name+'.cpu.txt')).write_text(state, encoding='utf-8')
                assert f'PC=03:{pc:04x}'.lower() in state.lower(), state[:1500]
                assert all(marker+'=none' in state for marker in ['ROM_MISSING', 'UNSUPPORTED', 'INVALID_CONTROL']), state[:1500]
                registers = {int(k): int(v, 16) for k, v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})', '\n'.join(state.splitlines()[:5]), re.I)}
                assert registers[15] == 0x6000 and registers[12] == 0x600d and registers[0] == 0
                def word(address):
                    return struct.unpack('<H', base64.b64decode(emulator.snapshot('data', address, 2, diagnostics=False)['frame']))[0]
                after = dict(mode=word(0x527934), event=word(0x527936),
                             map_init=word(0x527ab4), collision_kind=word(0x527a86))
                assert after == dict(mode=mode, event=row['event'], map_init=1, collision_kind=0), after
                cases.append(dict(name=name, input=row, before_flag=before_flag, after=after,
                                  code_regions=code_regions, stack_restored=True, passed=True))
                (out/(name+'.path')).write_text(row['source']+'\n', encoding='utf-8')
                fields = [row['map'], row['x'], row['y'], row['progress'], before_flag,
                          after['mode'], after['event'], after['map_init']]
                (out/(name+'.txt')).write_text(' '.join(map(str, fields))+'\n', encoding='ascii')
                print(name, after, flush=True)
            finally:
                if emulator:
                    emulator.close()
    result = dict(passed=True, scope=__doc__, cases=cases,
                  original_prg_sha256=hashlib.sha256(original).hexdigest(),
                  qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),
                  original_lcd_checked=False, natural_path_injected=False)
    (out/'report.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')


if __name__ == '__main__':
    main()
