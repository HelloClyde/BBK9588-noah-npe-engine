"""Execute unmodified C/D string, glyph and framebuffer drawing at the edges.

Resident font bytes are relocated unchanged. The original renderer writes the
entire explicitly seeded RAM display buffer; no glyph/LCD drawing adapters.
This does not validate LCD transfer, physical timing or natural game routes.
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
sys.path.insert(0, str(Path('npe-sdl/tools').resolve()))
from web.np560_web import Emulator
import extract_np560_gbk_font as font_source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=Path('tmp/native-font-edges'))
    args = parser.parse_args()
    out = args.output
    out.mkdir(parents=True, exist_ok=True)
    original = Path('tmp/NP560.PRG').read_bytes()
    assert hashlib.sha256(original).hexdigest() == font_source.PROGRAM_SHA256
    shapes = [
        ('ordinary', 16, 16, b'A\xb0\xa19'),
        ('right_chinese', 231, 16, b'\xb0\xa1A'),
        ('last_chinese', 238, 16, b'\xb0\xa1A'),
        ('right_ascii', 237, 16, b'WZ'),
        ('last_ascii', 238, 16, b'AZ'),
        ('right_guard', 239, 16, b'W\xb0\xa1'),
        ('outside_x', 240, 16, b'W\xb0\xa1'),
        ('bottom_ascii', 16, 118, b'W\xb0\xa1'),
        ('bottom_guard', 16, 119, b'W\xb0\xa1'),
        ('bottom_chinese', 16, 112, b'\xb0\xa1A'),
        ('last_chinese_row', 16, 118, b'\xb0\xa1A'),
        ('both_edges', 237, 112, b'\xb0\xa1'),
        ('negative_x', 65535, 16, b'AB'),
        ('negative_y', 16, 65535, b'ABC'),
        ('negative_chinese_x', 65535, 16, b'\xb0\xa1A'),
        ('negative_chinese_y', 16, 65535, b'\xb0\xa1A'),
        ('nul_stop', 16, 16, b'A\0B'),
        ('zero_trail', 16, 16, b'\xcb\0A'),
        ('empty', 16, 16, b''),
    ]
    shapes.append(('foreground_zero', 16, 16, b'A\xb0\xa19'))
    cases = [dict(name=f'{name}_font{height}_pitch{pitch}', height=height, pitch=pitch,
                  left=x, top=y, color=0 if name == 'foreground_zero' else 15,
                  text_hex=raw.hex())
             for height, pitch in [(12, 6), (12, 8), (16, 8)]
             for name, x, y, raw in shapes]
    assert len(cases) == 60 and 0x560000 + len(cases) * 0x4000 < 0x680000
    image = bytearray(0x30000) + original
    code = [0x6800, 2, 0x68FF, 0x6000, 0x6810, 0, 0x6811, 0]

    def word(address, value):
        code.extend([0x6810, address >> 15, 0x68F0, value, 0xA8F0, address & 0x7FFF])

    def long(address, value):
        word(address, value >> 16)
        word(address + 2, value & 65535)

    # Native display pointer, transparent drawing and verified resident font base.
    code += [0x6811, 0, 0x68F0, 0x54, 0xA8F0, 0x84FC, 0x4800, 0xA8F0, 0x84FE]
    long(0x51298C, 0x680000)
    word(0x50B84E, 0)
    word(0x4BB0, 1)
    word(0x4BAE, 0)
    previous = None
    for i, case in enumerate(cases):
        setup = (case['height'], case['pitch'])
        if setup != previous:
            height, pitch = setup
            word(0x50B84C, (0xC if height == 12 else 0xD) << 8)
            long(0x512914, 0xED90 if height == 12 else 0x41850)
            word(0x512918, height * 2)
            word(0x51291A, height)
            word(0x51291C, height)
            long(0x51291E, 0x2000 if height == 12 else 0x6800)
            word(0x512922, 32)
            word(0x512924, pitch)
            word(0x512926, height)
            word(0x4B1E, height)
            word(0x4B20, height)
            previous = setup
        code += [0x6810, 0xA8, 0x4810, 0x68F0, 0x7777, 0xBF00, 7199, 0x1A10]
        word(0x4BA8, case['color'])
        raw = bytes.fromhex(case['text_hex']) + b'\0\0'
        if len(raw) % 2:
            raw += b'\0'
        for j in range(0, len(raw), 2):
            word(0x521000 + j, int.from_bytes(raw[j:j + 2], 'big'))
        code += [0x68F0, 0x1000, 0x58F0, 0x68F0, 0x52, 0x58F0,
                 0x68F0, case['top'], 0x58F0, 0x68F0, case['left'], 0x58F0,
                 0xBD28, 0x1F08, 0x50F8, 0x3F10, 0xA8F0, 0x1800 + i * 2]
        # Copy the complete original packed framebuffer after each call.
        address = 0x560000 + i * 0x4000
        code += [0x68F0, 14400, 0x58F0, 0x4800, 0x58F0,
                 0x68F0, 0x54, 0x58F0, 0x68F0, address & 65535, 0x58F0,
                 0x68F0, address >> 16, 0x58F0, 0xBD18, 0x657A, 0x50FA]
    code += [0x68FC, 0x600D]
    final_pc = len(code) * 2
    code += [0xD0FF]
    assert len(code) * 2 < 0x8000
    image[0x30000:0x30000 + len(code) * 2] = struct.pack('<%dH' % len(code), *code)
    rom, firmware = out / 'resident.rom', out / 'caller.prg'
    rom.write_bytes(image)
    firmware.write_bytes(image[0x30000:])
    backing = out / 'font-backing.bin'
    backing.write_bytes(original[0x30D000:font_source.FONT16_SOURCE + 94 * 94 * 32])
    emulator = None
    try:
        emulator = Emulator(qemu=args.qemu, firmware=firmware, soc_rom=rom,
                             soc_rom_skeleton=True, boot_mode='firmware-vma03', start_paused=True)
        emulator.load_data(0x680000, backing.resolve())
        checked = [(0x281F08, 0x451F08, 0x314), (0x179244, 0x349244, 0xBDC),
                   (0x28FD38, 0x45FD38, 0x2C8), (0x186454, 0x356454, 0x286),
                   (0x18657A, 0x35657A, 0x90), (0x3307B0, 0x5007B0, 0x34)]
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
        fixture = bytearray(b'NPEFED01' + struct.pack('<I', len(cases)))
        for i, case in enumerate(cases):
            packed = bytearray(base64.b64decode(emulator.snapshot('data', 0x560000 + i * 0x4000,
                                             14400, diagnostics=False)['frame']))
            packed[0::2], packed[1::2] = packed[1::2], packed[0::2]
            pixels = bytes((packed[(119 - y) * 120 + x // 2] >> (0 if x & 1 else 4)) & 15
                           for y in range(120) for x in range(240))
            end_x = struct.unpack('<H', base64.b64decode(emulator.snapshot('data', 0x1800 + i * 2,
                                                  2, diagnostics=False)['frame']))[0]
            case.update(end_x=end_x, changed_pixels=sum(value != 7 for value in pixels),
                        framebuffer_sha256=hashlib.sha256(packed).hexdigest(),
                        pixels_sha256=hashlib.sha256(pixels).hexdigest())
            if case['name'].startswith(('right_guard', 'outside_x', 'bottom_guard', 'empty')):
                assert case['changed_pixels'] == 0, case
            elif case['name'].startswith('ordinary'):
                assert case['changed_pixels'] > 0, case
            raw = bytes.fromhex(case['text_hex'])
            fixture += struct.pack('<8H', case['height'], case['pitch'], case['left'], case['top'],
                                   case['color'], len(raw), end_x, 7) + raw + pixels
        (out / 'frames.bin').write_bytes(fixture)
        report = dict(passed=True, original_framebuffer_cases=len(cases), original_lcd_transport_checked=False,
                      draw_adapters=False, font_backing_relocated=True,
                      explicit_zero_padding=2, checked_code=checked, cases=cases,
                      program_sha256=hashlib.sha256(original).hexdigest(),
                      fixture_sha256=hashlib.sha256(fixture).hexdigest())
        (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
        print(f"Original font renderer: {len(cases)} complete packed framebuffer outputs captured; no drawing adapters")
    finally:
        if emulator:
            emulator.close()


if __name__ == '__main__':
    main()
