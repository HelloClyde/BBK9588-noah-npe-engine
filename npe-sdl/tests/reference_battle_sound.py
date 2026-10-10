"""Run unchanged 3E:F85E; capture only the 03:438C playback boundary.

Setup uses guest instructions. No original codec, file service, IRQ or DAC proof.
"""
import argparse, base64, hashlib, json, re, struct, sys, time
from pathlib import Path
sys.path.insert(0, str(Path('scripts').resolve()))
sys.path.insert(0, str(Path('scripts/tests').resolve()))
from web.np560_web import Emulator
from test_np560_dtu import write_word

parser = argparse.ArgumentParser()
parser.add_argument('--qemu', type=Path, required=True)
args = parser.parse_args()
out = Path('tmp/battle-sound-native'); out.mkdir(exist_ok=True)
original = Path('tmp/NP560.PRG').read_bytes()
cases = []
inputs = []
for kind in (0, 1, 2, 3, 4, 5, 6, 255):
    for target in (1, 3):
        inputs.append((f'party-{kind}-{target}', 1, 2, 2, kind, target, 0, 0x123400fe))
for enabled in (0, 2, 0x100, 0x101, 0xffff):
    inputs.append((f'enabled-{enabled}', enabled, 2, 1, 0, 3, 0, 0x123400fe))
for length in (0, 0x7fff, 0x8000, 0xffff):
    inputs.append((f'length-{length}', 1, 2, 1, 0, 3, 0, 0xface0000 | length))
for target, defending in ((1, 5), (2, 5), (1, 0), (3, 5), (0, 5)):
    inputs.append((f'enemy-{target}-{defending}', 1, 2, 3, 0, target, defending, 0x123400fe))
inputs += [('zero-index', 1, 2, 0, 0, 3, 0, 0xface8000),
           ('max-index', 1, 255, 255, 0, 1, 0, 0xbeef00fe),
           ('enemy-max-target', 1, 255, 255, 1, 255, 5, 0x123400fe)]

for name, enabled, party, slot, kind, target, defending, length in inputs:
    initial = {0x7a50: enabled, 0x7a98: 0xa6b7, 0x7acc: slot << 8 | 0x91,
               0x7afa: party << 8 | 0xd3}
    record = 0x6450 + 148 * (slot - 1)
    initial[record + 126] = target << 8 | kind
    # Party attack descriptor has independent high/low words.
    initial.update({record + 114: 0x1357, record + 116: 0x2468,
                    record + 118: length >> 16, record + 120: length & 0xffff})
    if slot > party and target <= party:
        a = 0x6450 + 148 * (target - 1) + 126
        initial[a] = 0x5c00 | defending
    for index, a in enumerate((0x794a, 0x7952, 0x796a, 0x7972)):
        initial.update({a: 0xaabb, a+2: 0x200 + index,
                        a+4: 0x3344 + index, a+6: 0x5566 + index})
    code = [0x6800, 2, 0x68ff, 0x6000, 0x6810, 0, 0x6811, 0]
    write_word(code, 0x1600, 0)
    for a in range(0x1602, 0x161a, 2): write_word(code, a, 0xdead)
    code += [0x6810, 0xa4]
    for a, v in sorted(initial.items()):
        code += [0x6810, 0xa4 + (a >> 15)]
        write_word(code, a & 0x7fff, v, banked=True)
    code += [0xbd21, 0xf85e, 0x68fc, 0x600d]
    pc = len(code)*2; code += [0xd0ff]
    # Capture all twelve argument words after LINK; retain caller registers.
    boundary = [0x2e00, 0x58f0, 0x3f10, 0x88f0, 0x1600, 0x5001,
                0x3f10, 0xa8f0, 0x1600]
    for i in range(12):
        boundary += [0x780b, 6+2*i, 0x3f10, 0xa8f0, 0x1602+2*i]
    boundary += [0x380f, 0x2f00, 0xc620]
    caller = bytearray(original)
    caller[:len(code)*2] = struct.pack('<%dH' % len(code), *code)
    caller[0x438c:0x438c+len(boundary)*2] = struct.pack('<%dH' % len(boundary), *boundary)
    firmware = out/(name+'.prg'); firmware.write_bytes(caller)
    em = None
    try:
        em = Emulator(qemu=args.qemu, firmware=firmware, boot_mode='firmware-vma03', start_paused=True)
        function = base64.b64decode(em.snapshot('code', 0x21f85e, 0x522, diagnostics=False)['frame'])
        assert function == original[0x1ef85e:0x1efd80]
        em.qmp.execute('cont'); deadline=time.monotonic()+8; state=''
        while time.monotonic()<deadline:
            state=em.qmp.execute('qom-get', {'path':'/machine', 'property':'cpu-state'})
            if f'PC=03:{pc:04x}'.lower() in state.lower(): break
            time.sleep(.01)
        em.qmp.execute('stop'); (out/(name+'.cpu.txt')).write_text(state, encoding='utf8')
        assert f'PC=03:{pc:04x}'.lower() in state.lower() and 'ROM_MISSING=none' in state and 'UNSUPPORTED=none' in state, state[:1600]
        regs={int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})','\n'.join(state.splitlines()[:5]),re.I)}
        assert regs[12]==0x600d and regs[15]==0x6000
        def memory(a,n): return base64.b64decode(em.snapshot('data',a,n,diagnostics=False)['frame'])
        captured=memory(0x1600,26); (out/(name+'.call.bin')).write_bytes(captured)
        ram=b''.join(memory(a,0x8000) for a in range(0x510000,0x530000,0x8000))
        # Native snapshot bytes are low/high; C guest storage is high/low.
        guest=bytearray(ram); guest[0::2],guest[1::2]=ram[1::2],ram[0::2]
        (out/(name+'.ram.bin')).write_bytes(guest)
        calls,*words=struct.unpack('<13H',captured)
        row=dict(name=name, initial_words=sorted(initial.items()), calls=calls, arguments=words,
                 marker=guest[0x17a98], stack_restored=True, host_guest_memory_writes=0,
                 function_sha256=hashlib.sha256(function).hexdigest(), passed=True)
        assert calls in (0,1)
        cases.append(row); print(json.dumps({k:row[k] for k in ('name','calls','arguments','marker')}), flush=True)
    finally:
        if em: em.close()
report=dict(passed=len(cases)==len(inputs), cases=cases,
            original_prg_sha256=hashlib.sha256(original).hexdigest(),
            qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),
            scope='unchanged battle sound selector with diagnostic playback boundary; no original codec/file/IRQ/DAC')
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
