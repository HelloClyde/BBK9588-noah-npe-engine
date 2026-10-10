import json
from pathlib import Path

all_path=Path('tmp/native-queued-negative-hp-all.json')
if all_path.exists():
 queued=json.loads(all_path.read_text(encoding='utf-8'))
 samples=[s for side in [0,1] for s in queued if s['config']['side']==side]
else:
 # Retain the first successful enemy samples and corrected friendly samples.
 old=json.loads(Path('tmp/native-queued-negative-hp.json').read_text(encoding='utf-8'))
 samples=[s for s in old if s['config']['side']==0]
 samples+=json.loads(Path('tmp/native-queued-negative-hp-friend.json').read_text(encoding='utf-8'))
samples+=[json.loads(Path(f'tmp/native-group-negative-hp-{hp}.json').read_text(encoding='utf-8'))[0] for hp in [65535,32768]]
assert len(samples)==8 and all('error' not in s for s in samples)
lines=['/* Completed independent original input sequences and resident-resource fixtures. */',
 '#ifndef DYNAMIC_HP_NATIVE_H','#define DYNAMIC_HP_NATIVE_H','#include "negative_hp_native.h"',
 'typedef struct { const char *name; unsigned kind, phases, seed, changed_hp; } DynamicHpCase;',
 'static const DynamicHpCase dynamic_hp_cases[] = {']
for s in samples:
 c=s['config'];kind=2 if c['name'].startswith('group-') else c['side']
 s['kind']=kind
 assert len(s['events'])==(4 if kind!=1 else 5)
 if kind==2: assert [p['skill'] for p in s['events']]==[0,0,0,6]
 assert all(e['mode']==3 for e in s['events'])
 lines.append('{"%s",%u,%u,%u,%u},' % (c['name'],kind,len(s['events']),128 if kind==2 else 4,c['hp']))
lines+=['};','typedef struct { unsigned scenario, phase, actor; NegativeHpRecord records[6]; } DynamicHpPhase;',
 'static const DynamicHpPhase dynamic_hp_phases[] = {']
def records(rows):
 result=[]
 for r in rows:
  v=[r[k] for k in ['hp','maximum','qi','mental','attack','defense','level','experience','attackTimer','defenseTimer']]
  st=[x if x<32768 else x-65536 for x in r['status']]
  result.append('{'+','.join(map(str,v))+',{'+','.join(map(str,st))+'}}')
 return ','.join(result)
for i,s in enumerate(samples):
 for j,e in enumerate(s['events']):lines.append('{%u,%u,%u,{%s}},' % (i,j,e['actor'],records(e['records'])))
lines+=['};','#endif','']
Path('npe-sdl/tests/dynamic_hp_native.h').write_text('\n'.join(lines),encoding='utf-8',newline='\n')
print(f'8 original input sequences, {sum(len(s["events"])*6 for s in samples)} complete record phases')
