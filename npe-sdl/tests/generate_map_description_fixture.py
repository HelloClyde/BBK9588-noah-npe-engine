import json
from pathlib import Path
rows=json.loads(Path('tmp/native-map-description-service.json').read_text(encoding='utf-8'))
fields=['mode','event','page','member','selected','length','start','end','itemLoaded','skillLoaded']
def arr(a):return '{'+','.join(map(str,a))+'}'
def state(s):
 p=s['party'];party=[p[k] for k in ['hp','maxHp','qi','maxQi','attack','defense','level','experience','next']]+p['other']+p['status']
 return '{'+','.join(map(str,[s[f] for f in fields]))+','+arr(s['itemBytes'])+','+arr(s['skillBytes'])+','+arr(party)+'}'
lines=['/* Original map item/skill description controllers. Menu7A54 is not a bound field. */',
 'typedef struct {unsigned '+','.join(fields)+';uint8_t itemBytes[142],skillBytes[144];unsigned party[16];} MapDescriptionState;',
 'typedef struct {unsigned key,patch[3],count,index;MapDescriptionState before,after,frame;} MapDescriptionPhase;',
 'typedef struct {const char *name;unsigned skill,id,count,commits;MapDescriptionState before;unsigned npc[20][4];MapDescriptionPhase phases[4];} MapDescriptionCase;',
 'static const MapDescriptionCase map_description_cases[]={']
for c in rows:
 phases=[]
 for p in c['phases']:
  assert len(p['commits'])<=1
  patch=[p.get('patch',{}).get(k,4294967295) for k in ['length','start','end']]
  frame=p['commits'][0] if p['commits'] else {'index':0,'state':p['after']}
  phases.append('{'+','.join([str(p.get('key',p.get('mask'))),arr(patch),str(len(p['commits'])),str(frame['index']),state(p['before']),state(p['after']),state(frame['state'])])+'}')
 lines.append('{"'+c['name']+'",'+str(int(c['kind']=='skill'))+','+str(c['id'])+','+str(len(phases))+','+str(c['commits'])+','+state(c['before'])+','+arr([arr([n[k] for k in ['id','x','y','direction']]) for n in c['before']['npc']])+',{'+','.join(phases)+'}},')
lines+=['};',''];Path('npe-sdl/tests/map_description_native.h').write_text('\n'.join(lines),encoding='utf-8');print(len(rows),'fixtures')
