"""Original map-name RAM and 28:1F08 parsing after a consumed NUL trail.

Use the unchanged original loader's existing 139/140 RAM snapshots. Warm tail
variants are isolated diagnostic inputs, not claims about natural game paths.
Original string/lookup/copy code runs unchanged; post-lookup adapters capture
glyphs and coordinates. This is not an original complete LCD reference.
"""
import argparse, base64, hashlib, json, re, struct, sys, time
from pathlib import Path

sys.path.insert(0, str(Path('scripts').resolve()))
from web.np560_web import Emulator

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--qemu', type=Path, required=True)
p.add_argument('--npe', type=Path, required=True)
args = p.parse_args()
out = Path('tmp/native-map-name-tail')
out.mkdir(parents=True, exist_ok=True)
original = Path('tmp/NP560.PRG').read_bytes()
npe = args.npe.read_bytes()
cases = []
for number in (139, 140):
    folder = Path('tmp/map-stream-native') / f'tianming-v13-map{number}-full'
    report = json.loads((folder/'report.json').read_text(encoding='utf-8'))
    assert report['passed'] and report['source_sha256'] == hashlib.sha256(npe).hexdigest()
    assert report['original_prg_sha256'] == hashlib.sha256(original).hexdigest()
    ram = (folder/'ram.bin').read_bytes()
    raw = ram[0x78e4:0x78e4+96]
    assert raw[12] == 0
    cases.append(dict(name=f'map{number}-loaded', map=number, raw=raw,
                      original_loader_ram=True))

base = cases[0]['raw']
for name, suffix in [('ascii', b'A\0'), ('gbk', b'\xb0\xa1\0'),
                     ('control', b'\x1e\0'),
                     ('clip-pair', b'A'*7+b'\xb0\xa1\0'),
                     ('no-nul', b'A'*83)]:
    raw = bytearray(base)
    raw[13:13+len(suffix)] = suffix
    cases.append(dict(name='map139-warm-'+name, map=139, raw=bytes(raw),
                      original_loader_ram=False))
raw = bytearray(cases[1]['raw'])
raw[13:16] = b'ABC'
cases.append(dict(name='map140-warm-ignored', map=140, raw=bytes(raw),
                  original_loader_ram=False))
image = bytearray(0x30000)+original

def install(address, words):
    image[address:address+2*len(words)] = struct.pack('<%dH' % len(words), *words)

# Capture at the same two post-lookup boundaries as reference_text16_bytes.
capture = [0x58f0,0x58f1,0x58f2,0x58f4,0x1f10,0x6810,0,
           0x88f1,0x1b02]
for offset in (6,8,10): capture += [0x780b,offset,0x1a10]
capture += [0x68f2,0x4844,0x484f]
repeat = 0x3a00+2*len(capture)
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

word(0x50b84c,0x0d00);word(0x50b84e,0)
long(0x51298c,0x540000)
long(0x512914,0x41850);word(0x512918,32)
word(0x51291a,16);word(0x51291c,16)
long(0x51291e,0x6800);word(0x512922,32)
word(0x512924,8);word(0x512926,16)
for i,cfg in enumerate(cases):
    raw = cfg['raw']
    size = raw.index(0)
    cfg.update(length=size,left=120-4*size,top=52)
    for j in range(0,len(raw),2): word(0x5278e4+j,int.from_bytes(raw[j:j+2],'big'))
    # Original strlen used by21:FE26. Capture its byte count, not glyph count.
    code += [0x68f0,0x78e4,0x58f0,0x68f0,0x52,0x58f0,
             0xbd31,0xaed0,0x50f4,0x6810,0,0xa8f1,0x1804+i*6]
    word(0x1b00,0);word(0x1b02,0x2000+i*0x400)
    code += [0x68f0,0x78e4,0x58f0,0x68f0,0x52,0x58f0,
             0x68f0,cfg['top'],0x58f0,0x68f0,cfg['left'],0x58f0,
             0xbd28,0x1f08,0x50f8,0x3f10,0xa8f0,0x1800+i*6,
             0x3f10,0x88f0,0x1b00,0x3f10,0xa8f0,0x1802+i*6]
