import json
from pathlib import Path
r=json.loads(Path('tmp/audio-eof-gate-native/report.json').read_bytes())
assert r['passed'] and len(r['cases'])==56
lines=['/* Actual original EOF guard/cleanup/callback observations. */','static const EofNative eof_native[]={']
keys=['initial_flags89','guard8d','tested_stack','initial_flags8a','callback_called',
      'callback_flags89','reopen','cleanup_flags89','cleanup_flags8a','final_flags89','final_flags8a','timer2_ticks']
for c in r['cases']:
    assert c['passed'] and c['stack_restored'] and c['foreground_registers_restored']
    status=0 if c['early'] else 1 if c['blocked'] else 2
    lines.append('    {"'+c['name']+'",'+str(status)+','+','.join(str(int(c[k]))+'u' for k in keys)+'},')
lines+=['};','']
Path('npe-sdl/tests/audio_eof_gate_native.h').write_text('\n'.join(lines),encoding='utf8')
