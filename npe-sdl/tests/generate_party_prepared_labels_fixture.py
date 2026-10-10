"""Generate warm label fixtures from original-controller samples only."""
from pathlib import Path
import json


def records(values):
    out = []
    for r in values:
        numbers = [r[k] for k in ['hp', 'maximum', 'qi', 'mental', 'attack',
                   'defense', 'level', 'experience', 'attackTimer', 'defenseTimer']]
        status = [v if v < 32768 else v - 65536 for v in r['status']]
        out.append('{' + ','.join(map(str, numbers)) + ',{' + ','.join(map(str, status)) + '}}')
    return ','.join(out)


def prepared(values):
    return ','.join('{' + ','.join(map(str, row)) + '}' for row in values)


for kind, expected in [('spell', 6), ('item', 4)]:
    rows = json.loads(Path(f'tmp/native-party-{kind}-prepared-labels.json' if kind == 'spell'
                           else 'tmp/native-item-prepared-labels.json').read_text(encoding='utf8'))
    assert len(rows) == expected
    struct = 'EnemyAnimationCase' if kind == 'spell' else 'ItemAnimationCase'
    array = 'enemy_animation_cases' if kind == 'spell' else 'item_animation_cases'
    prefix = 'ENEMY' if kind == 'spell' else 'ITEM'
    fields = 'skill' if kind == 'spell' else 'item'
    lines = [f'#ifndef PARTY_PREPARED_{kind.upper()}_LABELS_NATIVE_H',
             f'#define PARTY_PREPARED_{kind.upper()}_LABELS_NATIVE_H',
             '#include "negative_hp_native.h"',
             'typedef struct { const char *name; unsigned party,enemy,fast,frames,party_actions,final_actor,direct,icon,seed,'
             + fields + ',target,own,group,character; uint8_t actions[3],actors[3]; NegativeHpRecord before[6],after[6];'
             ' uint8_t prepared_before[6][12],prepared_after[6][12],prepared_action[6][12]; } ' + struct + ';',
             f'static const {struct} {array}[] = {{']
    total_frames = total_phases = total_actions = total_records = 0
    for row in rows:
        c, events = row['config'], row['events']
        actors = [i for i in range(c['party']) if 0 < c['hp'][i] < 32768]
        assert actors[0] == 0 and len(events) == 2 * len(actors) + 1
        assert all(e['records'] == events[0]['records'] for e in events[:-1])
        commits = json.loads(Path(f"tmp/native-{c['name']}-commits.json").read_text())
        command = events[2]['commands'][0] if kind == 'spell' else commits[0]['command']
        assert command['kind'] == (1 if kind == 'spell' else 2) and command[fields] == c[fields]
        target = command['target'] - 1 - (0 if c['own'] else c['party'])
        assert 0 <= target < 3
        assert row['preparedPhases'] and all(p['execution'] == 1 for p in row['preparedPhases'])
        values = {'name': '"' + c['name'] + '"', 'party': str(c['party']), 'enemy': '1',
                  'frames': str(len(commits)), 'party_actions': str(len(actors)),
                  'final_actor': str(events[-1]['actor']), 'icon': '2', 'seed': '61',
                  fields: str(c[fields]), 'target': str(target), 'own': str(int(c['own'])),
                  'group': str(int(c['group'])), 'character': str(c['character']),
                  'actions': '{' + ','.join(map(str, [8 if kind == 'spell' else 2] + [5] * (len(actors) - 1))) + '}',
                  'actors': '{' + ','.join(map(str, actors)) + '}',
                  'before': '{' + records(events[0]['records']) + '}',
                  'after': '{' + records(events[-1]['records']) + '}',
                  'prepared_before': '{' + prepared(events[0]['prepared']) + '}',
                  'prepared_after': '{' + prepared(events[-1]['prepared']) + '}',
                  'prepared_action': '{' + prepared(row['preparedPhases'][-1]['prepared']) + '}'}
        lines.append('{' + ','.join('.' + k + '=' + v for k, v in values.items()) + '},')
        total_frames += len(commits)
        total_phases += len(events) * (c['party'] + 3)
        total_actions += len(actors)
        total_records += c['party'] + 3
    lines += ['};', f'#define {prefix}_ANIMATION_RECORD_PHASES {total_phases}',
              f'#define {prefix}_ANIMATION_ACTIONS {total_actions}',
              f'#define {prefix}_ANIMATION_COMMITS {total_frames}',
              f'#define PARTY_PREPARED_LABEL_RECORDS {total_records}', '#endif', '']
    Path(f'npe-sdl/tests/party_prepared_{kind}_labels_native.h').write_text('\n'.join(lines), encoding='utf8', newline='\n')
    print(f'{kind}: {len(rows)} rounds / {total_frames} original LCD commits')
