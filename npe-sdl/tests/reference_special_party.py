"""Execute original party/leader services on current-file special map tables.

The map table inputs are prior original full-loader RAM samples. The character
records are explicitly exported C diagnostic inputs, not C expected outputs.
Only the timed notice is replaced with RETS; no LCD/timing claim is made.
The original mode23 service runs when the original party service assigns it.
"""
import argparse, base64, hashlib, json, re, struct, sys, time
from pathlib import Path
sys.path.insert(0, str(Path('scripts').resolve()))
from web.np560_web import Emulator

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--qemu', type=Path, required=True)
p.add_argument('--npe', type=Path, required=True)
p.add_argument('--inputs', type=Path, required=True)
p.add_argument('--output', type=Path, default=Path('tmp/special-party-native'))
args = p.parse_args()
out = args.output
out.mkdir(parents=True, exist_ok=True)
prg = Path('tmp/NP560.PRG').read_bytes()
source_hash = hashlib.sha256(args.npe.read_bytes()).hexdigest()
cases = [json.loads(line) for line in (args.inputs/'cases.jsonl').read_text(encoding='utf-8').splitlines()]
report = dict(passed=False, source=str(args.npe.resolve()), source_sha256=source_hash,
              program_sha256=hashlib.sha256(prg).hexdigest(),
              diagnostic_character_inputs=True, original_lcd_checked=False,
              notice_wait_skipped=True, cases=[])
regions = [(0x220f60, 0x3f0f60, 0x522), (0x29ef8c, 0x46ef8c, 0x28e),
           (0x27c8b6, 0x44c8b6, 0x330), (0x3307b0, 0x5007b0, 0x34),
           (0x32945c, 0x4f945c, 0x220), (0x328204, 0x4f8204, 0x200),
           (0x3346a2, 0x5046a2, 4), (0x186d56, 0x356d56, 0x180)]
images = {}
for mode, function in [(10, (0x22, 0x0f60)), (13, (0x29, 0xef8c))]:
    image = bytearray(0x30000)+prg
    image[0x34050:0x34052] = struct.pack('<H', 0xc620)
    code = [0x6800, 2, 0x68ff, 0x6000, 0x6810, 0, 0x6811, 0,
            0xbd00 | function[0], function[1]]
    for offset, destination in [(0x7934, 0x1800), (0x7936, 0x1802)]:
        code += [0x6810, 0xa4, 0x88f0, offset, 0x6810, 0, 0xa8f0, destination]
    code += [0x6810, 0xa4, 0x88f0, 0x7934, 0x76f0, 23, 0xac02, 0]
    branch = len(code)-1
    code += [0xbd27, 0xc8b6]
    code[branch] = len(code)*2
    code += [0x68fc, 0x600d]
    final_pc = len(code)*2
    code += [0xd0ff]
    raw_code = struct.pack('<%dH' % len(code), *code)
    image[0x30000:0x30000+len(raw_code)] = raw_code
    rom = out/f'service{mode}.rom'; rom.write_bytes(image)
    firmware = out/f'service{mode}.prg'; firmware.write_bytes(image[0x30000:])
    images[mode] = (rom, firmware, final_pc)

try:
    for cfg in cases:
        native_dir = Path(f"tmp/map-stream-native/tianming-v13-map{cfg['map']}-full")
        loader = json.loads((native_dir/'report.json').read_text(encoding='utf-8'))
        assert loader['passed'] and not loader['grid_only'] and loader['source_sha256'] == source_hash
        ram = bytearray((native_dir/'ram.bin').read_bytes())
        assert len(ram) == 65536
        before = (args.inputs/(cfg['name']+'.before')).read_bytes()
        assert len(before) == 444 and [before[148*i+12] for i in range(3)] == [1, 2, 3]
        (out/(cfg['name']+'.before')).write_bytes(before)
        ram[0x69c6:0x69c6+444] = before
        for address, value in [(0x7934, cfg['mode']), (0x7936, cfg['slot']),
                               (0x7a58, cfg['map']), (0x7ab4, 0)]:
            struct.pack_into('>H', ram, address, value)
        raw = bytearray(ram); raw[0::2], raw[1::2] = raw[1::2], raw[0::2]
        input_path = out/(cfg['name']+'.input-ram'); input_path.write_bytes(raw)
        rom, firmware, final_pc = images[cfg['mode']]
        em = None
        try:
            em = Emulator(qemu=args.qemu, firmware=firmware, soc_rom=rom,
                          soc_rom_skeleton=True, boot_mode='firmware-vma03', start_paused=True)
            em.load_data(0x520000, input_path.resolve())
            for address, physical, size in regions:
                observed = base64.b64decode(em.snapshot('code', address, size, diagnostics=False)['frame'])
                assert observed == prg[physical-0x200000:physical-0x200000+size], hex(address)
            em.qmp.execute('cont'); deadline = time.monotonic()+20; state = ''
            while time.monotonic() < deadline:
                state = em.qmp.execute('qom-get', {'path':'/machine', 'property':'cpu-state'})
                if f'PC=03:{final_pc:04x}'.lower() in state.lower(): break
                if any(k+'=none' not in state for k in ['ROM_MISSING','UNSUPPORTED','INVALID_CONTROL']): break
                time.sleep(.01)
            em.qmp.execute('stop'); (out/(cfg['name']+'.cpu.txt')).write_text(state)
            assert f'PC=03:{final_pc:04x}'.lower() in state.lower(), state[:2000]
            assert all(k+'=none' in state for k in ['ROM_MISSING','UNSUPPORTED','INVALID_CONTROL']), state
            regs = {int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})', state, re.I)}
            assert regs[12] == 0x600d and regs[15] == 0x6000
            def memory(address, size):
                data = bytearray(base64.b64decode(em.snapshot('data', address, size, diagnostics=False)['frame']))
                data[0::2], data[1::2] = data[1::2], data[0::2]
                return bytes(data)
            after_records = memory(0x5269c6, 444)
            after_mode_event = memory(0x527934, 4)
            stage = struct.unpack('>2H', memory(0x1800, 4))
            (out/(cfg['name']+'.after')).write_bytes(after_records+after_mode_event)
            row = dict(name=cfg['name'], map=cfg['map'], mode=cfg['mode'], slot=cfg['slot'],
                       original_service_mode=stage[0], original_service_event=stage[1],
                       experience_called=stage[0] == 23,
                       final_mode=int.from_bytes(after_mode_event[:2], 'big'),
                       final_event=int.from_bytes(after_mode_event[2:], 'big'),
                       before_sha256=hashlib.sha256(before).hexdigest(),
                       after_sha256=hashlib.sha256(after_records).hexdigest(),
                       hp=int.from_bytes(after_records[80:82], 'big'),
                       qi=int.from_bytes(after_records[84:86], 'big'), level=after_records[24],
                       experience=int.from_bytes(after_records[76:78], 'big'))
            report['cases'].append(row)
            print(json.dumps(row), flush=True)
        finally:
            if em: em.close()
    report.update(passed=True, original_regions=regions)
except Exception as error:
    report['error'] = str(error)
    raise
finally:
    (out/'report.json').write_text(json.dumps(report, indent=2)+'\n')
