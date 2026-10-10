"""Execute original C/D glyph lookup and every original packed-12 unpack.

Only flash transport and the post-lookup LCD boundary are adapted. Flash bytes
are verified against the factory physical NAND and served unchanged from RAM.
Lookup arithmetic, resident copies and 29:C3C0 unpacking execute original code.
Captured glyphs/positions are a raster oracle, not an original LCD/timing claim.
"""
import argparse
import base64
import hashlib
import json
import mmap
import re
import struct
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path('scripts').resolve()))
sys.path.insert(0, str(Path('npe-sdl/tools').resolve()))
from web.np560_web import Emulator
from np560_assembler import _DMCUAssembler
import extract_np560_gbk_font as font_source


def verify_flash(source, physical):
    start, end = font_source.EXTENDED12_SOURCE, font_source.EXTENDED_END
    records = []
    with physical.open('rb') as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as data:
        _, header, page, oob, ppb, blocks, data_size, _, _, _ = struct.unpack_from('<6I2Q2I', data, 8)
        block_size = page * ppb
        tags = {}
        for block in range(blocks):
            tag = struct.unpack_from('<3H', data, header + data_size + (block * ppb + ppb - 1) * oob + 58)
            if len(set(tag)) == 1 and tag[0] != 0xFFFF:
                tags.setdefault(tag[0], []).append(block)
        pos = start
        while pos < end:
            tag = 0xE93 + pos // block_size
            candidates = tags[tag]
            assert len(candidates) == 1, (hex(tag), candidates)
            size = min(block_size - pos % block_size, end - pos)
            offset = header + candidates[0] * block_size + pos % block_size
            raw = bytearray(data[offset:offset + size])
            raw[0::2], raw[1::2] = raw[1::2], raw[0::2]
            source_offset = pos + font_source.NAND_SOURCE_DELTA
            assert raw == source[source_offset:source_offset + size], hex(tag)
            records.append(dict(address=pos, tag=tag, physical_block=candidates[0],
                                source_offset=source_offset, size=size))
            pos += size
    return records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', type=Path, required=True)
    parser.add_argument('--physical-nand', type=Path, required=True)
    parser.add_argument('--program', type=Path, default=Path('tmp/NP560.PRG'))
    parser.add_argument('--nand', type=Path, default=Path('tmp/NP560_NAND_IMAGE.bin'))
    parser.add_argument('--output', type=Path, default=Path('tmp/native-extended-fonts'))
    args = parser.parse_args()
    original, nand = args.program.read_bytes(), args.nand.read_bytes()
    table12, table16 = font_source.extract(original, nand)
    blocks = verify_flash(nand, args.physical_nand)
    out = args.output
    out.mkdir(parents=True, exist_ok=True)
    codes = [(lead, trail) for lead in [0x81, 0x82, 0x90, 0x9F, 0xA0]
             for trail in [0x40, 0x7E, 0x7F, 0x80, 0xFD, 0xFE]]
    codes += [(lead, trail) for lead in [0xA8, 0xA9, 0xB0, 0xCF, 0xFD, 0xFE]
              for trail in [0x40, 0x7E, 0x7F, 0x80, 0x9F, 0xA0]]
    resident = [(0xA9, 0xFE), (0xAA, 0xA1), (0xAF, 0xA1), (0xB0, 0xA1),
                (0xF7, 0xFE), (0xF8, 0xA1), (0xFD, 0xA1), (0xFD, 0xFE), (0xFE, 0xA1)]
    cases = [dict(height=height, code=lead * 256 + trail, extended=extended)
             for height in [12, 16]
             for extended, group in [(True, codes), (False, resident)]
             for lead, trail in group]
    image = bytearray(0x30000) + original

    def install(address, words):
        image[address:address + len(words) * 2] = struct.pack('<%dH' % len(words), *words)

    # Original glyph frame remains intact; capture x/y/code and 32 buffer bytes.
    capture = [0x58F0, 0x58F1, 0x58F2, 0x58F4, 0x1F10, 0x6810, 0,
               0x88F1, 0x1B02]
    for offset in [6, 8, 10]:
        capture += [0x780B, offset, 0x1A10]
    capture += [0x68F2, 0x4844, 0x484F]
    repeat = 0x3A00 + len(capture) * 2
    capture += [0xAF04, repeat + 8, 0x3802, 0x1A10]
    capture += [0xA8F1, 0x1B02, 0x1F90, 0x384F, 0x382F, 0x381F, 0x380F, 0xC620]
    install(0x33A00, capture)
    install(0x179DA2, [0xBD03, 0x3A00, 0xAD17, 0x9E0E])
    # Replace only 28:12A4 transport. Translate the original byte address to
    # diagnostic 54:0000, log the original address/count, use original memcpy.
    flash = [0x2E00, 0x58F0, 0x58F1, 0x58F2, 0x58F3, 0x58F4,
             0x1F10, 0x6810, 0, 0x780B, 10, 0x781B, 12, 0x782B, 14,
             0x88F4, 0x1B04, 0x1A40, 0x1A41, 0x1A42, 0xA8F4, 0x1B04,
             0x72F1, 0xBA9C, 0x73F0, 0x020E, 0x58F2, 0x58F1, 0x58F0,
             0x784B, 8, 0x783B, 6, 0x58F4, 0x58F3, 0xBD18, 0x657A,
             0x50FA, 0x1F90, 0x384F, 0x383F, 0x382F, 0x381F, 0x380F,
             0x2F00, 0xC620]
    install(0x33C00, flash)
    install(0x2812A4, [0xAD03, 0x3C00])
    code = _DMCUAssembler(0x30000)
    code.emit(0x6800, 2, 0x68FF, 0x6000, 0x6810, 0, 0x6811, 0)

    def word(address, value):
        code.emit(0x6810, address >> 15, 0x68F0, value, 0xA8F0, address & 0x7FFF)

    def long(address, value):
        word(address, value >> 16)
        word(address + 2, value & 65535)

    long(0x51298C, 0x680000)
    word(0x1B02, 0x2000)
    word(0x1B04, 0x0500)
    height = None
    for case in cases:
        if height != case['height']:
            height = case['height']
            word(0x50B84C, (0xC if height == 12 else 0xD) << 8)
            long(0x512914, 0xED90 if height == 12 else 0x41850)
            word(0x512918, height * 2)
            word(0x51291A, height)
            word(0x51291C, height)
        code.emit(0x68F0, case['code'], 0x58F0, 0x68F0, 16,
                  0x58F0, 0x58F0, 0xBD17, 0x9244, 0x50F6)
    # Decode every packed glyph with the unchanged original function. The
    # compact loop uses original memcpy before/after the internal 4844 buffer.
    code.emit(0x68F4, 0x54, 0x68F5, 0, 0x68F6, 0x60, 0x68F7, 0,
              0x68F8, font_source.EXTENDED_COUNT)
    code.label('unpack')
    code.emit(0x68F0, 18, 0x58F0, 0x58F5, 0x58F4, 0x68F0, 0x4844,
              0x58F0, 0x4800, 0x58F0, 0xBD18, 0x657A, 0x50FA,
              0xBD29, 0xC3C0, 0x68F0, 24, 0x58F0, 0x68F0, 0x4844,
              0x58F0, 0x4800, 0x58F0, 0x58F7, 0x58F6,
              0xBD18, 0x657A, 0x50FA, 0x70F5, 18, 0x71F4, 0,
              0x70F7, 24, 0x71F6, 0, 0x72F8, 1, 0x5680)
    code.ajmp('unpack', 2)
    code.emit(0x68FC, 0x600D)
    final_pc = code.pc & 65535
    code.emit(0xD0FF)
    assert code.pc < 0x33A00
    install(0x30000, code.finish())
    rom = out / 'resident.rom'
    rom.write_bytes(image)
    firmware = out / 'caller.prg'
    firmware.write_bytes(image[0x30000:])
    backing = out / 'resident-backing.bin'
    backing.write_bytes(original[0x30D000:font_source.FONT16_SOURCE + 94 * 94 * 32])
    raw = bytearray(nand[font_source.EXTENDED12_SOURCE + font_source.NAND_SOURCE_DELTA:
                         font_source.EXTENDED_END + font_source.NAND_SOURCE_DELTA])
    raw[0::2], raw[1::2] = raw[1::2], raw[0::2]
    flash_backing = out / 'flash-backing.bin'
    flash_backing.write_bytes(raw)
    em = None
    try:
        em = Emulator(qemu=args.qemu, firmware=firmware, soc_rom=rom,
                      soc_rom_skeleton=True, boot_mode='firmware-vma03', start_paused=True)
        em.load_data(0x540000, flash_backing.resolve())
        em.load_data(0x680000, backing.resolve())
        checked = [(0x179244, 0x349244, 0xB5E), (0x29C3C0, 0x46C3C0, 0x29A),
                   (0x18657A, 0x35657A, 0x90), (0x306490, 0x4D6490, 0x40),
                   (0x3307B0, 0x5007B0, 0x34)]
        for address, physical, size in checked:
            observed = base64.b64decode(em.snapshot('code', address, size, diagnostics=False)['frame'])
            assert observed == original[physical - 0x200000:physical - 0x200000 + size], hex(address)
        em.qmp.execute('cont')
        deadline = time.monotonic() + 55
        state = ''
        while time.monotonic() < deadline:
            state = em.qmp.execute('qom-get', {'path': '/machine', 'property': 'cpu-state'})
            if f'PC=03:{final_pc:04x}'.lower() in state.lower():
                break
            time.sleep(.02)
        em.qmp.execute('stop')
        (out / 'cpu.txt').write_text(state, encoding='utf8')
        assert f'PC=03:{final_pc:04x}'.lower() in state.lower(), state[:1800]
        assert all(key + '=none' in state for key in ['ROM_MISSING', 'UNSUPPORTED', 'INVALID_CONTROL']), state
        registers = {int(r): int(v, 16) for r, v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})', state, re.I)}
        assert registers[15] == 0x6000

        def memory(address, size):
            return base64.b64decode(em.snapshot('data', address, size, diagnostics=False)['frame'])

        records = memory(0x2000, len(cases) * 38)
        requests = memory(0x0500, len(codes) * 2 * 6)
        request_index = 0
        fixture = bytearray(b'NPEXT001' + struct.pack('<I', len(cases)))
        for i, case in enumerate(cases):
            record = records[i * 38:(i + 1) * 38]
            x, y, character = struct.unpack_from('<3H', record)
            assert (x, y, character) == (16, 16, case['code']), (i, x, y, character, case)
            bits = bytearray(record[6:6 + case['height'] * 2])
            bits[0::2], bits[1::2] = bits[1::2], bits[0::2]
            lead, trail = character >> 8, character & 255
            if case['extended']:
                first = lead <= 0xA0
                index = ((lead - 0x81) * 191 + trail - 0x40 if first else
                         font_source.EXTENDED_FIRST_COUNT + (lead - 0xA8) * 97 + trail - 0x40)
                source = (font_source.EXTENDED12_SOURCE if case['height'] == 12 else
                          font_source.EXTENDED16_SOURCE) + index * (18 if case['height'] == 12 else 32)
                high, low, length = struct.unpack_from('<3H', requests, request_index * 6)
                assert (high * 65536 + low, length) == (source, 18 if case['height'] == 12 else 32)
                case['flash_address'] = source
                request_index += 1
            if case['height'] == 12:
                index = 95 + (lead - 0x81) * 191 + trail - 0x40
                wanted = table12[8 + index * 24:8 + (index + 1) * 24]
            else:
                if case['extended']:
                    index += 95 + 94 * 94 + 33
                else:
                    index = 95 + ((lead - 0xA1) * 94 + trail - 0xA1 if lead <= 0xFD else 0)
                wanted = table16[8 + index * 32:8 + (index + 1) * 32]
            assert bits == wanted, (case, bits.hex(), wanted.hex())
            case['bits_hex'] = bits.hex()
            fixture += struct.pack('<4H', case['height'], x, y, character) + bits + bytes(32 - len(bits))
        decoded = bytearray()
        size = font_source.EXTENDED_COUNT * 24
        for offset in range(0, size, 65536):
            decoded.extend(memory(0x600000 + offset, min(65536, size - offset)))
        decoded[0::2], decoded[1::2] = decoded[1::2], decoded[0::2]
        for index, (lead, trail) in enumerate(font_source.extended_codes()):
            slot = 95 + (lead - 0x81) * 191 + trail - 0x40
            wanted = table12[8 + slot * 24:8 + (slot + 1) * 24]
            assert decoded[index * 24:(index + 1) * 24] == wanted, (index, hex(lead * 256 + trail))
        (out / 'glyphs.bin').write_bytes(fixture)
        (out / 'unpacked12.bin').write_bytes(decoded)
        extended16 = nand[font_source.EXTENDED16_SOURCE + font_source.NAND_SOURCE_DELTA:
                          font_source.EXTENDED_END + font_source.NAND_SOURCE_DELTA]
        (out / 'extended16.bin').write_bytes(extended16)
        report = dict(passed=True, original_lcd_checked=False, adapted_flash_transport=True,
                      original_lookup_cases=len(cases), original_unpack_glyphs=font_source.EXTENDED_COUNT,
                      program_sha256=hashlib.sha256(original).hexdigest(),
                      nand_sha256=hashlib.sha256(nand).hexdigest(), flash_blocks=blocks,
                      unpacked12_sha256=hashlib.sha256(decoded).hexdigest(), cases=cases)
        (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
        print(f'{len(cases)} original glyph lookups and {font_source.EXTENDED_COUNT} original 12-row unpacks passed')
    finally:
        if em:
            em.close()


if __name__ == '__main__':
    main()
