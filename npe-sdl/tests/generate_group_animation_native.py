from pathlib import Path
import json
import sys
type_words='--type-words' in sys.argv

rows = json.loads(Path('tmp/native-group-animation'+('-type-words' if type_words else '')+'.json').read_text(encoding='utf-8'))
assert len(rows) == 6
lines = ['#ifndef GROUP_ANIMATION_NATIVE_H', '#define GROUP_ANIMATION_NATIVE_H',
         '#include "negative_hp_native.h"',
         'typedef struct { const char *name; unsigned party,frames,actions,queued_defend; uint8_t order[3]; uint16_t speeds[3]; NegativeHpRecord before[6],after[6]; uint8_t display_targets[3];'+('uint16_t type_words[3][3];' if type_words else '')+' } AttackCase;',
         'static const AttackCase attack_animation_cases[] = {']

def records(rr):
    result = []
    for r in rr:
        values = [r[k] for k in ['hp', 'maximum', 'qi', 'mental', 'attack', 'defense',
                                 'level', 'experience', 'attackTimer', 'defenseTimer']]
        status = [x if x < 32768 else x - 65536 for x in r['status']]
        result.append('{' + ','.join(map(str, values)) + ',{' + ','.join(map(str, status)) + '}}')
    return ','.join(result)

action_count = 0
commit_count = 0
for row in rows:
    c, events = row['config'], row['events']
    party, name = c['party'], c['name']
    if type_words:name=name.removesuffix('-type-words')
    queued = bool(c.get('queuedDefend'))
    assert len(events) == (5 if queued else 3)
    assert all(e['mode'] == 3 and len(e['records']) == party + 3 for e in events)
    assert all(e['records'] == events[0]['records'] for e in events[:-1])
    assert events[-2]['label'] == 'group' and events[-2]['menu'] == 31 and events[-2]['row'] == 0
    assert events[-2]['actor'] == (1 if queued else 0) and events[-1]['actor'] == 0
    if queued:
        assert events[1]['menu'] == 31 and events[1]['row'] == 2
        assert events[2]['commands'][0]['kind'] == 5 and events[2]['actor'] == 1
    speeds = [1,999,2] if c.get('secondFast') else [999,998,997]
    order = sorted([i for i in range(party) if c['hp'][i] > 0 and i != c.get('sleepSlot')], key=lambda i:-speeds[i])
    frames = 6 + 9 * len(order)
    commits = json.loads(Path('tmp/native-' + name + '-commits.json').read_text())
    assert len(commits) == frames, (name, len(commits), frames)
    # The captured next-input records contain type0; final command bytes
    # are not evidence of the earlier type4 group assignment.
    assert all(e['kind'] == 0 for e in events[-1]['commands'][:party])
    extra=',{'+','.join('{'+','.join(map(str,events[i]['typeWords']))+'}' for i in (0,-2,-1))+'}' if type_words else ''
    lines.append('{"%s",%u,%u,%u,%u,{%s},{%s},{%s},{%s},{%s}%s},' %
                 (name, party, frames, len(order), queued, ','.join(map(str,order)),
                  ','.join(map(str,speeds)), records(events[0]['records']), records(events[-1]['records']),
                  ','.join(str(events[i]['displayTarget']) for i in (0,-2,-1)),extra))
    action_count += len(order)
    commit_count += frames
lines += ['};', f'#define ATTACK_RECORD_PHASES {sum(len(e["records"]) for r in rows for e in r["events"])}',
          f'#define ATTACK_ACTIONS {action_count}', f'#define ATTACK_COMMITS {commit_count}', '#endif', '']
Path('npe-sdl/tests/group_animation'+('_type_words' if type_words else '')+'_native.h').write_text('\n'.join(lines), encoding='utf-8', newline='\n')
