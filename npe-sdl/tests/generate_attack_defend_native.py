from pathlib import Path
import json

rows = json.loads(Path('tmp/native-attack-defend.json').read_text(encoding='utf-8'))
assert len(rows) == 7
extra = json.loads(Path('tmp/native-attack-defend-extra.json').read_text(encoding='utf-8'))
assert len(extra) == 2
rows += extra
lines = ['#ifndef ATTACK_DEFEND_NATIVE_H', '#define ATTACK_DEFEND_NATIVE_H',
         '#include "negative_hp_native.h"',
         'typedef struct { const char *name; unsigned party,frames,icon; uint8_t actions[3],targets[3],order[3]; uint16_t speeds[3]; NegativeHpRecord before[6],after[6]; } AttackCase;',
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
    actions = [5 if action == 'defend' else 0 for action in row['config']['actions']]
    targets = row['config'].get('attackTargets',[0,0,0])
    assert len(events) == 2 * party + 1
    assert all(e['mode'] == 3 and len(e['records']) == party + 3 for e in events)
    assert all(e['records'] == events[0]['records'] for e in events[:-1])
    assert events[-1]['actor'] == 0
    for actor in range(party):
        assert events[2 * actor + 1]['actor'] == actor
        assert events[2 * actor + 1]['menu'] == (31 if actions[actor] == 5 else 17)
        if actions[actor] == 5: assert events[2 * actor + 1]['row'] == 2
        if actions[actor] == 0:
            selected_target = party + targets[actor] + 1
            assert events[2 * actor + 1]['displayTarget'] == selected_target, (name, actor)
            if actor + 1 < party:
                assert events[2 * actor + 2]['commands'][actor]['target'] == selected_target, (name, actor)
    commits = json.loads(Path('tmp/native-' + name + '-commits.json').read_text())
    expected = 6 + sum(2 if action == 5 else 9 for action in actions)
    assert len(commits) == expected
    speeds = [1,999,2] if row['config'].get('secondFast') else [999,998,997]
    order = sorted(range(party),key=lambda actor:-speeds[actor])
    lines.append('{"%s",%u,%u,%u,{%s},{%s},{%s},{%s},{%s},{%s}},' % (name, party, expected, 2 if actions[-1] == 5 else 1,
                 ','.join(map(str, actions)), ','.join(map(str,targets)), ','.join(map(str,order)), ','.join(map(str,speeds)),
                 records(events[0]['records']), records(events[-1]['records'])))
lines += ['};', f'#define ATTACK_RECORD_PHASES {sum(len(e["records"]) for r in rows for e in r["events"])}',
          f'#define ATTACK_ACTIONS {sum(r["config"]["party"] for r in rows)}',
          f'#define ATTACK_COMMITS {sum(6 + sum(2 if a == "defend" else 9 for a in r["config"]["actions"]) for r in rows)}', '#endif', '']
Path('npe-sdl/tests/attack_defend_native.h').write_text('\n'.join(lines), encoding='utf-8', newline='\n')
