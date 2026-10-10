from pathlib import Path
import json

rows = json.loads(Path('tmp/native-enemy-animation.json').read_text(encoding='utf-8'))
assert len(rows) == 2
extra = json.loads(Path('tmp/native-enemy-animation-extra.json').read_text(encoding='utf-8'))
assert len(extra) == 4
rows += extra
lines = ['#ifndef ENEMY_ANIMATION_NATIVE_H', '#define ENEMY_ANIMATION_NATIVE_H',
         '#include "negative_hp_native.h"',
         'typedef struct { const char *name; unsigned party,enemy,fast,frames,party_actions,final_actor,direct,icon,seed; uint8_t actions[3]; NegativeHpRecord before[6],after[6]; } EnemyAnimationCase;',
         'static const EnemyAnimationCase enemy_animation_cases[] = {']
def records(rr):
    out = []
    for r in rr:
        values = [r[k] for k in ['hp','maximum','qi','mental','attack','defense','level','experience','attackTimer','defenseTimer']]
        status = [v if v < 32768 else v-65536 for v in r['status']]
        out.append('{' + ','.join(map(str,values)) + ',{' + ','.join(map(str,status)) + '}}')
    return ','.join(out)
for row in rows:
    c,events = row['config'],row['events']
    assert len(events) == 2*c['party']+1 and events[-1]['mode'] == 3
    assert all(e['records'] == events[0]['records'] for e in events[:-1])
    assert events[-1]['commands'][c['party']+c['enemyActor']]['kind'] == 0
    frames = len(json.loads(Path('tmp/native-'+c['name']+'-commits.json').read_text()))
    actions = [5 if a=='defend' else 0 for a in c['actions']]
    party_actions = c['party'] - bool(c.get('killedFirst'))
    assert frames == 14+sum(2 if a==5 else 9 for i,a in enumerate(actions) if not(c.get('killedFirst') and i==0))
    # Match this captured context with one initial seed, never per-action
    # reseeding. Original startup state/call completeness remains unproven.
    seed = 14 if c['party']==3 else 4
    lines.append('{"%s",%u,%u,%u,%u,%u,%u,%u,%u,%u,{%s},{%s},{%s}},' %
                 (c['name'],c['party'],c['enemyActor'],bool(c.get('enemyFast')),frames,party_actions,events[-1]['actor'],
                  bool(c.get('emptyOuter') and actions[0]==0),2 if actions[-1]==5 else 1,seed,','.join(map(str,actions)),
                  records(events[0]['records']),records(events[-1]['records'])))
lines += ['};', f'#define ENEMY_ANIMATION_RECORD_PHASES {sum(len(e["records"]) for r in rows for e in r["events"])}',
          f'#define ENEMY_ANIMATION_ACTIONS {sum(r["config"]["party"]+1-bool(r["config"].get("killedFirst")) for r in rows)}',
          f'#define ENEMY_ANIMATION_COMMITS {sum(len(json.loads(Path("tmp/native-"+r["config"]["name"]+"-commits.json").read_text())) for r in rows)}', '#endif','']
Path('npe-sdl/tests/enemy_animation_native.h').write_text('\n'.join(lines),encoding='utf-8',newline='\n')
