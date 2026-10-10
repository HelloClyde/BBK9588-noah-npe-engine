from pathlib import Path
import json

rows=json.loads(Path('tmp/native-revival-animation.json').read_text(encoding='utf-8'))
assert len(rows)==4
lines=['#ifndef REVIVAL_ANIMATION_NATIVE_H','#define REVIVAL_ANIMATION_NATIVE_H',
 '#include "negative_hp_native.h"',
 'typedef struct { const char *name; unsigned party,enemy,fast,frames,party_actions,final_actor,direct,icon,seed,skill,target,own,group,character; uint8_t actions[3],actors[3]; NegativeHpRecord before[6],after[6]; } EnemyAnimationCase;',
 'static const EnemyAnimationCase enemy_animation_cases[] = {']

def records(rs):
    out=[]
    for r in rs:
        values=[r[k] for k in ['hp','maximum','qi','mental','attack','defense','level','experience','attackTimer','defenseTimer']]
        status=[v if v<32768 else v-65536 for v in r['status']]
        out.append('{'+','.join(map(str,values))+',{'+','.join(map(str,status))+'}}')
    return ','.join(out)

commits=phases=actions=0
for row in rows:
    c,ev=row['config'],row['events']
    actors=[i for i,hp in enumerate(c['hp']) if 0<hp<32768]
    assert 1<=len(actors)<=2 and actors[0]==0 and len(ev)==2*len(actors)+1
    assert all(e['records']==ev[0]['records'] for e in ev[:-1])
    command=ev[1]['commands'][0]
    assert command['kind']==1 and command['skill']==c['skill'] and command['type']==19*256
    target=command['target']-1
    assert 0<=target<3 and target not in actors
    frames=len(json.loads(Path('tmp/native-'+c['name']+'-commits.json').read_text()))
    phases+=len(ev)*(c['party']+3)
    actions+=len(actors)
    commits+=frames
    # Native menu30 is the flag/defend icon; menu20 is the magic icon.
    icon=2 if ev[-1]['menu']==30 else 0
    assert ev[-1]['menu'] in (20,30)
    lines.append('{"%s",%u,1,0,%u,%u,%u,0,%u,61,%u,%u,1,0,%u,{8,5},{%s},{%s},{%s}},'%
        (c['name'],c['party'],frames,len(actors),ev[-1]['actor'],icon,c['skill'],target,c['character'],
         ','.join(map(str,actors)),records(ev[0]['records']),records(ev[-1]['records'])))
lines+=['};',f'#define ENEMY_ANIMATION_RECORD_PHASES {phases}',f'#define ENEMY_ANIMATION_ACTIONS {actions}',f'#define ENEMY_ANIMATION_COMMITS {commits}','#endif','']
Path('npe-sdl/tests/revival_animation_native.h').write_text('\n'.join(lines),encoding='utf-8',newline='\n')
