"""Original NPC rich row formatting and fonts 8, 9, 23.

Resident lookup/copy and NPC formatter/row renderer execute unchanged. LCD,
row queue and commit boundaries are adapted; output is a glyph/coordinate
composition, not a full original LCD or natural game event reference.
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
from np560_assembler import _DMCUAssembler
import extract_np560_gbk_font as font_source
import extract_np560_npc_fonts as rich_fonts


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=Path('tmp/native-npc-rich-text'))
    args = parser.parse_args()
    out = args.output
    out.mkdir(parents=True, exist_ok=True)
    original = Path('tmp/NP560.PRG').read_bytes()
    nand = Path('tmp/NP560_NAND_IMAGE.bin').read_bytes()
    fonts = rich_fonts.extract(original)
    assert hashlib.sha256(nand).hexdigest() == font_source.NAND_SHA256
    source = Path('tmp/tjfy-world-probe.npe').read_bytes()
    (out/'game0.npe').write_bytes(source)
    samples = [
        ('font-6', bytes.fromhex('06200641067f068006fe06ff')),
        ('font-8', bytes.fromhex('0801080208200841087f088008fe08ff')),
        ('marked', b'\x07EFGHabc0123\x07Z'),
        ('marked-high', bytes.fromhex('07458141fea006410841075a')),
        ('mixed', b'A\x06B\x08C\x07EFG\x07\xb0\xa1D'),
        ('marked-wrap', b'\x07'+b'E'*35+b'\x07Z'),
        ('font-edge', b'A'*29+b'\x06B\x08CD'),
        ('marked-pair-edge', b'A'*28+b'\x07\x81\x41BC\x07D'),
        ('marked-newline', b'\x07EF\r\nGH\x07Z'),
        ('small-control-run', b'A\x01EF\x07B\x05GH\x07C'),
        ('zero-trail', b'A\x06\0B\x08\0C'),
        ('unclosed-mark', b'A\x07EFG'),
    ]
    assert all(len(raw) <= 61 and b'@' not in raw and b'{' not in raw
               for _,raw in samples)
    image = bytearray(0x30000) + original

    def install(address, words):
        image[address:address+len(words)*2] = struct.pack('<%dH' % len(words), *words)

    # Record x/y/code/font-kind plus the original 4844 buffer. Special fonts
    # use its first 16 logical bytes; D uses all 32. DS is explicit per access.
    capture = [0x58f0,0x58f1,0x58f2,0x58f4,0x1f10,0x6810,0,0x88f1,0x1b02,
               0x6810,0xaa]
    for offset in (6,8,10):
        capture += [0x6810,0,0x780b,offset,0x6810,0xaa,0x1a10]
    capture += [0x1a14,0x6810,0,0x68f2,0x4844]
    cap = _DMCUAssembler(0x33a00)
    cap.emit(*capture,0x564d)
    cap.ajmp('special',2)
    cap.emit(0x484f)
    cap.ajmp('copy')
    cap.label('special')
    cap.emit(0x4847)
    cap.label('copy')
    repeat = cap.pc & 65535
    cap.emit(0xaf04,repeat+16,0x3802,0x6810,0xaa,0x1a10,0x6810,0)
    cap.emit(0xa8f1,0x1b02,0x4801,0x90f0,0x1b00,0xa8f0,0x1b00,
             0x1f90,0x384f,0x382f,0x381f,0x380f,0xc620)
    install(0x33a00,cap.finish())
    install(0x179da2,[0x68f4,13,0xbd03,0x3a00,0xad17,0x9e0e])
    install(0x28fe32,[0x68f4,13,0xbd03,0x3a00,0xad28,0xfff0])
    install(0x2102dc,[0x6810,0xa1,0x99f4,0x384d,0xbd03,0x3a00,0xad21,0x0542])
    flash = [0x2e00,0x58f0,0x58f1,0x58f2,0x58f3,0x58f4,
             0x1f10,0x6810,0,0x780b,10,0x781b,12,0x782b,14,
             0x72f1,0xba9c,0x73f0,0x020e,0x58f2,0x58f1,0x58f0,
             0x784b,8,0x783b,6,0x58f4,0x58f3,0xbd18,0x657a,0x50fa,
             0x1f90,0x384f,0x383f,0x382f,0x381f,0x380f,0x2f00,0xc620]
    install(0x33c00,flash)
    install(0x2812a4,[0xad03,0x3c00])
    install(0x192960,[0xc620])
    install(0x319778,[0xc620])
    code = _DMCUAssembler(0x30000)
    code.emit(0x6800,2,0x68ff,0x6000,0x6810,0,0x6811,0)

    def word(address,value):
        code.emit(0x6810,address>>15,0x68f0,value,0xa8f0,address&0x7fff)

    def long(address,value):
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
    word(0x1b00,0)
    for kind in rich_fonts.FONT_TYPES:
        code.emit(0x68f0,kind,0x58f0,0xbd31,0x598a,0x50f2,0x68f8,0)
        code.label('font-%d' % kind)
        code.emit(0x58f8,0x68f0,84,0x58f0,0x4800,0x58f0,
                  0xbd21,0,0x50f6,0x5081,0x76f8,256)
        code.ajmp('font-%d' % kind,9)
    for i,(_,raw) in enumerate(samples):
        # Isolated caller stack, preserving the row buffer between lines.
        code.emit(0x6810,0,0x68f0,0,0x68f1,0x5e80,0x68f2,191)
        repeat = code.pc & 65535
        code.emit(0xaf02,repeat+6,0x1a10)
        padded = raw+b'\0\0'
        if len(padded)%2:
            padded += b'\0'
        for j in range(0,len(padded),2):
            word(0x521000+j,int.from_bytes(padded[j:j+2],'big'))
        word(0x1b00,0)
        for value in (0x1000,0x52,118,240,84,0):
            code.emit(0x68f0,value,0x58f0)
        code.emit(0xbd1f,0x2b72,0x50fc,0x3f10,0xa8f0,0x1800+i*4,
                  0x3f10,0x88f0,0x1b00,0x3f10,0xa8f0,0x1802+i*4)
    code.emit(0x68fc,0x600d)
    final_pc = code.pc & 65535
    code.emit(0xd0ff)
    assert code.pc < 0x33a00
    install(0x30000,code.finish())
    rom,firmware = out/'resident.rom',out/'caller.prg'
    rom.write_bytes(image)
    firmware.write_bytes(image[0x30000:])
    backing = out/'resident-backing.bin'
    backing.write_bytes(original[0x30d000:0x38a3f0])
    flash_backing = out/'flash-backing.bin'
    raw = bytearray(nand[font_source.EXTENDED12_SOURCE+font_source.NAND_SOURCE_DELTA:
                         font_source.EXTENDED_END+font_source.NAND_SOURCE_DELTA])
    raw[0::2],raw[1::2] = raw[1::2],raw[0::2]
    flash_backing.write_bytes(raw)
    em = None
    try:
        em = Emulator(qemu=args.qemu,firmware=firmware,soc_rom=rom,
                      soc_rom_skeleton=True,boot_mode='firmware-vma03',start_paused=True)
        em.load_data(0x540000,flash_backing.resolve())
        em.load_data(0x680000,backing.resolve())
        checked = [(0x1f2b72,0x3c2b72,0x61e),(0x26459c,0x43459c,0x39e),
                   (0x31598a,0x4e598a,0xba),(0x3337c6,0x5037c6,0x22),
                   (0x210000,0x3e0000,0x2dc),(0x327b42,0x4f7b42,0x66),
                   (0x179244,0x349244,0xb5e),(0x28fd38,0x45fd38,0xfa),
                   (0x18657a,0x35657a,0x90),(0x2f6bfa,0x4c6bfa,240)]
        for address,physical,size in checked:
            observed = base64.b64decode(em.snapshot('code',address,size,diagnostics=False)['frame'])
            assert observed == original[physical-0x200000:physical-0x200000+size],hex(address)
        em.qmp.execute('cont')
        deadline = time.monotonic()+25
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

        def records(address,count,maximum=40):
            data = memory(address,count*maximum)
            rows,offset = [],0
            for j in range(count):
                x,y,char,kind = struct.unpack_from('<4H',data,offset)
                size = 32 if kind == 13 else 16
                bits = bytearray(data[offset+8:offset+8+size])
                bits[0::2],bits[1::2] = bits[1::2],bits[0::2]
                rows.append(dict(x=x,y=y,code=char,font=kind,bits=bytes(bits)))
                offset += 8+size
            return rows,offset

        address = 0x551000
        direct,size = records(address,768,24)
        address += size
        actual_fonts = bytearray()
        for i,row in enumerate(direct):
            kind,code = rich_fonts.FONT_TYPES[i//256],i%256
            assert (row['font'],row['code'],row['x'],row['y']) == (kind,code,0,84),row
            assert row['bits'][:16] == fonts[i//256][code*16:(code+1)*16],(kind,code)
            actual_fonts.extend(row['bits'][:16])
        (out/'fonts.bin').write_bytes(actual_fonts)
        cases = []
        for i,(name,raw) in enumerate(samples):
            consumed,count = struct.unpack('<2H',memory(0x1800+i*4,4))
            assert count <= 64 and address+count*40 <= 0x558000
            glyphs,size = records(address,count)
            address += size
            pixels = bytearray([7])*(240*120)
            pixels[84*240:119*240] = bytes(35*240)
            for row in glyphs:
                special = row['font'] != 13
                code = row['code']
                width = 8 if special or code>>8 == code&255 and code&255 < 128 else 16
                bits = row.pop('bits')
                row.update(width=width,bits_hex=bits[:16 if special else 32].hex())
                for y in range(16):
                    mask = bits[y] << 8 if special else int.from_bytes(bits[y*2:y*2+2],'big')
                    for x in range(width):
                        if row['x']+x<240 and row['y']+y<120 and mask&(0x8000>>x):
                            pixels[(row['y']+y)*240+row['x']+x] = 15
            (out/(name+'.idx')).write_bytes(pixels)
            header = struct.pack('<9HI',0,0,84,240,118,0,len(raw)+1,0,len(raw),0)
            (out/(name+'.input')).write_bytes(header+raw)
            cases.append(dict(name=name,text_hex=raw.hex(),consumed=consumed,glyphs=glyphs))
        report = dict(passed=True,program_sha256=hashlib.sha256(original).hexdigest(),
                      fonts_sha256=hashlib.sha256(actual_fonts).hexdigest(),
                      direct_glyphs=768,cases=cases,original_complete_lcd_checked=False,
                      zero_caller_stack_per_sample=True,diagnostic_inputs_only=True,
                      scope=__doc__)
        (out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        print(json.dumps(dict(passed=True,direct_glyphs=768,pages=len(cases),
                              page_glyphs=sum(len(c['glyphs']) for c in cases))))
    finally:
        if em:
            em.close()


if __name__ == '__main__':
    main()
