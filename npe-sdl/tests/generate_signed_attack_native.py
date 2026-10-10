import json
from pathlib import Path

samples = []
for part in ['party', 'enemy', 'enemy-overflow']:
    samples += json.loads(Path(f'tmp/native-signed-attack-{part}.json').read_text(encoding='utf-8'))
assert len(samples) == 17 and len({s['config']['name'] for s in samples}) == 17
lines = ['/* Original runtime independent RAM fixtures; no original resource files edited. */',
         '#ifndef SIGNED_ATTACK_NATIVE_H', '#define SIGNED_ATTACK_NATIVE_H',
         'typedef struct { uint16_t hp, qi, attack, defense, attack_timer, defense_timer; int16_t status[4]; } SignedAttackRecord;',
         'typedef struct { const char *name; unsigned defending, prepared; SignedAttackRecord before[4], after[4]; } SignedAttackCase;',
         'static const SignedAttackCase signed_attack_cases[] = {']
def records(rows):
    result = []
    for r in rows:
        values = [r[k] for k in ['hp','qi','attack','defense','attackTimer','defenseTimer']]
        status = [v if v < 32768 else v-65536 for v in r['status']]
        result.append('{'+','.join(map(str,values))+',{'+','.join(map(str,status))+'}}')
    return ','.join(result)
for s in samples:
    assert s['actor'] == 0
    lines.append('{"%s",%u,%u,{%s},{%s}},' %
                 (s['config']['name'], bool(s['config'].get('defend')), s['prepared'],
                  records(s['before']), records(s['after'])))
lines += ['};','#endif','']
Path('npe-sdl/tests/signed_attack_native.h').write_text('\n'.join(lines),encoding='utf-8',newline='\n')
print(f'{len(samples)} original attack/defense sequences, {len(samples)*8} record phases')
