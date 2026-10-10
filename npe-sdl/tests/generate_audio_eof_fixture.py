"""Export Timer2 counts measured in the original full EOF IRQ diagnostic."""
import json
from pathlib import Path

report = json.loads(Path('tmp/audio-eof-irq-native/report.json').read_text())
assert report['passed'] and len(report['cases']) == 8
rows = {c['name']: c for c in report['cases']}
assert all(c['passed'] and c['stack_restored'] and c['foreground_registers_restored']
           and c['host_guest_memory_writes'] == 0 for c in rows.values())
assert rows['finite']['callback_called'] is False
assert rows['callback-return']['callback_called'] is True
assert rows['callback-return']['timer2_ticks'] == rows['finite']['timer2_ticks']
assert all(c['timer2_ticks'] == 0 and not c['callback_called']
           for name, c in rows.items() if name.startswith('guard-'))
Path('npe-sdl/tests/audio_eof_native.h').write_text(
    '/* Original EOF IRQ Timer2 counts; generate_audio_eof_fixture.py.\n'
    '   Excludes refill/codec/resource reopen and DAC instruction timing. */\n'
    'enum { AUDIO_EOF_NATIVE_STOP_TICKS = %d, AUDIO_EOF_NATIVE_LOOP_TICKS = %d };\n'
    % (rows['finite']['timer2_ticks'], rows['callback-start-services']['timer2_ticks']),
    encoding='utf-8')
