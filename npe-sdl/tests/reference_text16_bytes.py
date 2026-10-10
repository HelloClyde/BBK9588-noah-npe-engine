"""Original 28:1F08 string parsing and resident 16-row glyph lookup.

Font backing bytes are copied unchanged from PRG into diagnostic RAM; the
configured pointers are relocated together. The glyph lookup/string code is
unchanged. Two post-lookup adapters capture glyph arguments and 32 raster bytes
instead of executing LCD drawing. This supplies glyph/position evidence, not
an original full LCD frame, font startup or hardware timing reference.
"""
import argparse, base64, hashlib, json, re, struct, sys, time
from pathlib import Path
sys.path.insert(0, str(Path('scripts').resolve()))
from web.np560_web import Emulator

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--qemu', type=Path, required=True)
p.add_argument('--npe', type=Path, required=True)
p.add_argument('--name', default='native-text16-bytes')
args = p.parse_args()
out = Path('tmp') / args.name
out.mkdir(parents=True, exist_ok=True)
original = Path('tmp/NP560.PRG').read_bytes()
source = args.npe.read_bytes()
section = struct.unpack_from('>I', source, 0x3a+8*8)[0]
cases = []
for number in [139, 140]:
    start = section+struct.unpack_from('>I', source, section+4*(number-1))[0]
    name = source[start+4:start+16].split(b'\0')[0]
    cases.append(dict(name='map'+str(number), text_hex=name.hex(),
                      left=120-4*len(name), top=52, map=number))
for name, raw in [('ordinary', b'RPG\xa3\xad\xb0\xa1'),
                  ('control', b'\x1eA\x7f'), ('unpaired', b'\xcb'),
                  ('invalid_pair', b'\xcb\x20A'), ('high80', b'\x80\x40A'),
                  ('lead_fe', b'\xfe\xa1A'), ('empty', b'')]:
    cases.append(dict(name=name, text_hex=raw.hex(), left=16, top=16))
image = bytearray(0x30000)+original

def install(address, words):
    image[address:address+len(words)*2] = struct.pack('<%dH' % len(words), *words)

# Both native glyph functions retain their own prologue/epilogue. The capture
# adapter borrows the native R11 frame, preserves registers/DS and copies words.
capture = [0x58f0,0x58f1,0x58f2,0x58f4,0x1f10,0x6810,0,
           0x88f1,0x1b02]
for offset in [6,8,10]: capture += [0x780b,offset,0x1a10]
capture += [0x68f2,0x4844,0x484f]
repeat = 0x3a00+len(capture)*2
capture += [0xaf04,repeat+8,0x3802,0x1a10]
capture += [0xa8f1,0x1b02,0x4801,0x90f0,0x1b00,0xa8f0,0x1b00,
            0x1f90,0x384f,0x382f,0x381f,0x380f,0xc620]
install(0x33a00, capture)
install(0x179da2, [0xbd03,0x3a00,0xad17,0x9e0e])
install(0x28fe32, [0xbd03,0x3a00,0xad28,0xfff0])
code = [0x6800,2,0x68ff,0x6000,0x6810,0,0x6811,0]

def word(address, value):
    code.extend([0x6810,address>>15,0x68f0,value,0xa8f0,address&0x7fff])

def long(address, value):
    word(address,value>>16);word(address+2,value&65535)

# Resident font D: original offsets from the resident font base 50:D000.
# Only its backing base is relocated to diagnostic external RAM 54:0000.
word(0x50b84c,0x0d00);word(0x50b84e,0)
long(0x51298c,0x540000)
long(0x512914,0x41850);word(0x512918,32)
word(0x51291a,16);word(0x51291c,16)
long(0x51291e,0x6800);word(0x512922,32)
word(0x512924,8);word(0x512926,16)
for i,cfg in enumerate(cases):
    # Two explicit zero bytes after a bounded input cover a trailing lead plus
    # terminator, then stop the native parser. They are diagnostic RAM padding.
    raw=bytes.fromhex(cfg['text_hex'])+b'\0\0'
    if len(raw)%2: raw+=b'\0'
    for j in range(0,len(raw),2): word(0x521000+j,int.from_bytes(raw[j:j+2],'big'))
    word(0x1b00,0);word(0x1b02,0x2000+i*0x400)
    code += [0x68f0,0x1000,0x58f0,0x68f0,0x52,0x58f0,
             0x68f0,cfg['top'],0x58f0,0x68f0,cfg['left'],0x58f0,
             0xbd28,0x1f08,0x50f8,0x3f10,0xa8f0,0x1800+i*4,
             0x3f10,0x88f0,0x1b00,0x3f10,0xa8f0,0x1802+i*4]
