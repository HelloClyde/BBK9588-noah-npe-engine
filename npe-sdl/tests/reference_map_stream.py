"""Run the original map directory lookup and loader against current NPE bytes.

03:4010/401C are diagnostic file adapters, paused at every request. The
directory, header/grid and (in full cases) table expansion run unchanged.
This proves requested offsets/counts, not original NAND/SPI/short-read timing.
"""
import argparse, base64, hashlib, json, re, struct, sys, time
from pathlib import Path
sys.path.insert(0, str(Path('scripts').resolve()))
from web.np560_web import Emulator

parser = argparse.ArgumentParser()
parser.add_argument('--qemu', type=Path, required=True)
parser.add_argument('--npe', type=Path, required=True)
parser.add_argument('--map', type=int, required=True)
parser.add_argument('--name', required=True)
parser.add_argument('--grid-only', action='store_true')
args = parser.parse_args()
out = Path('tmp/map-stream-native') / args.name
out.mkdir(parents=True, exist_ok=True)
original = Path('tmp/NP560.PRG').read_bytes()
npe = args.npe.read_bytes()
section, section_size = struct.unpack_from('>II', npe, 0x3a + 8 * 8)
start = section + struct.unpack_from('>I', npe, section + 4 * (args.map - 1))[0]
directory_words = struct.unpack_from('>I', npe, section)[0] // 4
nominal_end = section + section_size
if args.map < directory_words:
    next_offset = struct.unpack_from('>I', npe, section + 4 * args.map)[0]
    if next_offset != 0xffffffff: nominal_end = section + next_offset
image = bytearray(0x30000) + original

def install(address, words):
    raw = struct.pack('<%dH' % len(words), *words)
    image[address:address + len(raw)] = raw

# Each adapter captures the untouched caller arguments and waits for the host
# to supply real file bytes. Registers and DS are restored before RETS.
for operation, target in [(1, 0x1000), (2, 0x1100)]:
    body = [0x2e00, 0x1f10, 0x58f0]
    for i in range(5):
        body += [0x780b, 6 + 2*i, 0x3f10, 0xa8f0, 0x1410 + 2*i]
    body += [0x4800 | operation, 0x3f10, 0xa8f0, 0x4000]
    gate = target + len(body)*2
    body += [0x3f10, 0x88f0, 0x4000, 0x5600, 0xac02, gate,
             0x380f, 0x1f90, 0x2f00, 0xc620]
    install(0x30000 + target, body)
install(0x3401c, [0xad03, 0x1000])
install(0x34010, [0xad03, 0x1100])

# The grid-only diagnostic stops immediately after the original grid loop.
# Its continuation is an explicit boundary; it does not run bad tail records.
if args.grid_only:
    install(0xcbfb4, [0x68fc, 0x600d, 0xd0ff])
code = [0x6800, 2, 0x68ff, 0x6000, 0x6810, 0, 0x6811, 0]
for address in range(0x510000, 0x530000, 0x8000):
    code += [0x6810, address >> 15, 0x4810, 0x4800, 0xbf00, 0x3fff, 0x1a10]
def word(address, value):
    code.extend([0x6810, address >> 15, 0x68f0, value, 0xa8f0, address & 0x7fff])
def long(address, value):
    word(address, value >> 16); word(address + 2, value & 65535)
long(0x5279d6, section)
word(0x527a58, args.map)
long(0x52797e, 0x510000)
long(0x527992, 0x12345678)
code += [0xbd31, 0x76be, 0x58f1, 0x58f0, 0xbd0c, 0xbdbe,
         0x50f4, 0x68fc, 0x600d]
