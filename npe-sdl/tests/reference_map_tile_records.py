"""Original0C:D238..D584 tile-record loader; diagnostic file-byte adapter.

Includes declared pair counts beyond15, record overlap, signed indices and
live pointer/progress aliases. No original filesystem or LCD equivalence.
"""
import argparse, base64, hashlib, json, re, struct, sys, time
from pathlib import Path
sys.path.insert(0, str(Path('scripts').resolve()))
from web.np560_web import Emulator

p = argparse.ArgumentParser()
p.add_argument('--qemu', type=Path, required=True)
p.add_argument('--case')
p.add_argument('--grid', action='store_true')
args = p.parse_args()
out = Path('tmp/map-grid-native' if args.grid else 'tmp/map-tile-native'); out.mkdir(exist_ok=True)
original = Path('tmp/NP560.PRG').read_bytes()
cases = []
def record(threshold, pairs):
    return struct.pack('>HB', threshold, len(pairs)) + b''.join(
        struct.pack('>HH', index, tile) for index, tile in pairs)
def add(name, count, rows, progress=100, pointer=0x520000):
    cases.append(dict(name=name, count=count, payload=b''.join(rows),
                      progress=progress, pointer=pointer))
add('empty', 0, [])
add('present-empty', 1, [record(1, [])])
for n in (1, 15, 16, 31, 255):
    add('pairs-'+str(n), 1, [record(1, [(j+1, 0x1200+j) for j in range(n)])])
add('threshold-below', 1, [record(101, [(1, 0x1111), (2, 0x2222)])])
add('threshold-equal', 1, [record(100, [(1, 0x1111), (2, 0x2222)])])
add('zero-threshold', 1, [record(0, [(0, 0x1111), (1, 0x2222)])], progress=0)
add('signed-indices', 1, [record(1, [(0xffff, 0x1234), (0x8000, 0x2345),
                                   (0x7fff, 0x3456), (0, 0x4567)])])
for count in (2, 3, 4, 255):
    add('groups-'+str(count), count,
        [record(1, [(j+10, 0x2000+j)]) for j in range(min(count, 3))])
add('overlap-next-slot', 3, [record(1, [(j+20, 0x3000+j) for j in range(31)]),
                            record(2, [(3, 0x4444)]), record(3, [])])
add('alias-threshold', 1, [record(1, [(0x3927, 0xffff), (1, 0x2222)])])
add('alias-progress', 1, [record(100, [(0x3d33, 0), (1, 0x2222)])])
add('alias-pointer', 1, [record(1, [(0x3cc0, 0x4000), (1, 0x2222)])])
npe = Path('D:/Downloads/NP560plus游戏全集/天明游戏V1.3.npe').read_bytes()
cursor = 1664638+17+703; end = cursor
for i in range(2): end += 3+4*npe[end+2]
for progress in (1, 1280):
    add('map140-progress-'+str(progress), 2, [npe[cursor:end]],
        progress=progress, pointer=0x510000)

if args.grid:
    cases=[]
    def grid(name,width,height,words,pointer=0x520000,short=False):
        payload=b''.join(struct.pack('>H',v) for v in words)
        cases.append(dict(name=name,width=width,height=height,payload=payload,
                          count=0,progress=100,pointer=pointer,short=short))
    grid('empty',0,201,[])
    grid('one',1,1,[0x1234])
    grid('small',2,2,[0x1111,0x2222,0x3333,0x4444])
    grid('viewport',15,7,[0x1300+i for i in range(105)])
    grid('signed-product',255,255,[])
    grid('alias-clear-pointer',3,1,[0x1111,0x2222,0x3333],0x527980)
    grid('alias-width',3,1,[0x0100],0x527a56)
    grid('alias-height',3,1,[0],0x527a5a)
    grid('partial-read',2,1,[0x1234],short=True)
    cases[-1]['payload']+=b'\x56'
    grid('EOF',2,1,[],short=True)
    for number,name in [(138,'tianming-v13-map138-second'),(139,'tianming-v13-map139-full'),(140,'tianming-v13-map140-full')]:
        previous=Path('tmp/map-stream-native')/name
        r=json.loads((previous/'report.json').read_text(encoding='utf-8'))
        cases.append(dict(name='map'+str(number),width=r['width'],height=r['height'],
            payload=(previous/'grid.bin').read_bytes(),count=0,progress=100,pointer=0x510000,short=False))

