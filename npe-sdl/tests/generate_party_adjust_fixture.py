import json
from pathlib import Path

rows=json.loads(Path('tmp/native-party-adjust.json').read_text())
assert len(rows)==21 and sum(len(r['commits']) for r in rows)==33
fields=['mode','event','context','map','progress','init','sx','sy','vx','vy','direction','enabled','slot','menuCount']
def state(s):
 actors=','.join('{'+','.join(map(str,a['raw']))+'}' for a in s['actors'])
 npc=','.join('{'+','.join(str(n[k]) for k in ['id','x','y','direction'])+'}' for n in s['npc'])
 return '{'+','.join(str(s[k]) for k in fields)+',{'+actors+'},{'+npc+'}}'
def cache(c):return '{'+','.join(str(c[k]) for k in ['character','mode','next'])+'}'
out=['/* Original mode13 service: diagnostic cache inputs and map97 empty cache. */','static const AdjustCase adjust_cases[]={']
for r in rows:
 before=r['before'];after=r['after'];assert before['cache']==after['cache']
 expected=[a['raw'][:] for a in before['actors']]
 eligible=[i for i in [1,2] if before['actors'][i]['id']==before['cache']['character'] and before['actors'][i]['active']==1]
 changed=(before['event']&255)==1 and bool(eligible)
 if changed:
  slot=eligible[0];expected[0],expected[slot]=expected[slot],expected[0]
  expected[0][15:17]=before['actors'][0]['raw'][15:17]
 assert expected==[a['raw'] for a in after['actors']]
 out.append('{"'+r['name']+'",'+str(len(r['commits']))+','+str(int(changed))+','+cache(before['cache'])+','+state(before)+','+state(after)+'},')
out+=['};',''];Path('npe-sdl/tests/party_adjust_native.h').write_text('\n'.join(out))
rows=json.loads(Path('tmp/native-party-adjust-cache.json').read_text());assert len(rows)==8
out=['/* Original UI loads of current NPE empty group3 retain the resident slot. */','static const AdjustCacheCase adjust_cache_cases[]={']
for r in rows:
 assert r['beforeCache']==r['afterCache']
 out.append('{"'+r['entry']+'-'+str(r['targetMap'])+'",'+str(r['targetMap'])+','+str(int(r['entry'] in ['new','empty']))+','+','.join(cache(r[k]) for k in ['bootCache','beforeCache','afterCache'])+'},')
out+=['};',''];Path('npe-sdl/tests/party_adjust_cache_native.h').write_text('\n'.join(out))
print('21 original short paths/33 LCD submissions; 8 original UI cache loads; complete148-byte record swap evidence validated')