final_pc = len(code)*2
code += [0xd0ff]
install(0x30000, code)
rom = out / 'resident.rom'; rom.write_bytes(image)
firmware = out / 'caller.prg'; firmware.write_bytes(image[0x30000:])
em = None
try:
    em = Emulator(qemu=args.qemu, firmware=firmware, soc_rom=rom,
                  soc_rom_skeleton=True, boot_mode='firmware-vma03', start_paused=True)
    def raw_memory(address, size):
        data = bytearray()
        for i in range(0, size, 0x8000):
            data.extend(base64.b64decode(em.snapshot('data', address+i,
                        min(0x8000, size-i), diagnostics=False)['frame']))
        return bytes(data)
    def guest_memory(address, size):
        first = address & ~1
        raw = bytearray(raw_memory(first, (size + (address & 1) + 1) & ~1))
        raw[0::2], raw[1::2] = raw[1::2], raw[0::2]
        return bytes(raw[address & 1:(address & 1)+size])
    def put_guest(address, data):
        # Preserve the adjacent guest byte for odd requests.
        first = address & ~1; count = (len(data) + (address & 1) + 1) & ~1
        raw = bytearray(guest_memory(first, count))
        raw[address & 1:(address & 1)+len(data)] = data
        raw[0::2], raw[1::2] = raw[1::2], raw[0::2]
        em.write_data(first, raw)
    # Verify the entire executed loader; the grid-only end patch is excluded.
    regions = [(0x3176be, 0x4e76be, 0xb4), (0xcbdbe, 0x29bdbe,
                0x1f6 if args.grid_only else 0x349a),
               (0x253082, 0x423082, 0x20a), (0x3307b0, 0x5007b0, 0x34)]
    for address, physical, size in regions:
        observed = base64.b64decode(em.snapshot('code', address, size, diagnostics=False)['frame'])
        assert observed == original[physical-0x200000:physical-0x200000+size], hex(address)
    em.qmp.execute('cont'); cursor = 0; requests = []; state = ''
    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        state = em.qmp.execute('qom-get', {'path':'/machine', 'property':'cpu-state'})
        if ('PC=0C:bfb8' if args.grid_only else f'PC=03:{final_pc:04x}').lower() in state.lower():
            break
        if re.search(r'PC=03:1[01][0-9a-f]{2}', state, re.I):
            em.qmp.execute('stop')
            operation = int.from_bytes(raw_memory(0x4000, 2), 'little')
            if operation:
                values = struct.unpack('<5H', raw_memory(0x1410, 10))
                handle = values[0]*65536 + values[1]
                assert handle == 0x12345678, values
                position = values[2]*65536 + values[3]
                if operation == 1:
                    assert values[4] == 0
                    cursor = position
                    requests.append(dict(operation='seek', cursor=cursor))
                else:
                    count = values[4]; data = npe[cursor:cursor+count]
                    if data: put_guest(position, data)
                    requests.append(dict(operation='read', cursor=cursor, count=count,
                                         actual=len(data), destination=position,
                                         crossed_nominal_end=cursor+len(data)>nominal_end))
                    cursor += len(data)
                em.write_data(0x4000, bytes(2))
                deadline = time.monotonic() + 20
            em.qmp.execute('cont')
        time.sleep(.001)
    em.qmp.execute('stop')
    (out/'cpu.txt').write_text(state, encoding='utf-8')
    (out/'requests.json').write_text(json.dumps(requests, indent=2)+'\n', encoding='utf-8')
    assert 'ROM_MISSING=none' in state and 'UNSUPPORTED=none' in state and 'INVALID_CONTROL=none' in state, state
    assert 'r12=600d' in state.replace(' ', '').lower(), state[:300]
    if not args.grid_only: assert 'r15=6000' in state.replace(' ', '').lower(), state[:300]
    width = guest_memory(0x527a56, 1)[0]; height = guest_memory(0x527a5a, 1)[0]
    grid_bytes = width*height*2
    grid = guest_memory(0x510000, grid_bytes) if grid_bytes else b''
    assert (width,height) == tuple(npe[start+2:start+4])
    assert grid == npe[start+17:start+17+grid_bytes]
    (out/'grid.bin').write_bytes(grid)
    ram = guest_memory(0x520000, 0x10000); (out/'ram.bin').write_bytes(ram)
    report = dict(passed=True, source=str(args.npe.resolve()),
                  source_sha256=hashlib.sha256(npe).hexdigest(), map=args.map,
                  grid_only=args.grid_only, width=width, height=height,
                  start=start, nominal_end=nominal_end, final_cursor=cursor,
                  requests=requests, original_regions=regions,
                  original_prg_sha256=hashlib.sha256(original).hexdigest(),
                  scope='unchanged directory/header/grid and optional full map-table loader; host file-byte adapter, no original filesystem or bad-tail continuation proof')
    (out/'report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({k:report[k] for k in ['passed','map','width','height','start','nominal_end','final_cursor']}), flush=True)
finally:
    if em: em.close()
