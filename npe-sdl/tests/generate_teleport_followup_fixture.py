import json
from pathlib import Path
rows=json.loads(Path('tmp/native-teleport-followup.json').read_text());assert len(rows)==6
keys=['mode','event','context','map','progress','init','sx','sy','vx','vy','direction','enabled','slot','visited','rng']
def cache(c):return '{'+','.join(str(c[k]) for k in ['map','x','y','marker','mode','nextId'])+'}'
def state(s):
 story=','.join('{'+','.join(str(c[k]) for k in ['next','progress','mode'])+'}' for c in s['storyCache'])
 npc=','.join('{'+','.join(str(n[k]) for k in ['id','x','y','direction'])+'}' for n in s['npc'])
 return '{'+','.join(str(s[k]) for k in keys)+',{'+','.join(cache(c) for c in s['cache'])+'},{'+story+'},{'+','.join(map(str,s['npcState']))+'},{'+npc+'}}'
out=['/* Two unchanged NPE records/four music paths; two next-id overrides. */','static const TeleCase tele_cases[]={']
for r in rows:out.append('{"'+r['name']+'",'+','.join(str(r[k]) for k in ['targetMap','x','y','marker','nextMode','nextId'])+','+str(len(r['commits']))+','+state(r['before'])+','+state(r['after'])+'},')
out+=['};',''];Path('npe-sdl/tests/teleport_followup_native.h').write_text('\n'.join(out))
print('Six teleport/story flows,54 original LCD commits')
