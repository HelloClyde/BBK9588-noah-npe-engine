"""Independent original RAM samples; never derive expected targets from C."""
import json
from pathlib import Path

samples = json.loads(Path('tmp/native-battle-target-execution-all.json').read_text(encoding='utf8'))
assert len(samples) == 6 and all('error' not in s and s['execution'] for s in samples)
lines = ['/* Original firmware public-input samples, six isolated target cases. */',
         '#ifndef BATTLE_TARGET_EXECUTION_NATIVE_H', '#define BATTLE_TARGET_EXECUTION_NATIVE_H',
         'typedef struct { const char *name; unsigned side,hp,phase_count;',
         'unsigned menus[5]; uint8_t targets[5][3],execution[3],post_execution[3]; } NativeTargetCase;',
         'static const NativeTargetCase native_target_cases[] = {']
for s in samples:
    e = s['events']
    assert len(e) == (5 if s['config']['side'] else 4)
    execution = s['execution'][0]
    post_execution = s['execution'][-1]
    assert execution['execution'] == 1 and execution['actor'] == 0
    rows = ','.join('{' + ','.join(map(str, p['targets'])) + '}' for p in e)
    lines.append('{"%s",%u,%u,%u,{%s},{%s},{%s},{%s}},' % (
        s['config']['name'], s['config']['side'], s['config']['hp'], len(e),
        ','.join(str(p['menu']) for p in e), rows,
        ','.join(map(str, execution['targets'])),
        ','.join(map(str, post_execution['targets']))))
lines += ['};', '#endif', '']
Path('npe-sdl/tests/battle_target_execution_native.h').write_text('\n'.join(lines), encoding='utf8', newline='\n')
print('6 original target paths,27 menu states and6 execution states')
