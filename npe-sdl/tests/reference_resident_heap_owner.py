"""Original31:4974 resource owner helper and original launcher allocation prefix.

Diagnostic setup resets heap/RAM. Launcher filesystem initialization and its
UI are outside this test. Two allocation prompts are explicit count stubs.
"""
import argparse, base64, hashlib, json, re, struct, sys, time
from pathlib import Path
sys.path.insert(0, str(Path('scripts').resolve()))
from web.np560_web import Emulator

parser = argparse.ArgumentParser()
parser.add_argument('--qemu', type=Path, required=True)
args = parser.parse_args()
out = Path('tmp/resident-heap-owner-native'); out.mkdir(exist_ok=True)
original = Path('tmp/NP560.PRG').read_bytes()
cases = [dict(name=name, request=request, prefix=prefix, no_handles=handles, no_space=space)
         for name, request, prefix, handles, space in [
             ('launcher-four', 256, True, False, False),
             ('owned-high-size', 0x1000100, False, False, False),
             ('owned-zero', 0, False, False, False),
             ('owned-wrap', 65535, False, False, False),
             ('owned-no-handles', 256, False, True, False),
             ('owned-no-space', 256, False, False, True)]]
rows = []
for case in cases:
    resident = bytearray(bytes(0x30000) + original)
    caller = bytearray(original)
    def install(address, words):
        data = struct.pack('<%dH' % len(words), *words)
        resident[address:address + len(data)] = data
        caller[address - 0x30000:address - 0x30000 + len(data)] = data
    # Stop after the actual original four allocations, before file picker UI.
    install(0x1b2496, [0xc620])
    install(0x1b2c46, [0xc620])
    for address, counter in [(0x33197a, 0x1400), (0x32136c, 0x1402)]:
        install(address, [0x2e00, 0x1f10, 0x58f2, 0x3f10, 0x88f2, counter,
                          0x5021, 0x3f10, 0xa8f2, counter, 0x382f, 0x1f90, 0x2f00, 0xc620])
    code = [0x6800, 2, 0x68ff, 0x6000]
    for address in [0x510000, 0x518000, 0x520000, 0x528000]:
        code += [0x6810, address >> 15, 0x4810, 0x68f0, 0xa5a5, 0xbf00, 0x3fff, 0x1a10]
    code += [0xbd30, 0x8c1c]
    def word(address, value):
        code.extend([0x6810, address >> 15, 0x68f2, value, 0xa8f2, address & 0x7fff])
    if case['no_handles']:
        for address in range(0x5127ea, 0x51280a, 2): word(address, 65535)
    if case['no_space']:
        word(0x5129a0, 0x51); word(0x5129a2, 0x4a50)
    code += [0x4800, 0x3f10, 0xa8f0, 0x1400, 0x3f10, 0xa8f0, 0x1402]
    if case['prefix']:
        code += [0xbd1b, 0x23ce]
    else:
        for value in [0x5186, 0x52, case['request'] & 65535, case['request'] >> 16, 0x574c, 0x52]:
            code += [0x68f0, value, 0x58f0]
        code += [0xbd31, 0x4974, 0x50fc]
    code += [0x3f10, 0xa8f0, 0x1404, 0x68fc, 0x600d]
    pc = len(code) * 2; code.append(0xd0ff)
    install(0x30000, code)
    firmware = out / (case['name'] + '.prg'); rom = out / (case['name'] + '.rom')
    firmware.write_bytes(caller); rom.write_bytes(resident)
    em = None
    try:
        em = Emulator(qemu=args.qemu, firmware=firmware, soc_rom=rom,
                      soc_rom_skeleton=True, boot_mode='firmware-vma03', start_paused=True)
        regions = [(0x314974, 0xbe), (0x1b23ce, 0xc8), (0x2577da, 0x3e8),
                   (0x315300, 0xba), (0x308c1c, 0xf6), (0x332512, 0x28)]
        for address, size in regions:
            actual = base64.b64decode(em.snapshot('code', address, size, diagnostics=False)['frame'])
            assert actual == original[address - 0x30000:address - 0x30000 + size], (case['name'], hex(address))
        em.qmp.execute('cont'); deadline = time.monotonic() + 12; state = ''
        while time.monotonic() < deadline:
            state = em.qmp.execute('qom-get', {'path': '/machine', 'property': 'cpu-state'})
            if f'PC=03:{pc:04x}'.lower() in state.lower(): break
            time.sleep(.01)
        em.qmp.execute('stop')
        (out / (case['name'] + '.cpu.txt')).write_text(state, encoding='utf8')
        assert f'PC=03:{pc:04x}'.lower() in state.lower(), state[:1800]
        assert 'ROM_MISSING=none' in state and 'UNSUPPORTED=none' in state, state[:1800]
        regs = {int(r): int(v, 16) for r, v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})', '\n'.join(state.splitlines()[:5]), re.I)}
        assert regs[12] == 0x600d and regs[15] == 0x6000, regs
        def memory(address, size):
            data = bytearray()
            for offset in range(0, size, 65536):
                data.extend(base64.b64decode(em.snapshot('data', address + offset, min(65536, size - offset), diagnostics=False)['frame']))
            for i in range(0, len(data), 2): data[i], data[i + 1] = data[i + 1], data[i]
            return bytes(data)
        ram = memory(0x510000, 0x20000); trace = memory(0x1400, 6)
        fail = case['no_handles'] or case['no_space']
        assert trace == struct.pack('>3H', int(fail), int(fail), int(not fail)), trace.hex()
        if case['prefix']:
            for i, (handle, pointer) in enumerate([(0x574c, 0x5186), (0x574e, 0x518a), (0x5760, 0x518e), (0x5626, 0x5192)]):
                assert int.from_bytes(ram[0x10000 + handle:0x10002 + handle], 'big') == i + 1
                assert int.from_bytes(ram[0x10000 + pointer:0x10004 + pointer], 'big') == 0x514a46 + 262 * i
        row = dict(input=case, original_code_regions=regions, stack_restored=True, passed=True)
        for label, content in [('ram', ram), ('trace', trace)]:
            path = out / (case['name'] + '-' + label + '.bin'); path.write_bytes(content)
            row[label] = path.as_posix(); row[label + '_sha256'] = hashlib.sha256(content).hexdigest()
        rows.append(row)
        print(case['name'], '128KiB RAM, original owner writes and two prompt boundaries captured', flush=True)
    finally:
        if em is not None: em.close()
report = dict(passed=True, cases=rows, original_prg_sha256=hashlib.sha256(original).hexdigest(),
              scope='Unmodified owner helper and launcher four-allocation prefix, diagnostic heap reset and prompt stubs. Filesystem initialization, full picker lifecycle and actual RPG drawing are not covered.')
(out / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
