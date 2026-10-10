"""Export callback activity and waits from the original driver diagnostic."""
import hashlib
import json
from pathlib import Path

report = json.loads(Path('tmp/audio-callback-lifetime-native/report.json').read_text())
assert report['passed'] and len(report['cases']) == 3
assert report['original_prg_sha256'] == hashlib.sha256(Path('tmp/NP560.PRG').read_bytes()).hexdigest()
rows = {row['name']: row for row in report['cases']}
assert all(row['passed'] and row['original_service_bytes_unchanged']
           and row['host_guest_memory_writes'] == 0 for row in rows.values())
stop = rows['callback-return']['timer2_ticks']
normal = rows['normal-reopen']
muted = rows['muted-reopen']
cleanup = muted['timer2_ticks'] - stop
startup = normal['timer2_ticks'] - muted['timer2_ticks']
assert stop == cleanup
text = ('/* Original callback lifetime; generate_audio_callback_lifetime_fixture.py.\n'
        '   Diagnostic EOF/callback; no codec or game resource reopen proof. */\n'
        'enum { AUDIO_CALLBACK_STOP_TICKS = %d, AUDIO_CALLBACK_CLEANUP_TICKS = %d,\n'
        '       AUDIO_CALLBACK_START_TICKS = %d };\n'
        'static const unsigned audio_callback_activity[] = {%d, %d, %d, %d};\n')
Path('npe-sdl/tests/audio_callback_lifetime_native.h').write_text(text % (
    stop, cleanup, startup, normal['entry_flags89'] & 8,
    normal['cleanup_flags89'] & 8, normal['started_flags89'] & 8,
    normal['returned_flags89'] & 8), encoding='utf-8')
