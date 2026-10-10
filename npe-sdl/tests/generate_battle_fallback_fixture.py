import json
import sys
from pathlib import Path
def array(v):return '{'+','.join(map(str,v))+'}'
zero_event='--zero-event' in sys.argv
stem='battle_zero_event' if zero_event else 'battle_fallback_entry'
prefix='native-battle-zero-event' if zero_event else 'native-battle-fallback-entry'
if not zero_event:
 loads=json.loads(Path('tmp/native-random-enemy-resident-load.json').read_text());assert len(loads)==16
 out=['/* Original public map load:all eight random enemy bytes. */','static const RandomEnemyLoadCase random_enemy_load_cases[]={']
 for r in loads:
  assert len(r['beforeBytes'])==len(r['afterBytes'])==8
  out.append('{'+json.dumps(r['entry'])+','+str(r['targetMap'])+','+array(r['beforeBytes'])+','+array(r['afterBytes'])+'},')
 out.append('};');Path('npe-sdl/tests/random_enemy_resident_load_native.h').write_text('\n'.join(out)+'\n')
rows=json.loads(Path('tmp/'+prefix+'.json').read_text());assert len(rows)==(4 if zero_event else 13)
out=['/* Original unmatched formation dispatch,retained IDs and shared RNG. */','static const ScriptEntryCase script_cases[]={']
for r in rows:
 c,a,t=r['config'],r['after'],r['beforeTables'];assert len(r['commits'])==4
 assert t==r['afterTables']
 records='{'+','.join('{'+','.join(str(s[k]) for k in ['hp','maxHp','qi','maxQi','attack','defense','speed'])+','+array(s['status'])+'}' for s in a['records'])+'}'
 out.append('{'+json.dumps(c['name'])+','+str(len(c['hps']))+','+array(c['hps'])+','+array(a['ids'])+','+records+','+','.join(str(c[k]) for k in ['event','winMode','winEvent','loseMode','loseEvent'])+','+array(t['battle'])+','+array(t['monster'])+','+str(r['address'])+','+array(t['alias'])+','+','.join(str(v) for v in [a['count'],c['context'],c['seed'],a['rng']])+','+array(t['random'])+','+array(t['chase'])+','+str(c.get('chaseEnemy',0))+'},')
out.append('};');Path('npe-sdl/tests/'+stem+'_native.h').write_text('\n'.join(out)+'\n')
print('4 original zero-event entries/16 LCD commits' if zero_event else '16 original random enemy loads/128 bytes,13 original fallback entries/52 LCD commits')
