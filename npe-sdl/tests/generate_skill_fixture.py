import json
from pathlib import Path
rows=json.loads(Path('tmp/native-skill-service.json').read_text());assert len(rows)==21
assert sum(len(r['commits']) for r in rows)==58
for r in rows:
 assert [a['id'] for a in r['before']['actors']]==([1,1,3] if r['name']=='duplicate-id' else [1,2,3])
 assert r['before']['cache']==r['after']['cache']
fields=['mode','event','context','map','progress','init','sx','sy','vx','vy','direction','enabled','slot']
def cache(c):return '{'+','.join(str(c[k]) for k in ['nextId','skill','character','mode'])+'}'
def state(s):
 npc=','.join('{'+','.join(str(n[k]) for k in ['id','x','y','direction'])+'}' for n in s['npc'])
 actors=','.join('{'+str(a['id'])+','+str(a['active'])+',{'+','.join(map(str,a['skills']))+'}}' for a in s['actors'])
 return '{'+','.join(str(s[k]) for k in fields)+',{'+npc+'},{'+actors+'}}'
out=['/* Three original NPE records and eighteen diagnostic paths. */','static const SkillCase skill_cases[]={']
for r in rows:
 out.append('{"'+r['name']+'",'+str(int(r.get('real',False)))+','+str(len(r['commits']))+','+state(r['before'])+','+state(r['after'])+',{'+','.join(cache(c) for c in r['before']['cache'])+'},{'+','.join(map(str,r['after']['skillName']))+'}},')
out+=['};',''];Path('npe-sdl/tests/skill_service_native.h').write_text('\n'.join(out))
rows=json.loads(Path('tmp/native-skill-cache-load.json').read_text());assert len(rows)==8
out=['/* Original UI group15 partial loads, padding remains unchanged. */','static const SkillCacheCase skill_cache_cases[]={']
for r in rows:
 assert all(a['padding']==b['padding'] for a,b in zip(r['beforeCache'],r['afterCache']))
 out.append('{"'+r['entry']+'-'+str(r['targetMap'])+'",'+str(r['targetMap'])+','+str(int(r['entry'] in ['new','empty']))+','+','.join('{'+','.join(cache(c) for c in r[k])+'}' for k in ['bootCache','beforeCache','afterCache'])+'},')
out+=['};',''];Path('npe-sdl/tests/skill_cache_native.h').write_text('\n'.join(out))
print('21 service paths/58 LCD commits,8 cache cases')
