from pathlib import Path
import json

rows=json.loads(Path('tmp/native-item-animation.json').read_text(encoding='utf-8'))
assert len(rows)==8
lines=['#ifndef ITEM_ANIMATION_NATIVE_H','#define ITEM_ANIMATION_NATIVE_H',
 '#include "negative_hp_native.h"',
 'typedef struct { const char *name; unsigned party,enemy,fast,frames,party_actions,final_actor,direct,icon,seed,item,target,own,group,character; uint8_t actions[3],actors[3]; NegativeHpRecord before[6],after[6]; } ItemAnimationCase;',
 'static const ItemAnimationCase item_animation_cases[] = {']

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
    assert actors[0]==0 and len(ev)==2*len(actors)+1
    assert all(e['records']==ev[0]['records'] for e in ev[:-1])
    assert ev[0]['quantity']==1 and all(e['quantity']==0 for e in ev[2:])
    assert ev[1]['menu']==(33 if c['group'] else 37)
    submitted=json.loads(Path('tmp/native-'+c['name']+'-commits.json').read_text())
    command=submitted[0]['command']
    assert command['kind']==2 and command['item']==c['item']
    target=command['target']-1
    assert 0<=target<3
    if len(actors)>1:assert ev[2]['commands'][0]['target']==target+1
    frames=len(submitted)
    phases+=len(ev)*(c['party']+3)
    actions+=len(actors)
    commits+=frames
    assert ev[-1]['menu']==30
    lines.append('{"%s",%u,1,0,%u,%u,%u,0,2,61,%u,%u,1,%u,%u,{2,5,5},{%s},{%s},{%s}},'%
        (c['name'],c['party'],frames,len(actors),ev[-1]['actor'],c['item'],target,c['group'],c['character'],
         ','.join(map(str,actors)),records(ev[0]['records']),records(ev[-1]['records'])))
lines+=['};',f'#define ITEM_ANIMATION_RECORD_PHASES {phases}',f'#define ITEM_ANIMATION_ACTIONS {actions}',f'#define ITEM_ANIMATION_COMMITS {commits}','#endif','']
Path('npe-sdl/tests/item_animation_native.h').write_text('\n'.join(lines),encoding='utf-8',newline='\n')
