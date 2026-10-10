import json
from pathlib import Path

report = json.loads(Path('tmp/audio-dac-state-native/report.json').read_text(encoding='utf-8'))
assert report['passed'] and len(report['cases']) == 10
lines = ['/* Unmodified original DAC service state; transfer-start boundary is explicit. */',
         'static const DacStateNative dac_state_native[]={']
for c in report['cases']:
    assert c['passed']
    a = c['after']
    values = [c['flags88'], c['flags8a'], c['first'], *c['saved'], *c['current'], c['control'], c['gpio']]
    calls = c['calls'] + [0] * (2 - len(c['calls']))
    expected = [*a['current'], *a['saved'], a['control'], a['gpio'], a['ticks']]
    lines.append('{"%s",%s,%d,{%s},%s},' % (
        c['name'], ','.join(map(str, values)), len(c['calls']), ','.join(map(str, calls)), ','.join(map(str, expected))))
lines += ['};']
Path('npe-sdl/tests/audio_dac_state_native.h').write_text('\n'.join(lines) + '\n', encoding='utf-8')
print('10 original DAC service states generated')
