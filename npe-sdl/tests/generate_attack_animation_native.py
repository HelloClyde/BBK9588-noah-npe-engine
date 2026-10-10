from pathlib import Path
import json
rows=json.loads(Path('tmp/native-attack-animation.json').read_text(encoding='utf-8'))
assert len(rows)==5
lines=['#ifndef ATTACK_ANIMATION_NATIVE_H','#define ATTACK_ANIMATION_NATIVE_H','#include "negative_hp_native.h"','typedef struct { const char *name; unsigned party; NegativeHpRecord before[6],after[6]; } AttackCase;','static const AttackCase attack_animation_cases[] = {']
def records(rr):
 out=[]
 for r in rr:
  vv=[r[k] for k in ['hp','maximum','qi','mental','attack','defense','level','experience','attackTimer','defenseTimer']]
  st=[x if x<32768 else x-65536 for x in r['status']]
  out.append('{'+','.join(map(str,vv))+',{'+','.join(map(str,st))+'}}')
 return ','.join(out)
for r in rows:
 assert len(r['events'])==3 and len(r['events'][0]['records'])==r['config']['party']+3
 assert r['events'][0]['records']==r['events'][1]['records']
 assert all(e['mode']==3 and e['actor']==0 for e in r['events'])
 assert len(json.loads(Path('tmp/native-'+r['config']['name']+'-commits.json').read_text(encoding='utf-8')))==15
 lines.append('{"%s",%u,{%s},{%s}},'%(r['config']['name'],r['config']['party'],records(r['events'][0]['records']),records(r['events'][2]['records'])))
lines+=['};',f'#define ATTACK_RECORD_PHASES {sum(len(e["records"]) for r in rows for e in r["events"])}',f'#define ATTACK_ACTIONS {len(rows)}','#endif','']
Path('npe-sdl/tests/attack_animation_native.h').write_text('\n'.join(lines),encoding='utf-8',newline='\n')
