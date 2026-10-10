import json
from pathlib import Path

all_samples=json.loads(Path('tmp/native-negative-hp.json').read_text(encoding='utf-8'))
# The two early target explorations sent extra keys after the automatic attack.
# Their two-round results are retained in raw logs, excluded from one-round fixtures.
samples=[s for s in all_samples if not s['config']['name'].startswith('target-')]
samples+=json.loads(Path('tmp/native-negative-hp-picker.json').read_text(encoding='utf-8'))
assert len(samples)==14 and all('after' in s and 'error' not in s for s in samples)
lines=['/* Completed independent original RAM input fixtures, not continuous story saves. */',
 '#ifndef NEGATIVE_HP_NATIVE_H','#define NEGATIVE_HP_NATIVE_H',
 'typedef struct { uint16_t hp, maximum, qi, mental, attack, defense, level, experience, attack_timer, defense_timer; int16_t status[4]; } NegativeHpRecord;',
 'typedef struct { const char *name; unsigned actor, formation, mode; NegativeHpRecord before[6], after[6]; } NegativeHpCase;',
 'static const NegativeHpCase negative_hp_cases[] = {']
def records(rows):
    result=[]
    for r in rows:
        values=[r[k] for k in ['hp','maximum','qi','mental','attack','defense','level','experience','attackTimer','defenseTimer']]
        status=[v if v<32768 else v-65536 for v in r['status']]
        result.append('{'+','.join(map(str,values))+',{'+','.join(map(str,status))+'}}')
    return ','.join(result)
for s in samples:
    c=s['config']
    assert s['mode']==(5 if c.get('win') else 3)
    if c.get('formation'):
        assert s['pickerTrace']==[{'menu':17,'target':5},{'menu':17,'target':6},{'menu':17,'target':5},{'menu':17,'target':6}]
    else: assert s['picker']['menu']==10
    if c.get('win'):
        for r,m in zip(s['after'][:3],s['mapParty']):
            assert all(r[k]==m[k] for k in ['hp','level','experience'])
    lines.append('{"%s",%u,%u,%u,{%s},{%s}},' % (c['name'],c['actor'],bool(c.get('formation')),s['mode'],records(s['before']),records(s['after'])))
lines+=['};','#endif','']
Path('npe-sdl/tests/negative_hp_native.h').write_text('\n'.join(lines),encoding='utf-8',newline='\n')
print('14 completed original negative HP/settlement inputs, 168 full record phases; 21 victory map copies')
