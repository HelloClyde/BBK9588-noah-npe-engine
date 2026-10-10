"""Original 48:2290..23B2 coordinates, including byte wrap on small maps.

Only the function prefix/return are diagnostic adapters. No map loader,
heap, LCD, input or zero-map continuation equivalence is claimed.
"""
import argparse, base64, hashlib, json, re, struct, sys, time
from pathlib import Path
sys.path.insert(0, str(Path('scripts').resolve()))
from web.np560_web import Emulator

p = argparse.ArgumentParser()
p.add_argument('--qemu', type=Path, required=True)
p.add_argument('--npe', type=Path)
p.add_argument('--maps', type=int, nargs='+')
p.add_argument('--name', default='map-spawn-native')
args = p.parse_args()
out = Path('tmp') / args.name
out.mkdir(parents=True, exist_ok=True)
original = Path('tmp/NP560.PRG').read_bytes()
inputs = []
for width, height in [(0, 201), (0, 0), (1, 1), (7, 3), (14, 6),
                      (15, 7), (16, 8), (25, 15), (255, 255)]:
    for x, y in [(0, 0), (1, 1), (6, 3), (7, 4),
                 (max(0, width-1), max(0, height-1)), (255, 255)]:
        inputs.append(dict(width=width, height=height, x=x, y=y))
for width, height, x, y in [(16, 8, 8, 6), (25, 15, 17, 13),
                           (25, 15, 25, 15), (255, 255, 247, 253),
                           (25, 15, 256, 256), (25, 15, 65535, 65535)]:
    inputs.append(dict(width=width, height=height, x=x, y=y))

