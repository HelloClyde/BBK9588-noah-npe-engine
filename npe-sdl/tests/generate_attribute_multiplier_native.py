"""Freeze original gameplay records and separately labelled PRG CPU slices."""
import json
from pathlib import Path

samples = json.loads(Path('tmp/native-attribute-multiplier-all.json').read_text(encoding='utf-8'))
assert len(samples) == 20
completed = [s for s in samples if 'after' in s]
faults = [s for s in samples if 'error' in s]
assert len(completed) == 14 and len(faults) == 6
assert all(s['error'] == 'execution_fault' for s in faults)
cpu = json.loads(Path('tmp/attribute-cpu-slice.json').read_text(encoding='utf-8'))
assert len(cpu) == 27
lines = ['/* Independent original RAM inputs. Six frontend faults are excluded. */',
 '#ifndef ATTRIBUTE_MULTIPLIER_NATIVE_H', '#define ATTRIBUTE_MULTIPLIER_NATIVE_H',
 'typedef struct { uint16_t hp, qi, attribute, attack, defense, attack_timer, defense_timer; int16_t status[4]; } AttributeRecord;',
 'typedef struct { const char *name; unsigned prepared; AttributeRecord before[4], after[4]; } AttributeCase;',
 'static const AttributeCase attribute_cases[] = {']
def records(rows):
    result=[]
    for r in rows:
        values=[r[k] for k in ['hp','qi','attribute','attack','defense','attackTimer','defenseTimer']]
        status=[v if v<32768 else v-65536 for v in r['status']]
        result.append('{'+','.join(map(str,values))+',{'+','.join(map(str,status))+'}}')
    return ','.join(result)
for s in completed:
    assert s['actor'] == 0
    lines.append('{"%s",%u,{%s},{%s}},' % (s['config']['name'],s['prepared'],records(s['before']),records(s['after'])))
lines += ['};', '/* Unmodified source2D:37B2..37C2 arithmetic executed in QEMU DMCU. */',
 'static const struct { int actor, target; unsigned roll; int multiplier; } attribute_cpu_cases[] = {']
for r in cpu:
    lines.append('{%d,%d,%u,%d},' % (r['actor'],r['target'],r['roll'],r['multiplier']))
lines += ['};','#endif','']
Path('npe-sdl/tests/attribute_multiplier_native.h').write_text('\n'.join(lines),encoding='utf-8',newline='\n')
print('14 original completed input sequences / 112 record phases; 27 independent CPU slices; 6 frontend faults excluded')
