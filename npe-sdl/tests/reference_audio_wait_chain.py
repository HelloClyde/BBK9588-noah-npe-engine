"""Isolate original DAC stop/cleanup/start services and their Timer2 waits.

The call order comes from the original EOF and reopen paths; this is not a
codec, complete IRQ callback, game, or physical DAC timing reference.
"""
import argparse, hashlib, json, re, struct, sys, time
from pathlib import Path
sys.path.insert(0, str(Path('scripts').resolve()))
from web.np560_web import Emulator
sys.path.insert(0, str(Path('scripts/tests').resolve()))
from test_np560_dtu import write_word

parser = argparse.ArgumentParser()
parser.add_argument('--qemu', type=Path, required=True)
args = parser.parse_args()
out = Path('tmp/audio-wait-chain-native'); out.mkdir(exist_ok=True)
original = Path('tmp/NP560.PRG').read_bytes()
# PRG source banks 20..5F occupy runtime banks 03..42.
image = bytearray(0x30000) + original
rom = out/'original-services.rom'; rom.write_bytes(image)
cases = []
for name, idle, cold, calls, count in [
    ('warm-start', True, False, [0x4702], 200),
    ('replace-active', False, False, [0x3262, 0x4702], 250),
    ('eof-reopen-services', False, False, [0x475e, 0x3262, 0x4702], 300),
    ('explicit-stop-twice', False, False, [0x3262, 0x3262], 50),
    ('already-idle-stop', True, False, [0x3262], 0),
    ('cold-gpio-start', True, True, [0x4702], 1200),
]:
    code = [0x6800, 2, 0x68ff, 0x6000, 0x68e9, 0x8000, 0xee00,
            0x682e, 0, 0xfe00, 0x6810, 0,
            0x6831, 0x4000 if cold else 0x0040,
            0x6870, 3, 0x6871, 0xfffd, 0x6872, 1]
    # BIT88 is the DPRAM word F010, not direct SFR88.
    write_word(code, 0xf010, 4 if idle else 1)
    for entry in calls:
        code.extend([0xbd08, entry])
    # Duplicate final control value to mark the return without changing it.
    code.extend([0x6872, 1 if calls[-1] == 0x4702 else 0x8000, 0x68f4, 0x600d])
    pc = len(code)*2; code.append(0xd0ff)
    firmware = out/(name+'.prg')
    # The machine maps banks >=20 from the PRG bus, not the resident ROM.
    # Preserve those original functions too (notably 24:9AB8/9BD4).
    caller_image = bytearray(image[0x200000:])
    caller_image[:len(code)*2] = struct.pack('<%dH'%len(code), *code)
    firmware.write_bytes(caller_image)
    emulator = None
    try:
        emulator = Emulator(qemu=args.qemu, firmware=firmware, soc_rom=rom,
                            soc_rom_skeleton=True, start_paused=True, icount_shift=0)
        emulator.qmp.execute('qom-set', {'path':'/machine', 'property':'instruction-clock', 'value':True})
        emulator.qmp.execute('cont'); deadline = time.monotonic()+12; state = ''
        while time.monotonic()<deadline:
            state = emulator.qmp.execute('qom-get', {'path':'/machine','property':'cpu-state'})
            if f'PC=20:{pc:04x}'.lower() in state.lower(): break
            time.sleep(.02)
        emulator.qmp.execute('stop')
        (out/(name+'.cpu.txt')).write_text(state, encoding='utf-8')
        assert f'PC=20:{pc:04x}'.lower() in state.lower(), state[:1500]
        assert 'ROM_MISSING=none' in state and 'UNSUPPORTED=none' in state, state[:1500]
        assert re.search(r'\bR4\s*=600d\b', state, re.I), state[:1500]
        assert re.search(r'\bR15\s*=6000\b', state, re.I), state[:1500]
        packet = emulator.audio()
        events = packet['control_events']
        timer = emulator.qmp.execute('qom-get', {'path':'/machine','property':'timer2-state'})
        assert int(re.search(r'EXPIRED:(\d+)', timer)[1]) == count, timer
        assert 'RUNNING:0' in timer and 'ICR:0000' in timer, timer
        elapsed = (events[-1]['virtual_ns']-events[0]['virtual_ns'])/1e6
        assert count <= elapsed < count+1, (name, elapsed)
        output_events = [e for e in events if e['reg']==0x72]
        expected_controls = {
            'warm-start': [1,1,1,1],
            'replace-active': [1,0x8001,0x8000,0x8001,1,1],
            'eof-reopen-services': [1,0x8001,0x8000,0x8000,0x8000,0x8001,1,1],
            'explicit-stop-twice': [1,0x8001,0x8000,0x8000],
            'already-idle-stop': [1,0x8000],
            'cold-gpio-start': [1,1,1,1],
        }
        assert [e['value'] for e in output_events] == expected_controls[name], output_events
        if name=='explicit-stop-twice':
            assert 'DPRAM_F010=0004' in state, state[:1500]
        row = dict(name=name, calls=[f'08:{v:04X}' for v in calls],
                   timer2_ticks=count, elapsed_ms=elapsed, events=events,
                   registers=packet['registers'], host_guest_memory_writes=0,
                   passed=True)
        cases.append(row); print(json.dumps(dict(name=name, timer2_ticks=count,
            elapsed_ms=elapsed, output_control_writes=len(output_events), passed=True)), flush=True)
    finally:
        if emulator: emulator.close()
report = dict(passed=len(cases)==6, cases=cases,
              original_prg_sha256=hashlib.sha256(original).hexdigest(),
              qemu_sha256=hashlib.sha256(args.qemu.read_bytes()).hexdigest(),
              scope='original service call chains and Timer2 wait counts, not physical DAC/codec timing')
(out/'report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
