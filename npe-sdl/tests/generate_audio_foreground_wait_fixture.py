"""Original foreground service Timer2 counts; not a codec/LCD reference."""
import hashlib
import json
from pathlib import Path

r = json.loads(Path('tmp/audio-foreground-wait-native/report.json').read_text(encoding='utf8'))
assert r['passed'] and len(r['cases']) == 6
assert hashlib.sha256(Path('tmp/NP560.PRG').read_bytes()).hexdigest() == r['original_prg_sha256']
counts = [c['timer2_ticks'] for c in r['cases']]
assert counts == [200, 250, 450, 50, 0, 0]
Path('npe-sdl/tests/audio_foreground_wait_native.h').write_text(
    '/* Unchanged original foreground service counts, zero DAC targets. */\n'
    'static const unsigned foreground_wait_native[] = {' + ','.join(map(str, counts)) + '};\n',
    encoding='utf8')