rows = []
for cfg in cases:
    if args.case and cfg['name'] != args.case: continue
    root = out/cfg['name']; root.mkdir(exist_ok=True)
    (root/'payload.bin').write_bytes(cfg['payload'])
    image = bytearray(0x30000)+original
    def install(address, words):
        image[address:address+len(words)*2] = struct.pack('<%dH' % len(words), *words)
    body = [0x2e00, 0x1f10, 0x58f0]
    for i in range(5): body += [0x780b, 6+2*i, 0x3f10, 0xa8f0, 0x1410+2*i]
    body += [0x4801, 0x3f10, 0xa8f0, 0x4000]
    gate = 0x1000+len(body)*2
    body += [0x3f10, 0x88f0, 0x4000, 0x5600, 0xac02, gate,
             0x380f, 0x1f90, 0x2f00, 0xc620]
    install(0x31000, body); install(0x34010, [0xad03, 0x1000])
    install(0x31300, [0x3f10, 0x88f0, 0x4000, 0x5600, 0xac02, 0x1300, 0xc620])
    prefix = list(struct.unpack('<9H', original[0x9bdbe:0x9bdd0]))
    prefix += ([0xad0c,0xbf0c] if args.grid else
               [0x68f0, cfg['count'] << 8, 0x6a0b, 0xffea, 0xad0c, 0xd238])
    install(0x31200, prefix)
    install(0xcbfb4 if args.grid else 0xcd584, [0x3807, 0x3806, 0x3805, 0x3804, 0x3803,
                      0x3802, 0x3801, 0x3800, 0x2f00, 0xc620])
    code = [0x6800, 2, 0x68ff, 0x6000, 0x6811, 0]
    for address in range(0x510000, 0x530000, 0x8000):
        code += [0x6810, address >> 15, 0x4810, 0x68f0, 0xa55a,
                 0xbf00, 0x3fff, 0x1a10]
    def word(a, v):
        code.extend([0x6810, a >> 15, 0x68f0, v, 0xa8f0, a & 0x7fff])
    word(0x527a66, cfg['progress'])
    if args.grid:
        word(0x527a56,cfg['width']<<8|0x91)
        word(0x527a5a,cfg['height']<<8|0x92)
    word(0x52797e, cfg['pointer'] >> 16); word(0x527980, cfg['pointer'] & 65535)
    word(0x527992, 0x1234); word(0x527994, 0x5678)
    code += [0x68f0, 1, 0x3f10, 0xa8f0, 0x4000,
             0xbd03, 0x1300, 0xbd03, 0x1200, 0x68fc, 0x600d]
    pc = len(code)*2; code += [0xd0ff]; install(0x30000, code)
    rom = root/'resident.rom'; rom.write_bytes(image)
    firmware = root/'caller.prg'; firmware.write_bytes(image[0x30000:])
    em = None
    try:
        em = Emulator(qemu=args.qemu, firmware=firmware, soc_rom=rom,
                      soc_rom_skeleton=True, boot_mode='firmware-vma03', start_paused=True)
        region,size,offset=(0xcbf0c,0xa8,0x9bf0c) if args.grid else (0xcd238,0x34c,0x9d238)
        observed = base64.b64decode(em.snapshot('code', region, size,
                                    diagnostics=False)['frame'])
        assert observed == original[offset:offset+size]
        def raw(a, n): return base64.b64decode(em.snapshot('data', a, n, diagnostics=False)['frame'])
        def guest(a, n):
            first = a & ~1
            b = bytearray(raw(first, (n+(a&1)+1)&~1))
            b[0::2], b[1::2] = b[1::2], b[0::2]
            return bytes(b[a&1:(a&1)+n])
        def ram(): return b''.join(guest(a, 0x8000) for a in range(0x510000, 0x530000, 0x8000))
        def put(a, data):
            first = a & ~1; n = (len(data)+(a&1)+1)&~1
            b = bytearray(guest(first, n)); b[a&1:(a&1)+len(data)] = data
            b[0::2], b[1::2] = b[1::2], b[0::2]; em.write_data(first, b)
        em.qmp.execute('cont'); deadline = time.monotonic()+10
        while time.monotonic() < deadline:
            state = em.qmp.execute('qom-get', {'path':'/machine','property':'cpu-state'})
            if re.search(r'PC=03:13[0-9a-f]{2}', state, re.I): break
            time.sleep(.001)
        assert 'pc=03:13' in state.lower(), state[:1200]
        em.qmp.execute('stop'); (root/'before.bin').write_bytes(ram())
        em.write_data(0x4000, bytes(2)); em.qmp.execute('cont')
        cursor = 0; requests = []; deadline = time.monotonic()+10
        while time.monotonic() < deadline:
            state = em.qmp.execute('qom-get', {'path':'/machine','property':'cpu-state'})
            if f'PC=03:{pc:04x}'.lower() in state.lower(): break
            if re.search(r'PC=03:10[0-9a-f]{2}', state, re.I):
                em.qmp.execute('stop')
                if int.from_bytes(raw(0x4000, 2), 'little'):
                    values = struct.unpack('<5H', raw(0x1410, 10))
                    assert values[:2] == (0x1234, 0x5678)
                    a = values[2]*65536+values[3]; count = values[4]
                    data = cfg['payload'][cursor:cursor+count]
                    assert len(data) == count or cfg.get('short',False)
                    if data:put(a, data)
                    requests.append(dict(destination=a, count=count, cursor=cursor))
                    cursor += len(data); em.write_data(0x4000, bytes(2))
                    deadline = time.monotonic()+10
                em.qmp.execute('cont')
            time.sleep(.001)
        em.qmp.execute('stop'); (root/'cpu.txt').write_text(state, encoding='utf-8')
        assert f'PC=03:{pc:04x}'.lower() in state.lower(), state[:1400]
        assert all(k+'=none' in state for k in ('ROM_MISSING','UNSUPPORTED','INVALID_CONTROL')), state[:1400]
        assert 'r12=600d' in state.replace(' ', '').lower() and 'r15=6000' in state.replace(' ', '').lower()
        assert cursor == len(cfg['payload'])
        (root/'after.bin').write_bytes(ram())
        row = {k:v for k,v in cfg.items() if k != 'payload'}
        row.update(passed=True, bytes=cursor, requests=requests, stack_restored=True)
        (root/'report.json').write_text(json.dumps(row, indent=2)+'\n', encoding='utf-8')
        rows.append(row); print(cfg['name'], cursor, flush=True)
    finally:
        if em: em.close()
(out/'report.json').write_text(json.dumps(dict(passed=True, cases=rows,
    original_prg_sha256=hashlib.sha256(original).hexdigest(),
    qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),
    original_region=(dict(cpu=0xcbf0c,file=0x9bf0c,size=0xa8) if args.grid else
                     dict(cpu=0xcd238, file=0x9d238, size=0x34c)),
    scope='unchanged original grid loop, host file-byte adapter; no full loader/LCD proof' if args.grid else __doc__), indent=2)+'\n', encoding='utf-8')