code += [0x68fc,0x600d]
final_pc=len(code)*2;code += [0xd0ff]
install(0x30000,code)
rom=out/'resident.rom';rom.write_bytes(image)
firmware=out/'caller.prg';firmware.write_bytes(image[0x30000:])
backing=out/'font-backing.bin';backing.write_bytes(original[0x30d000:0x370000])
em=None
try:
    em=Emulator(qemu=args.qemu,firmware=firmware,soc_rom=rom,
                soc_rom_skeleton=True,boot_mode='firmware-vma03',start_paused=True)
    em.load_data(0x540000,backing.resolve())
    for address, physical, size in [(0x281f08,0x451f08,0x314),
            (0x179244,0x349244,0xb5e),(0x28fd38,0x45fd38,0xfa),
            (0x18657a,0x35657a,0x90),(0x3307b0,0x5007b0,0x34)]:
        observed=base64.b64decode(em.snapshot('code',address,size,diagnostics=False)['frame'])
        assert observed==original[physical-0x200000:physical-0x200000+size],hex(address)
    em.qmp.execute('cont');deadline=time.monotonic()+20;state=''
    while time.monotonic()<deadline:
        state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
        if f'PC=03:{final_pc:04x}'.lower() in state.lower():break
        time.sleep(.01)
    em.qmp.execute('stop');(out/'cpu.txt').write_text(state,encoding='utf8')
    assert f'PC=03:{final_pc:04x}'.lower() in state.lower(),state[:1800]
    assert all(k+'=none' in state for k in ['ROM_MISSING','UNSUPPORTED','INVALID_CONTROL']),state
    regs={int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})',state,re.I)}
    assert regs[15]==0x6000
    def raw_memory(address,size):
        return base64.b64decode(em.snapshot('data',address,size,diagnostics=False)['frame'])
    for i,cfg in enumerate(cases):
        end_x,count=struct.unpack('<2H',raw_memory(0x1800+i*4,4))
        assert count<20
        records=raw_memory(0x2000+i*0x400,count*38) if count else b''
        glyphs=[]
        for j in range(count):
            row=records[j*38:(j+1)*38];x,y,character=struct.unpack_from('<3H',row)
            bits=bytearray(row[6:]);bits[0::2],bits[1::2]=bits[1::2],bits[0::2]
            # The ASCII function stores its low argument byte into the high
            # stack byte too. Preserve the sampled word and normalize the code.
            ascii_code=(character>>8)==(character&255) and (character&255)<128
            glyphs.append(dict(x=x,y=y,argument_word=character,
                               code=character&255 if ascii_code else character,
                               width=8 if ascii_code else 16,
                               bits_hex=bits.hex()))
        cfg.update(end_x=end_x,glyphs=glyphs)
        expected=[];raw=bytes.fromhex(cfg['text_hex'])+b'\0\0';pos=0;x=cfg['left']
        while raw[pos]:
            lead=raw[pos];width=8 if lead<128 else 16
            character=lead if width==8 else lead*256+raw[pos+1]
            if width==8: offset=0x313800+lead*32
            elif 0xa1<=lead<=0xfd and 0xa1<=raw[pos+1]<=0xfe:
                offset=0x343810+((lead-0xa1)*94+raw[pos+1]-0xa1)*32
            else: offset=0x343810
            bits=bytearray(original[offset:offset+32])
            bits[0::2],bits[1::2]=bits[1::2],bits[0::2]
            expected.append(dict(x=x,y=cfg['top'],code=character,width=width,bits_hex=bits.hex()))
            x+=width;pos+=width//8
        assert end_x==x and len(expected)==len(glyphs),(cfg['name'],end_x,x)
        for sampled,wanted in zip(glyphs,expected):
            assert all(sampled[k]==v for k,v in wanted.items()),(cfg['name'],sampled,wanted)
        pixels=bytearray(240*120)
        for g in glyphs:
            bits=bytes.fromhex(g['bits_hex'])
            for y in range(16):
                mask=int.from_bytes(bits[2*y:2*y+2],'big')
                for x in range(g['width']):
                    if mask&(0x8000>>x):pixels[(g['y']+y)*240+g['x']+x]=15
        (out/(cfg['name']+'.idx')).write_bytes(pixels)
        (out/(cfg['name']+'.input')).write_bytes(struct.pack('<2H',cfg['left'],cfg['top'])+
                                               bytes.fromhex(cfg['text_hex']))
    report=dict(passed=True,source=str(args.npe.resolve()),
        source_sha256=hashlib.sha256(source).hexdigest(),
        program_sha256=hashlib.sha256(original).hexdigest(),
        original_lcd_checked=False,diagnostic_font_backing=True,
        explicit_zero_padding=2,cases=cases)
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    print(json.dumps(dict(passed=True,cases=[dict(name=c['name'],end_x=c['end_x'],
          glyphs=[hex(g['code']) for g in c['glyphs']]) for c in cases])))
finally:
    if em:em.close()