if args.npe:
    if not args.maps: p.error('--npe requires --maps')
    source = args.npe.read_bytes()
    section, size = struct.unpack_from('>II', source, 0x3a+8*8)
    offsets = list(struct.unpack_from('>%dI' % (struct.unpack_from('>I', source, section)[0]//4), source, section))
    if offsets[-1] == 0xffffffff: offsets.pop()
    inputs = []
    for number, offset in enumerate(offsets, 1):
        start = section+offset
        width, height = source[start+2:start+4]
        tail = start+17+width*height*2
        for slot in range(12):
            row = source[tail+340+21*slot:tail+361+21*slot]
            if len(row) != 21: raise ValueError('exit outside file')
            for branch, target_at, x_at in [(0,3,7), (1,12,16)]:
                target = struct.unpack_from('>H', row, target_at)[0]
                if target not in args.maps: continue
                if target < 1 or target > len(offsets): raise ValueError('invalid target')
                destination = section+offsets[target-1]
                inputs.append(dict(width=source[destination+2], height=source[destination+3],
                    x=row[x_at], y=row[x_at+1], source_map=number, exit_slot=slot+1,
                    target_map=target, branch=branch))
    if not inputs: raise ValueError('no matching file-backed exits')
elif args.maps:
    p.error('--maps requires --npe')

(out/'inputs.bin').write_bytes(b''.join(struct.pack('<4H', c['width'],
    c['height'], c['x'], c['y']) for c in inputs))
code = [0x6800, 2, 0x68ff, 0x6000, 0x6810, 0, 0x6811, 0]
addresses = [0x7a56, 0x7a5a, 0x7a2e, 0x7a2c, 0x69d5, 0x69d6]
for i, cfg in enumerate(inputs):
    # Deliberately retain adjacent low bytes and start all four output
    # bytes nonzero. The original byte stores must preserve their peers.
    initial = [cfg['width']*256+0x91, cfg['height']*256+0x92,
               0xa593, 0xb694, 0xc7a5, 0xb6d8]
    for a, v in zip(addresses[:4]+[0x69d4, 0x69d6], initial):
        code += [0x6810, 0xa4+(a >> 15), 0x68f0, v,
                 0xa8f0, a & 0x7fff]
    code += [0x68f0, cfg['y'], 0x58f0, 0x68f0, cfg['x'], 0x58f0,
             0xbd03, 0x3a00, 0x50f4]
    for j, a in enumerate(addresses):
        # 69D5 is odd: CPU word snapshot from69D4 exposes the byte.
        aligned = a & ~1
        code += [0x6810, 0xa4+(a >> 15), 0x88f0, aligned & 0x7fff,
                 0x3f10, 0xa8f0, 0x1800 + i*12+j*2]
code += [0x68fc, 0x600d]
pc = len(code)*2
code += [0xd0ff]
assert len(code)*2 < 0x3a00
caller = bytearray(original)
caller[:len(code)*2] = struct.pack('<%dH' % len(code), *code)
# Keep the actual prologue and argument byte conversion:21EE/21F6 copy
# the low byte to the first guest byte of each incoming stack word.
prefix = list(struct.unpack('<13H', original[0x2821e0:0x2821fa]))
prefix += [0xad2b, 0x2290]
suffix = [0x3807, 0x3806, 0x3801, 0x3800, 0x2f00, 0xc620]
for a, body in [(0x3a00, prefix), (0x2823b2, suffix)]:
    caller[a:a+len(body)*2] = struct.pack('<%dH' % len(body), *body)
firmware = out/'caller.prg'
firmware.write_bytes(caller)
em = None
try:
    em = Emulator(qemu=args.qemu, firmware=firmware,
                  boot_mode='firmware-vma03', start_paused=True)
    observed = base64.b64decode(em.snapshot('code', 0x2b2290, 0x122,
                                diagnostics=False)['frame'])
    assert observed == original[0x282290:0x2823b2]
    assert base64.b64decode(em.snapshot('code', 0x33a00, 0x1a,
        diagnostics=False)['frame']) == original[0x2821e0:0x2821fa]
    em.qmp.execute('cont')
    deadline = time.monotonic()+15
    while time.monotonic() < deadline:
        state = em.qmp.execute('qom-get', {'path':'/machine', 'property':'cpu-state'})
        if f'PC=03:{pc:04x}'.lower() in state.lower(): break
        time.sleep(.01)
    em.qmp.execute('stop')
    (out/'cpu.txt').write_text(state, encoding='utf-8')
    assert f'PC=03:{pc:04x}'.lower() in state.lower(), state[:1800]
    assert all(k+'=none' in state for k in ('ROM_MISSING','UNSUPPORTED','INVALID_CONTROL')), state[:1800]
    regs = {int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})',
            '\n'.join(state.splitlines()[:5]), re.I)}
    assert regs[12] == 0x600d and regs[15] == 0x6000
    raw = base64.b64decode(em.snapshot('data', 0x1800, len(inputs)*12,
                                      diagnostics=False)['frame'])
    (out/'trace.bin').write_bytes(raw)
    cases = []
    for i, cfg in enumerate(inputs):
        words = list(struct.unpack_from('<6H', raw, i*12))
        assert words[:2] == [cfg['width']*256+0x91, cfg['height']*256+0x92]
        assert words[2] & 255 == 0x93 and words[3] & 255 == 0x94
        assert words[4] >> 8 == 0xc7 and words[5] & 255 == 0xd8
        cases.append(dict(**cfg, words=words, offset_x=words[2] >> 8,
                          offset_y=words[3] >> 8, screen_x=words[4] & 255,
                          screen_y=words[5] >> 8))
    report = dict(passed=True, cases=cases, stack_restored=True,
                  host_guest_memory_writes=0,
                  original_prg_sha256=hashlib.sha256(original).hexdigest(),
                  qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),
                  original_region=dict(cpu=0x2b2290, file=0x282290, size=0x122),
                  scope='unchanged map-coordinate slice only; prefix/return adapters; no LCD or zero-map continuation')
    if args.npe:
        report.update(source=str(args.npe.resolve()), source_sha256=hashlib.sha256(source).hexdigest())
    (out/'report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(dict(passed=True, cases=len(cases), first=cases[0])), flush=True)
finally:
    if em: em.close()
