from pathlib import Path
import json

rows=json.loads(Path('tmp/native-party-spell-animation.json').read_text(encoding='utf-8'))
assert len(rows)==3
extra=json.loads(Path('tmp/native-party-spell-extra.json').read_text(encoding='utf-8'))
assert len(extra)==3
rows+=extra
lines=['#ifndef PARTY_SPELL_ANIMATION_NATIVE_H','#define PARTY_SPELL_ANIMATION_NATIVE_H',
 '#include "negative_hp_native.h"',
 'typedef struct { const char *name; unsigned party,enemy,fast,frames,party_actions,final_actor,direct,icon,seed,skill,target,own,group,character; uint8_t actions[3]; NegativeHpRecord before[6],after[6]; } EnemyAnimationCase;',
 'static const EnemyAnimationCase enemy_animation_cases[] = {']

def records(rs):
 out=[]
 for r in rs:
  values=[r[k] for k in ['hp','maximum','qi','mental','attack','defense','level','experience','attackTimer','defenseTimer']]
  status=[v if v<32768 else v-65536 for v in r['status']]
  out.append('{'+','.join(map(str,values))+',{'+','.join(map(str,status))+'}}')
 return ','.join(out)

commits=phases=actions=0
for r in rows:
 c,ev=r['config'],r['events'];assert len(ev)==2*c['party']+1
 assert all(e['records']==ev[0]['records'] for e in ev[:-1])
 command=ev[2]['commands'][0];assert command['kind']==1 and command['skill']==c['skill']
 target=command['target']-1 if c['own'] else command['target']-c['party']-1
 assert 0<=target<3
 frames=len(json.loads(Path('tmp/native-'+c['name']+'-commits.json').read_text()))
 phases+=len(ev)*(c['party']+3);actions+=c['party'];commits+=frames
 lines.append('{"%s",%u,1,0,%u,%u,%u,0,2,61,%u,%u,%u,%u,%u,{8,5,5},{%s},{%s}},'%
  (c['name'],c['party'],frames,c['party'],ev[-1]['actor'],c['skill'],target,c['own'],c['group'],c.get('character',1),records(ev[0]['records']),records(ev[-1]['records'])))
lines+=['};',f'#define ENEMY_ANIMATION_RECORD_PHASES {phases}',f'#define ENEMY_ANIMATION_ACTIONS {actions}',f'#define ENEMY_ANIMATION_COMMITS {commits}','#endif','']
Path('npe-sdl/tests/party_spell_animation_native.h').write_text('\n'.join(lines),encoding='utf-8',newline='\n')