code += [0x68fc,0x600d]
final_pc = 2*len(code)
code += [0xd0ff]
assert 2*len(code) < 0x1800
install(0x30000, code)
rom = out/'resident.rom';rom.write_bytes(image)
firmware = out/'caller.prg';firmware.write_bytes(image[0x30000:])
backing = out/'font-backing.bin';backing.write_bytes(original[0x30d000:0x370000])
em = None
try:
    em = Emulator(qemu=args.qemu,firmware=firmware,soc_rom=rom,
        soc_rom_skeleton=True,boot_mode='firmware-vma03',start_paused=True)
    em.load_data(0x540000,backing.resolve())
    for address,physical,size in [(0x281f08,0x451f08,0x314),
            (0x179244,0x349244,0xb5e),(0x28fd38,0x45fd38,0xfa),
            (0x18657a,0x35657a,0x90),(0x31aed0,0x4eaed0,0x44),
            (0x3307b0,0x5007b0,0x34)]:
        observed = base64.b64decode(em.snapshot('code',address,size,diagnostics=False)['frame'])
        assert observed == original[physical-0x200000:physical-0x200000+size],hex(address)
    em.qmp.execute('cont');deadline=time.monotonic()+20;state=''
    while time.monotonic()<deadline:
        state=em.qmp.execute('qom-get',{'path':'/machine','property':'cpu-state'})
        if f'PC=03:{final_pc:04x}'.lower() in state.lower():break
        time.sleep(.01)
    em.qmp.execute('stop');(out/'cpu.txt').write_text(state,encoding='utf-8')
    assert f'PC=03:{final_pc:04x}'.lower() in state.lower(),state[:1800]
    assert all(k+'=none' in state for k in ['ROM_MISSING','UNSUPPORTED','INVALID_CONTROL']),state
    registers={int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})',state,re.I)}
    assert registers[15] == 0x6000
    def memory(address,size):
        return base64.b64decode(em.snapshot('data',address,size,diagnostics=False)['frame'])
    for i,cfg in enumerate(cases):
        end_x,count,length = struct.unpack('<3H',memory(0x1800+i*6,6))
        assert length == cfg['length'] and count <= 24
        records = memory(0x2000+i*0x400,count*38)
        glyphs=[];expected=[];pos=0;x=cfg['left'];raw=cfg.pop('raw')
        while x<239 and raw[pos]:
            lead=raw[pos];width=8 if lead<128 else 16
            character=lead if width==8 else lead*256+raw[pos+1]
            if width==8:offset=0x313800+lead*32
            elif 0xa1<=lead<=0xfd and 0xa1<=raw[pos+1]<=0xfe:
                offset=0x343810+((lead-0xa1)*94+raw[pos+1]-0xa1)*32
            else:offset=0x343810
            bits=bytearray(original[offset:offset+32]);bits[0::2],bits[1::2]=bits[1::2],bits[0::2]
            expected.append(dict(x=x,y=52,code=character,width=width,bits_hex=bits.hex()))
            x+=width;pos+=width//8
        assert x==end_x and len(expected)==count,(cfg['name'],x,end_x,count)
        for j,wanted in enumerate(expected):
            row=records[j*38:(j+1)*38];gx,gy,character=struct.unpack_from('<3H',row)
            ascii_code=(character>>8)==(character&255) and (character&255)<128
            bits=bytearray(row[6:]);bits[0::2],bits[1::2]=bits[1::2],bits[0::2]
            sampled=dict(x=gx,y=gy,code=character&255 if ascii_code else character,
                         width=8 if ascii_code else 16,bits_hex=bits.hex())
            assert sampled==wanted,(cfg['name'],sampled,wanted)
            glyphs.append(sampled)
        pixels=bytearray(240*120)
        left=cfg['left']-5;right=240-cfg['left']+5
        for y in range(49,72):
            for xx in range(left+1,right):pixels[y*240+xx]=15 if xx in (left+1,right-1) or y in (49,71) else 5
        for g in glyphs:
            bits=bytes.fromhex(g['bits_hex'])
            for y in range(16):
                mask=int.from_bytes(bits[2*y:2*y+2],'big')
                for xx in range(g['width']):
                    if g['x']+xx<240 and mask&(0x8000>>xx):pixels[(52+y)*240+g['x']+xx]=15
        (out/(cfg['name']+'.idx')).write_bytes(pixels)
        (out/(cfg['name']+'.input')).write_bytes(struct.pack('<5H',cfg['map'],cfg['left'],52,end_x,length)+raw)
        cfg.update(end_x=end_x,glyphs=glyphs,ram_hex=raw.hex(),passed=True)
    report=dict(passed=True,source_sha256=hashlib.sha256(npe).hexdigest(),
        program_sha256=hashlib.sha256(original).hexdigest(),
        original_lcd_checked=False,cases=cases,
        scope='Original strlen, string/glyph parsing and coordinates; loader RAM inputs plus isolated warm-tail diagnostics. Composed glyph/window pixels, not complete original LCD or natural reachability.')
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(passed=True,cases=[dict(name=c['name'],length=c['length'],end_x=c['end_x'],glyphs=len(c['glyphs'])) for c in cases])))
finally:
    if em:em.close()
