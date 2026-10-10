from pathlib import Path
import json

rows = json.loads(Path('tmp/native-enemy-prepared-records.json').read_text(encoding='utf-8'))
assert len(rows) == 9 and len({r['config']['name'] for r in rows}) == 9
lines = ['#ifndef ENEMY_PREPARED_RECORDS_NATIVE_H', '#define ENEMY_PREPARED_RECORDS_NATIVE_H',
         '#include "negative_hp_native.h"',
         'typedef struct { const char *name; unsigned party,enemy,fast,frames,party_actions,final_actor,direct,icon,seed,skill; uint8_t actions[3]; NegativeHpRecord before[6],after[6]; uint8_t prepared_before[6][12],prepared_after[6][12],prepared_action[6][12]; unsigned has_action,has_begin; } EnemyAnimationCase;',
         'static const EnemyAnimationCase enemy_animation_cases[] = {']

def records(rs):
    out = []
    for r in rs:
        values = [r[k] for k in ['hp', 'maximum', 'qi', 'mental', 'attack', 'defense', 'level', 'experience', 'attackTimer', 'defenseTimer']]
        status = [v if v < 32768 else v - 65536 for v in r['status']]
        out.append('{' + ','.join(map(str, values)) + ',{' + ','.join(map(str, status)) + '}}')
    return ','.join(out)

def prepared(rs):
    return ','.join('{' + ','.join(map(str, r)) + '}' for r in rs)

commits = phases = actions = 0
for r in rows:
    c, ev = r['config'], r['events']
    assert len(ev) == 2 * c['party'] + 1
    assert all(e['records'] == ev[0]['records'] for e in ev[:-1])
    frames = len(json.loads(Path('tmp/native-' + c['name'] + '-commits.json').read_text()))
    phases += len(ev) * (c['party'] + 3)
    actions += c['party'] + 1
    commits += frames
    samples = r.get('preparedPhases', [])
    action = samples[-1]['prepared'] if samples else ev[0]['prepared']
    # Reuse the existing original action-context seeds; no per-action reseed.
    seed = 4 if c['party'] == 1 else 14 if not c['skill'] else 61
    lines.append('{"%s",%u,%u,0,%u,%u,%u,0,2,%u,%u,{%s},{%s},{%s},{%s},{%s},{%s},%u,%u},' %
                 (c['name'], c['party'], c['enemyActor'], frames, c['party'], ev[-1]['actor'], seed, c['skill'],
                  ','.join(['5'] * c['party']), records(ev[0]['records']), records(ev[-1]['records']),
                  prepared(ev[0]['prepared']), prepared(ev[-1]['prepared']), prepared(action), bool(samples),
                  bool(samples and samples[0]['prepared'] == ev[0]['prepared'])))
lines += ['};', f'#define ENEMY_ANIMATION_RECORD_PHASES {phases}', f'#define ENEMY_ANIMATION_ACTIONS {actions}',
          f'#define ENEMY_ANIMATION_COMMITS {commits}', '#endif', '']
Path('npe-sdl/tests/enemy_prepared_records_native.h').write_text('\n'.join(lines), encoding='utf-8', newline='\n')
print(f'{len(rows)} enemy paths / {commits} LCD commits / {sum(bool(r.get("preparedPhases")) for r in rows)} action snapshots')
