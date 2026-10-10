from pathlib import Path
import json

rows = json.loads(Path('tmp/native-attack-sequence.json').read_text(encoding='utf-8'))
assert len(rows) == 2
lines = ['#ifndef ATTACK_SEQUENCE_NATIVE_H', '#define ATTACK_SEQUENCE_NATIVE_H',
         '#include "negative_hp_native.h"',
         'typedef struct { const char *name; unsigned party; NegativeHpRecord before[6],after[6]; } AttackCase;',
         'static const AttackCase attack_animation_cases[] = {']

def records(rr):
    out = []
    for r in rr:
        values = [r[k] for k in ['hp', 'maximum', 'qi', 'mental', 'attack', 'defense',
                                 'level', 'experience', 'attackTimer', 'defenseTimer']]
        status = [x if x < 32768 else x - 65536 for x in r['status']]
        out.append('{' + ','.join(map(str, values)) + ',{' + ','.join(map(str, status)) + '}}')
    return ','.join(out)

for row in rows:
    name, party, events = row['config']['name'], row['config']['party'], row['events']
    assert len(events) == 2 * party + 1
    assert all(e['mode'] == 3 and len(e['records']) == party + 3 for e in events)
    assert all(e['records'] == events[0]['records'] for e in events[:-1])
    assert events[-1]['actor'] == 0
    for actor in range(party):
        assert events[2 * actor + 1]['actor'] == actor
        assert events[2 * actor + 1]['menu'] == 17
    assert len(json.loads(Path('tmp/native-' + name + '-commits.json').read_text())) == 9 * party + 6
    lines.append('{"%s",%u,{%s},{%s}},' % (name, party, records(events[0]['records']), records(events[-1]['records'])))
lines += ['};', f'#define ATTACK_RECORD_PHASES {sum(len(e["records"]) for r in rows for e in r["events"])}',
          f'#define ATTACK_ACTIONS {sum(r["config"]["party"] for r in rows)}', '#endif', '']
Path('npe-sdl/tests/attack_sequence_native.h').write_text('\n'.join(lines), encoding='utf-8', newline='\n')
