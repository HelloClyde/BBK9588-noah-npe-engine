"""Run the original NPC 1F:2B72 and story 29:E552 formatters and D lookup.

Real NPC page bytes come from six unchanged NPEs. Additional byte/layout inputs
are isolated diagnostics. Flash transport and post-lookup LCD output are adapted;
captured glyphs/coordinates are composed into pixels, not a complete native LCD.
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


def npc_page(source, npc, pair):
    section = struct.unpack_from('>I', source, 0x3a + 5 * 8)[0]
    count = source[section + (npc - 1) * 5]
    record = section + struct.unpack_from('>I', source, section + (npc - 1) * 5 + 1)[0]
    for case in range(count):
        for branch in range(3):
            pointer = section + struct.unpack_from('>I', source, record + case * 38 + 26 + branch * 4)[0]
            offset = 1  # Original NPC entry consumes its initial delimiter.
            for page in range(128):
                begin = offset
                raw = bytearray()
                more = 1
                while True:
                    ch = source[pointer + offset]
                    offset += 1
                    if ch == ord('@'):
                        more = 0
                        break
                    if ch == ord('{'):
                        if source[pointer + offset] == ord('@'):
                            more = 0
                        break
                    if len(raw) >= 61:
                        break  # This byte is consumed, then overwritten by NUL.
                    raw.append(ch)
                if pair in raw:
                    return dict(pointer=pointer, offset=begin, after=offset,
                                more=more, npc=npc, npc_case=case, branch=branch,
                                page=page, raw=bytes(raw))
                if not more:
                    break
    raise AssertionError((npc, pair.hex()))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--qemu', type=Path, required=True)
    p.add_argument('--corpus', type=Path, default=Path('D:/Downloads/NP560plus游戏全集'))
    p.add_argument('--output', type=Path, default=Path('tmp/native-dialogue-glyphs'))
    args = p.parse_args()
    out = args.output
    out.mkdir(parents=True, exist_ok=True)
    original = Path('tmp/NP560.PRG').read_bytes()
    nand = Path('tmp/NP560_NAND_IMAGE.bin').read_bytes()
    assert hashlib.sha256(original).hexdigest() == font_source.PROGRAM_SHA256
    assert hashlib.sha256(nand).hexdigest() == font_source.NAND_SHA256
    cases = []
    for i, (name, npc, pair) in enumerate([
            ('天机之江湖启示录', 155, b'\xf9\x2c'),
            ('新仙剑后传', 28, b'\xa1\x20'),
            ('天明游戏V1.0', 230, b'\xa6\x2c'),
            ('天明游戏V1.1', 230, b'\xa6\x2c'),
            ('天明游戏V1.2', 230, b'\xa6\x2c'),
            ('天明游戏V1.3', 168, b'\xf2\x2e')]):
        path = args.corpus / (name + '.npe')
        source = path.read_bytes()
        cfg = npc_page(source, npc, pair)
        (out / f'game{i}.npe').write_bytes(source)
        cfg.update(name=f'npc-{i}', file=i, source=str(path),
                   source_sha256=hashlib.sha256(source).hexdigest(), diagnostic=False)
        cases.append(cfg)
    for name, raw, rect in [
            ('extended', bytes.fromhex('8141817fa0fea841a87ffea0b0a1'), (0,84,240,118)),
            ('fallback', bytes.fromhex('f92ca120a62cf22efea141'), (0,84,240,118)),
            ('control-del', b'A\x1eB\x7fC\x80\x41D', (0,84,240,118)),
            ('pair-edge', b'A'*28+b'\x81\x41BC', (0,84,240,118)),
            ('low-trail-edge', b'A'*28+b'\xf9\x2cBCD', (0,84,240,118)),
            ('unpaired', b'A\xcb', (0,84,240,118)),
            ('story-extended', bytes.fromhex('8141a87ffea0f92c7f41'), (0,12,240,118)),
            ('bottom-clip', bytes.fromhex('8141fea041'), (16,110,96,131))]:
        cases.append(dict(name=name, raw=raw, rect=rect, file=0, pointer=0,
                          offset=0, after=len(raw)+1, more=0, diagnostic=True))
    for cfg in cases:
        cfg.setdefault('rect', (0,84,240,118))

    inputs = cases
    cases = [dict(cfg, kind=kind, input_name=cfg['name'],name=cfg['name']+'-'+kind)
             for index,cfg in enumerate(inputs) for kind in ('npc','story')
             if kind=='story' or index<12]
    image = bytearray(0x30000) + original

    def install(address, words):
        image[address:address+2*len(words)] = struct.pack('<%dH' % len(words), *words)

    capture = [0x58f0,0x58f1,0x58f2,0x58f4,0x1f10,0x6810,0,0x88f1,0x1b02,
               0x6810,0xa6]
    for offset in (6,8,10):
        capture += [0x6810,0,0x780b,offset,0x6810,0xa6,0x1a10]
    capture += [0x6810,0,0x68f2,0x4844,0x484f]
    repeat = 0x3a00+len(capture)*2
    capture += [0xaf04,repeat+16,0x3802,0x6810,0xa6,0x1a10,0x6810,0]
    capture += [0x6810,0,0xa8f1,0x1b02,0x4801,0x90f0,0x1b00,0xa8f0,0x1b00,
                0x1f90,0x384f,0x382f,0x381f,0x380f,0xc620]
    install(0x33a00, capture)
    install(0x179da2, [0xbd03,0x3a00,0xad17,0x9e0e])
    install(0x28fe32, [0xbd03,0x3a00,0xad28,0xfff0])
    # Original flash address -> diagnostic RAM, with original memcpy.
    flash = [0x2e00,0x58f0,0x58f1,0x58f2,0x58f3,0x58f4,
             0x1f10,0x6810,0,0x780b,10,0x781b,12,0x782b,14,
             0x72f1,0xba9c,0x73f0,0x020e,0x58f2,0x58f1,0x58f0,
             0x784b,8,0x783b,6,0x58f4,0x58f3,0xbd18,0x657a,0x50fa,
             0x1f90,0x384f,0x383f,0x382f,0x381f,0x380f,0x2f00,0xc620]
    install(0x33c00, flash)
    install(0x2812a4, [0xad03,0x3c00])
    # NPC row queue/commit boundaries do not alter byte layout or font lookup.
    # The original NPC row renderer26:459C still executes unchanged.
    install(0x192960,[0xc620])
    install(0x319778,[0xc620])
    code = [0x6800,2,0x68ff,0x6000,0x6810,0,0x6811,0]

    def word(address, value):
        code.extend([0x6810,address>>15,0x68f0,value,0xa8f0,address&0x7fff])

    def long(address, value):
        word(address,value>>16)
        word(address+2,value&65535)

    word(0x50b84c,0x0d00)
    word(0x50b84e,0xf100)
    long(0x51298c,0x680000)
    long(0x512914,0x41850)
    word(0x512918,32)
    word(0x51291a,16)
    word(0x51291c,16)
    long(0x51291e,0x6800)
    word(0x512922,32)
    word(0x512924,8)
    word(0x512926,16)
    word(0x1b02,0x1000)
    for i,cfg in enumerate(cases):
        # Explicit isolated zero stack input; preserve row scratch between the
        # original formatter's lines, but do not inherit a prior sample's stack.
        code += [0x6810,0,0x68f0,0,0x68f1,0x5e80,0x68f2,191]
        repeat = len(code)*2
        code += [0xaf02,repeat+6,0x1a10]
        raw = cfg['raw'] + b'\0\0'
        if len(raw)%2:
            raw += b'\0'
        for j in range(0,len(raw),2):
            word(0x521000+j,int.from_bytes(raw[j:j+2],'big'))
        word(0x1b00,0)
        # Six original words: left, top, right, bottom, string high/low.
        for value in (0x1000,0x52,*reversed(cfg['rect'])):
            code += [0x68f0,value,0x58f0]
        call = [0xbd1f,0x2b72] if cfg['kind']=='npc' else [0xbd29,0xe552]
        code += call+[0x50fc,0x3f10,0xa8f0,0x1800+i*4,
                 0x3f10,0x88f0,0x1b00,0x3f10,0xa8f0,0x1802+i*4]
    code += [0x68fc,0x600d]
    final_pc = len(code)*2
    code += [0xd0ff]
    assert len(code)*2 < 0x3a00
    install(0x30000,code)
    rom = out/'resident.rom'
    firmware = out/'caller.prg'
    rom.write_bytes(image)
    firmware.write_bytes(image[0x30000:])
    backing = out/'resident-backing.bin'
    backing.write_bytes(original[0x30d000:font_source.FONT16_SOURCE+94*94*32])
    raw = bytearray(nand[font_source.EXTENDED12_SOURCE+font_source.NAND_SOURCE_DELTA:
                         font_source.EXTENDED_END+font_source.NAND_SOURCE_DELTA])
    raw[0::2],raw[1::2] = raw[1::2],raw[0::2]
    flash_backing = out/'flash-backing.bin'
    flash_backing.write_bytes(raw)
    em = None
    try:
        em = Emulator(qemu=args.qemu,firmware=firmware,soc_rom=rom,
                      soc_rom_skeleton=True,boot_mode='firmware-vma03',start_paused=True)
        em.load_data(0x540000,flash_backing.resolve())
        em.load_data(0x680000,backing.resolve())
        checked = [(0x1f2b72,0x3c2b72,0x61e),(0x26459c,0x43459c,0x3a0),
                   (0x327b42,0x4f7b42,0x66),
                   (0x29e552,0x46e552,0x290),(0x281f08,0x451f08,0x314),
                   (0x179244,0x349244,0xb5e),(0x28fd38,0x45fd38,0xfa),
                   (0x18657a,0x35657a,0x90),(0x335c18,0x505c18,0xe)]
        for address,physical,size in checked:
            observed = base64.b64decode(em.snapshot('code',address,size,diagnostics=False)['frame'])
            assert observed == original[physical-0x200000:physical-0x200000+size],hex(address)
        em.qmp.execute('cont')
        deadline = time.monotonic()+25
        state = ''
        while time.monotonic()<deadline:
            state = em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
            if f'PC=03:{final_pc:04x}'.lower() in state.lower():
                break
            time.sleep(.01)
        em.qmp.execute('stop')
        (out/'cpu.txt').write_text(state,encoding='utf-8')
        assert f'PC=03:{final_pc:04x}'.lower() in state.lower(),state[:1800]
        assert all(k+'=none' in state for k in ['ROM_MISSING','UNSUPPORTED','INVALID_CONTROL']),state
        registers = {int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})',state,re.I)}
        assert registers[15] == 0x6000

        def memory(address,size):
            return base64.b64decode(em.snapshot('data',address,size,diagnostics=False)['frame'])

        record_address = 0x531000
        for i,cfg in enumerate(cases):
            consumed,count = struct.unpack('<2H',memory(0x1800+i*4,4))
            assert count <= 80
            assert record_address+count*38 < 0x538000
            records = memory(record_address,count*38)
            record_address += count*38
            pixels = bytearray([7])*(240*120)
            left,top,right,bottom = cfg['rect']
            for y in range(top,min(bottom+1,120)):
                for x in range(left,min(right+1,240)):
                    pixels[y*240+x] = 0
            glyphs = []
            for j in range(count):
                row = records[j*38:(j+1)*38]
                x,y,character = struct.unpack_from('<3H',row)
                ascii_code = character>>8 == character&255 and character&255 < 128
                bits = bytearray(row[6:])
                bits[0::2],bits[1::2] = bits[1::2],bits[0::2]
                width = 8 if ascii_code else 16
                glyphs.append(dict(x=x,y=y,code=character&255 if ascii_code else character,
                                   width=width,bits_hex=bits.hex()))
                for yy in range(16):
                    mask = int.from_bytes(bits[yy*2:yy*2+2],'big')
                    for xx in range(width):
                        if x+xx<240 and y+yy<120 and mask&(0x8000>>xx):
                            pixels[(y+yy)*240+x+xx] = 15
            raw = cfg.pop('raw')
            (out/(cfg['name']+'.idx')).write_bytes(pixels)
            header = struct.pack('<9HI',cfg['file'],*cfg['rect'],cfg['offset'],
                                 cfg['after'],cfg['more'],len(raw),cfg['pointer'])
            (out/(cfg['input_name']+'.input')).write_bytes(header+raw)
            cfg.update(text_hex=raw.hex(),consumed=consumed,glyphs=glyphs,passed=True)
        report = dict(passed=True,original_complete_lcd_checked=False,
                      program_sha256=hashlib.sha256(original).hexdigest(),
                      nand_sha256=hashlib.sha256(nand).hexdigest(),cases=cases,
                      zero_caller_stack_per_sample=True,
                      npc_row_queue_commit_adapted=True,
                      scope='Original NPC/story formatters and row string/glyph lookup; row queue/commit and flash/LCD boundaries adapted. Six original NPC page byte inputs and eight isolated diagnostics with explicit zero caller stack; glyph/coordinate composition, not original full LCD, warm stack or NPC condition/controller equivalence.')
        (out/'report.json').write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
        print(json.dumps(dict(passed=True,cases=len(cases),glyphs=sum(len(c['glyphs']) for c in cases),
                              samples=[dict(name=c['name'],consumed=c['consumed'],glyphs=len(c['glyphs'])) for c in cases])))
    finally:
        if em:
            em.close()


if __name__ == '__main__':
    main()
