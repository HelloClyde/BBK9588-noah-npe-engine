import json
from pathlib import Path
rows=json.loads(Path('tmp/native-scene-resident-public.json').read_text());assert len(rows)==12
def array(v):return '{'+','.join(map(str,v))+'}'
def state(s):
 fields=['mode','event','map','progress','init','sx','sy','vx','vy','direction']
 npc=','.join('{'+','.join(str(n[k]) for k in ['id','x','y','direction'])+'}' for n in s['npc'])
 return '{'+','.join(str(s[k]) for k in fields)+',{'+npc+'}}'
out=['/* Original public mode15,unmodified PRG/NPE; diagnostic resident slots. */','static const SceneResidentCase scene_resident_cases[]={']
for r in rows:
 assert len(r['beforeBytes'])==len(r['afterBytes'])==80
 out.append('{'+json.dumps(r['name'])+','+str(len(r['commits']))+','+str(r['serviceCommits'])+','+state(r['before'])+','+state(r['after'])+','+array(r['beforeBytes'])+','+array(r['afterBytes'])+'},')
out.append('};');Path('npe-sdl/tests/scene_resident_public_native.h').write_text('\n'.join(out)+'\n')
print('12 original mode15 paths,%d LCD commits'%sum(len(r['commits']) for r in rows))
