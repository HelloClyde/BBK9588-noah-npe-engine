"""Execute original EOF IRQ, including its callback save/return machinery.

The guest sets BIT89.11 directly: compressed refill and codecs are excluded.
The loop callback uses original cleanup/start services, not resource reopen.
"""
import argparse, hashlib, json, re, struct, sys, time
from pathlib import Path
sys.path.insert(0, str(Path('scripts').resolve()))
sys.path.insert(0, str(Path('scripts/tests').resolve()))
from web.np560_web import Emulator
from test_np560_dtu import write_word, read_word

parser = argparse.ArgumentParser()
parser.add_argument('--qemu', type=Path, required=True)
args = parser.parse_args()
out = Path('tmp/audio-eof-irq-native'); out.mkdir(exist_ok=True)
original = Path('tmp/NP560.PRG').read_bytes()
image = bytearray(0x30000) + original
# Software TRAP vectors are fixed in bank01 (unlike the Timer2 RAM vector).
# Route only the diagnostic trap; every measured service remains unchanged.
resident = bytearray(image)
resident[0x100d0:0x100d4] = struct.pack('<2H', 0xad08, 0x3b48)
rom = out/'original-services.rom'; rom.write_bytes(resident)
cases = []
for name, callback, reopen, guard89, guard8d in [
    ('finite', False, False, 0, 0),
    ('callback-return', True, False, 0, 0),
    ('callback-start-services', True, True, 0, 0),
    ('guard-89-0', True, True, 1, 0),
    ('guard-89-1', True, True, 2, 0),
    ('guard-89-2', True, True, 4, 0),
    ('guard-8d-0', True, True, 0, 1),
    ('guard-8d-2', True, True, 0, 4),
]:
    blocked = bool(guard89 or guard8d)
    flags89 = 0x7800 | (0x8000 if callback else 0) | guard89
    # RAM vectors and 4MHz instruction-clock match the earlier waiter probe.
    code = [0x6800, 2, 0x68ff, 0x6000, 0x68e9, 0x8000, 0xee00,
            0x682e, 0, 0xfe00, 0x6810, 0,
            0x6831, 0x40, 0x6870, 3, 0x6871, 0xfffd, 0x6872, 1]
    for address, value in [(0xf010, 1), (0xf012, flags89), (0xf01a, guard8d),
                           (0xd0, 0xad08), (0xd2, 0x3b48),
                           (0x4aa8, 0x20), (0x4aaa, 0x1000),
                           (0x1200, 0), (0x1202, 0), (0x1204, 0)]:
        write_word(code, address, value)
    # Distinct foreground values must survive the synthetic RETI/RETS chain.
    code += [0x68f1, 0x1234, 0x68f2, 0x2345, 0x68f3, 0x3456,
             0x68f7, 0x789a, 0x0f34]
    for address, register in [(0x1200, 4), (0x1202, 5), (0x1204, 6)]:
        read_word(code, address, register)
    read_word(code, 0xf012, 8)
    code += [0x68fc, 0x600d]
    pc = len(code)*2; code += [0xd0ff]
    body = [0x58f0, 0x58f1]
    read_word(body, 0xf010, 0); body += [0x3f10, 0xa8f0, 0x1200]
    read_word(body, 0xf012, 0); body += [0x3f10, 0xa8f0, 0x1202]
    write_word(body, 0x1204, 0xcafe)
    if reopen: body += [0xbd08, 0x3262, 0xbd08, 0x4702]
    body += [0x381f, 0x380f, 0xc620]
    caller = bytearray(image[0x200000:])
    caller[:len(code)*2] = struct.pack('<%dH'%len(code), *code)
    caller[0x1000:0x1000+len(body)*2] = struct.pack('<%dH'%len(body), *body)
    firmware = out/(name+'.prg'); firmware.write_bytes(caller)
    em = None
    try:
        em = Emulator(qemu=args.qemu, firmware=firmware, soc_rom=rom,
                      soc_rom_skeleton=True, start_paused=True, icount_shift=0)
        em.qmp.execute('qom-set', {'path':'/machine', 'property':'instruction-clock', 'value':True})
        em.qmp.execute('cont'); deadline = time.monotonic()+12; state = ''
        while time.monotonic()<deadline:
            state = em.qmp.execute('qom-get', {'path':'/machine', 'property':'cpu-state'})
            if f'PC=20:{pc:04x}'.lower() in state.lower(): break
            time.sleep(.02)
        em.qmp.execute('stop'); (out/(name+'.cpu.txt')).write_text(state, encoding='utf-8')
        assert f'PC=20:{pc:04x}'.lower() in state.lower(), state[:1500]
        assert 'ROM_MISSING=none' in state and 'UNSUPPORTED=none' in state, state[:1500]
        regs = {int(r):int(v,16) for r,v in re.findall(r'\bR(\d+)\s*=([0-9a-f]{4})', '\n'.join(state.splitlines()[:5]), re.I)}
        assert [regs[r] for r in (1,2,3,7,12,15)] == [0x1234,0x2345,0x3456,0x789a,0x600d,0x6000], regs
        called = callback and not blocked
        assert (regs[6] == 0xcafe) == called, regs
        assert regs[8] == (0 if reopen and not blocked else flags89 & ~0x7000), regs
        if called:
            assert regs[4] == 1 and regs[5] == ((flags89 & ~0x7000) | 8), regs
        else: assert regs[4] == regs[5] == regs[6] == 0, regs
        timer = em.qmp.execute('qom-get', {'path':'/machine','property':'timer2-state'})
        ticks = int(re.search(r'EXPIRED:(\d+)', timer)[1])
        expected_ticks = 0 if blocked else 300 if reopen else 50
        assert ticks == expected_ticks, (name, timer)
        packet = em.audio()
        row = dict(name=name, initial_flags89=flags89, guard8d=guard8d,
                   callback_called=called, callback_flags88=regs[4],
                   callback_flags89=regs[5], final_flags89=regs[8],
                   timer2_ticks=ticks, events=packet['control_events'],
                   foreground_registers_restored=True, stack_restored=True,
                   host_guest_memory_writes=0, passed=True)
        cases.append(row)
        print(json.dumps({k:row[k] for k in ['name','callback_called','timer2_ticks','passed']}), flush=True)
    finally:
        if em: em.close()
report = dict(passed=len(cases)==8, cases=cases,
              original_prg_sha256=hashlib.sha256(original).hexdigest(),
              qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),
              scope='original EOF IRQ and synthetic callback dispatch/return; no compressed refill, resource reopen, codec or physical timing proof')
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n', encoding='utf-8')
