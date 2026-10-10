import json
from pathlib import Path
rows=json.loads(Path('tmp/native-combine-cases.json').read_text(encoding='utf-8'))
rows+=json.loads(Path('tmp/native-combine-negative-last.json').read_text(encoding='utf-8'))
rows+=json.loads(Path('tmp/native-combine-last-level.json').read_text(encoding='utf-8'))
rows+=json.loads(Path('tmp/native-combine-fallen-live-enemies.json').read_text(encoding='utf-8'))
assert len(rows)==12 and all('error' not in s for s in rows)
for sample in json.loads(Path('tmp/native-combine-animation.json').read_text(encoding='utf-8')):
 if sample['config']['name'] not in ['combine-one-kill-animation','combine-low-fallen-animation']: continue
 name=sample['config']['name']; sample['config']['name']=name.removesuffix('-animation')
 Path('tmp/native-'+sample['config']['name']+'-2.idx').write_bytes(Path('tmp/native-'+name+'-2.idx').read_bytes())
 rows.append(sample)
rows+=json.loads(Path('tmp/native-combine-repeat.json').read_text(encoding='utf-8'))
lines=['#ifndef COMBINE_NATIVE_H','#define COMBINE_NATIVE_H','#include "negative_hp_native.h"',
 'typedef struct { const char *name; unsigned party, second_fast; NegativeHpRecord before[6], after[6], after_second[6]; unsigned rounds; } CombineCase;',
 'static const CombineCase combine_cases[] = {']
def records(rows):
 result=[]
 for r in rows:
  values=[r[k] for k in ['hp','maximum','qi','mental','attack','defense','level','experience','attackTimer','defenseTimer']]
  st=[v if v<32768 else v-65536 for v in r['status']]
  result.append('{'+','.join(map(str,values))+',{'+','.join(map(str,st))+'}}')
 return ','.join(result)
for s in rows:
 c=s['config'];events=s['events'];rounds=c.get('rounds',1);assert len(events)==1+2*rounds
 assert [e['menu'] for e in events]==[10]+[40,10]*rounds
 assert all(e['mode']==3 and e['actor']==0 for e in events)
 assert events[0]['records']==events[1]['records']
 if rounds==2: assert events[2]['records']==events[3]['records']
 lines.append('{"%s",%u,%u,{%s},{%s},{%s},%u},'%(c['name'],c['party'],int(c.get('secondFast',False)),records(events[0]['records']),records(events[2]['records']),records(events[-1]['records']),rounds))
phases=sum(len(e['records']) for sample in rows for e in sample['events'])
assert len(rows)==16 and phases==309
actions=sum(s['config'].get('rounds',1) for s in rows)
lines+=['};',f'#define COMBINE_RECORD_PHASES {phases}',f'#define COMBINE_ACTIONS {actions}', '#endif','']
Path('npe-sdl/tests/combine_native.h').write_text('\n'.join(lines),encoding='utf-8',newline='\n')
print(f'{len(rows)} original sequences, {actions} completed combined attacks, {phases} full record phases')
