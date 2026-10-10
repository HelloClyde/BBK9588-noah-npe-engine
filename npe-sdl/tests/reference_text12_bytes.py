"""Sample original C-font parsing, glyphs and positions with six/eight ASCII pitch.

Only verified font backing/flash transport and post-lookup LCD adapters change.
The original string, glyph lookup, ASCII copy and packed glyph unpack execute.
Raster fixtures use sampled buffers: they are not original complete LCD frames.
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
from reference_extended_fonts import verify_flash


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', type=Path, required=True)
    parser.add_argument('--physical-nand', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=Path('tmp/native-text12-bytes'))
    args = parser.parse_args()
    out = args.output
    out.mkdir(parents=True, exist_ok=True)
    original = Path('tmp/NP560.PRG').read_bytes()
    nand = Path('tmp/NP560_NAND_IMAGE.bin').read_bytes()
    font_source.extract(original, nand)  # Authenticate both unchanged sources.
    blocks = verify_flash(nand, args.physical_nand)
    inputs = [
        ('ordinary', b'A\xb0\xa19'), ('invalid_trail', b'\xcb\x20A'),
        ('high80', b'\x80\x40A'), ('highff', b'\xff\x40A'),
        ('resident_holes', b'\xa1\x7f\xa7\x40A'), ('unpaired', b'\xcb'),
        ('zero_trail', b'\xcb\0A'), ('extended_first', b'\x81\x40\x81\x7f\xa0\xfe'),
        ('extended_second', b'\xa8\x40\xa8\xa0\xfe\xa0'),
        ('resident_end', b'\xfd\xfe\xfe\xa1A'), ('nul_stop', b'A\0B'),
        ('bad_edges', b'\x81\x3f\xa0\xffA'),
        ('resident_punctuation', b'\xa9\xfe\xaa\xa1\xaf\xa1\xb0\xa1'),
        ('empty', b''),
    ]
    controls = bytes(range(1, 32)) + b'\x7f'
    inputs += [('controls_' + str(i // 8), controls[i:i + 8])
               for i in range(0, len(controls), 8)]
    cases = [dict(name=name + '_pitch' + str(pitch), text_hex=raw.hex(),
                  pitch=pitch, left=16, top=16)
             for pitch in [6, 8] for name, raw in inputs]
    image = bytearray(0x30000) + original

    def install(address, words):
        image[address:address + len(words) * 2] = struct.pack('<%dH' % len(words), *words)

    capture = [0x58F0, 0x58F1, 0x58F2, 0x58F4, 0x1F10, 0x6810, 0,
               0x88F1, 0x1B02]
    for offset in [6, 8, 10]:
        capture += [0x780B, offset, 0x1A10]
    capture += [0x68F2, 0x4844, 0x484F]
    repeat = 0x3A00 + len(capture) * 2
    capture += [0xAF04, repeat + 8, 0x3802, 0x1A10]
    capture += [0xA8F1, 0x1B02, 0x4801, 0x90F0, 0x1B00, 0xA8F0, 0x1B00,
                0x1F90, 0x384F, 0x382F, 0x381F, 0x380F, 0xC620]
    install(0x33A00, capture)
    install(0x179DA2, [0xBD03, 0x3A00, 0xAD17, 0x9E0E])
    install(0x28FE32, [0xBD03, 0x3A00, 0xAD28, 0xFFF0])
    # Same transport relocation as the independently verified extended table.
    flash = [0x2E00, 0x58F0, 0x58F1, 0x58F2, 0x58F3, 0x58F4,
             0x1F10, 0x6810, 0, 0x780B, 10, 0x781B, 12, 0x782B, 14,
             0x88F4, 0x1B04, 0x1A40, 0x1A41, 0x1A42, 0xA8F4, 0x1B04,
             0x72F1, 0xBA9C, 0x73F0, 0x020E, 0x58F2, 0x58F1, 0x58F0,
             0x784B, 8, 0x783B, 6, 0x58F4, 0x58F3, 0xBD18, 0x657A,
             0x50FA, 0x1F90, 0x384F, 0x383F, 0x382F, 0x381F, 0x380F,
             0x2F00, 0xC620]
    install(0x2812A4, flash)
    code = [0x6800, 2, 0x68FF, 0x6000, 0x6810, 0, 0x6811, 0]

    def word(address, value):
        code.extend([0x6810, address >> 15, 0x68F0, value, 0xA8F0, address & 0x7FFF])

    def long(address, value):
        word(address, value >> 16)
        word(address + 2, value & 65535)

    word(0x50B84C, 0x0C00)
    word(0x50B84E, 0)
    long(0x51298C, 0x680000)
    long(0x512914, 0xED90)
    word(0x512918, 24)
    word(0x51291A, 12)
    word(0x51291C, 12)
    long(0x51291E, 0x2000)
    word(0x512922, 32)
    word(0x512926, 12)
    word(0x1B04, 0x0500)
    assert 0x2000 + len(cases) * 0x180 <= 0x5800
    for i, case in enumerate(cases):
        raw = bytes.fromhex(case['text_hex']) + b'\0\0'
        if len(raw) % 2:
            raw += b'\0'
        for j in range(0, len(raw), 2):
            word(0x521000 + j, int.from_bytes(raw[j:j + 2], 'big'))
        word(0x512924, case['pitch'])
        word(0x1B00, 0)
        word(0x1B02, 0x2000 + i * 0x180)
        code += [0x68F0, 0x1000, 0x58F0, 0x68F0, 0x52, 0x58F0,
                 0x68F0, case['top'], 0x58F0, 0x68F0, case['left'], 0x58F0,
                 0xBD28, 0x1F08, 0x50F8, 0x3F10, 0xA8F0, 0x1800 + i * 4,
                 0x3F10, 0x88F0, 0x1B00, 0x3F10, 0xA8F0, 0x1802 + i * 4]
    code += [0x68FC, 0x600D]
    final_pc = len(code) * 2
    code += [0xD0FF]
    assert len(code) * 2 < 0x3A00
    install(0x30000, code)
    rom = out / 'resident.rom'
    rom.write_bytes(image)
    firmware = out / 'caller.prg'
    firmware.write_bytes(image[0x30000:])
    backing = out / 'font-backing.bin'
    backing.write_bytes(original[0x30D000:font_source.FONT16_SOURCE + 94 * 94 * 32])
    raw = bytearray(nand[font_source.EXTENDED12_SOURCE + font_source.NAND_SOURCE_DELTA:
                         font_source.EXTENDED_END + font_source.NAND_SOURCE_DELTA])
    raw[0::2], raw[1::2] = raw[1::2], raw[0::2]
    flash_backing = out / 'flash-backing.bin'
    flash_backing.write_bytes(raw)
    emulator = None
    try:
        emulator = Emulator(qemu=args.qemu, firmware=firmware, soc_rom=rom,
                             soc_rom_skeleton=True, boot_mode='firmware-vma03', start_paused=True)
        emulator.load_data(0x680000, backing.resolve())
        emulator.load_data(0x540000, flash_backing.resolve())
        checked = [(0x281F08, 0x451F08, 0x314), (0x179244, 0x349244, 0xB5E),
                   (0x28FD38, 0x45FD38, 0xFA), (0x29C3C0, 0x46C3C0, 0x29A),
                   (0x18657A, 0x35657A, 0x90), (0x3307B0, 0x5007B0, 0x34)]
        for address, physical, size in checked:
            observed = base64.b64decode(emulator.snapshot('code', address, size, diagnostics=False)['frame'])
            assert observed == original[physical - 0x200000:physical - 0x200000 + size], hex(address)
        emulator.qmp.execute('cont')
        deadline = time.monotonic() + 30
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

        def memory(address, size):
            return base64.b64decode(emulator.snapshot('data', address, size, diagnostics=False)['frame'])

        fixture = bytearray(b'NPE12B01' + struct.pack('<I', len(cases)))
        for i, case in enumerate(cases):
            end_x, count = struct.unpack('<2H', memory(0x1800 + i * 4, 4))
            assert count * 38 <= 0x180
            sampled = memory(0x2000 + i * 0x180, count * 38) if count else b''
            glyphs, pos, x = [], 0, case['left']
            source = bytes.fromhex(case['text_hex']) + b'\0\0'
            pixels = bytearray(240 * 120)
            for j in range(count):
                record = sampled[j * 38:(j + 1) * 38]
                gx, gy, character = struct.unpack_from('<3H', record)
                lead, trail = source[pos], source[pos + 1]
                assert lead and (gx, gy) == (x, case['top'])
                advance = 1 if lead < 0x80 else 2
                assert character == (lead * 0x101 if advance == 1 else lead * 256 + trail)
                bits = bytearray(record[6:30])
                bits[0::2], bits[1::2] = bits[1::2], bits[0::2]
                if advance == 1:
                    wanted = font_source.resident_rows(original, font_source.ASCII12_SOURCE + lead * 32, 24)
                elif 0x81 <= lead <= 0xA0 and 0x40 <= trail <= 0xFE:
                    index = (lead - 0x81) * 191 + trail - 0x40
                    offset = font_source.EXTENDED12_SOURCE + font_source.NAND_SOURCE_DELTA + index * 18
                    wanted = font_source.unpack12(nand[offset:offset + 18])
                elif 0xA8 <= lead <= 0xFE and 0x40 <= trail <= 0xA0:
                    index = font_source.EXTENDED_FIRST_COUNT + (lead - 0xA8) * 97 + trail - 0x40
                    offset = font_source.EXTENDED12_SOURCE + font_source.NAND_SOURCE_DELTA + index * 18
                    wanted = font_source.unpack12(nand[offset:offset + 18])
                else:
                    valid = 0xA1 <= lead <= 0xFD and 0xA1 <= trail <= 0xFE
                    base = font_source.CHINESE12_SOURCE if valid and lead >= 0xB0 else font_source.PUNCTUATION12_SOURCE
                    index = (lead - 0xA1) * 94 + trail - 0xA1 if valid else 0
                    wanted = font_source.resident_rows(original, base + index * 24, 24)
                assert bits == wanted, (case['name'], hex(character), bits.hex(), wanted.hex())
                glyphs.append(dict(x=gx, y=gy, code=character, advance=advance, bits_hex=bits.hex()))
                for row in range(12):
                    mask = int.from_bytes(bits[2 * row:2 * row + 2], 'big')
                    for column in range(12):
                        if mask & (0x8000 >> column):
                            pixels[(gy + row) * 240 + gx + column] = 15
                x += case['pitch'] if advance == 1 else 12
                pos += advance
            assert source[pos] == 0 and end_x == x, (case['name'], end_x, x, pos)
            case.update(end_x=end_x, glyphs=glyphs)
            raw = bytes.fromhex(case['text_hex'])
            fixture += struct.pack('<6H', case['left'], case['top'], case['pitch'], len(raw), count, end_x)
            fixture += raw + pixels
        (out / 'strings.bin').write_bytes(fixture)
        report = dict(passed=True, original_lcd_checked=False, original_string_cases=len(cases),
                      original_glyphs=sum(len(case['glyphs']) for case in cases),
                      program_sha256=hashlib.sha256(original).hexdigest(),
                      nand_sha256=hashlib.sha256(nand).hexdigest(), flash_blocks=blocks,
                      checked_code=checked, explicit_zero_padding=2, cases=cases,
                      fixture_sha256=hashlib.sha256(fixture).hexdigest())
        (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
        print(f"Original C-font strings: {len(cases)} cases, {report['original_glyphs']} glyphs and positions passed")
    finally:
        if emulator:
            emulator.close()


if __name__ == '__main__':
    main()
