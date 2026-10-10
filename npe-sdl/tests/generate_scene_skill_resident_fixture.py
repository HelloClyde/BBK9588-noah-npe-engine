import json
from pathlib import Path
rows=json.loads(Path('tmp/native-scene-skill-resident-load.json').read_text());assert len(rows)==16
def array(v):return '{'+','.join(map(str,v))+'}'
out=['/* Original scene80 + skill12 + leader4, including retained padding. */','static const SceneSkillLoadCase scene_skill_load_cases[]={']
for row in rows:
 b,a=row['beforeBytes'],row['afterBytes'];assert len(b)==len(a)==96
 assert all(b[i*16+7]==a[i*16+7] for i in range(5))
 assert b[85]==a[85] and b[91]==a[91]
 out.append('{'+json.dumps(row['entry'])+','+str(row['targetMap'])+','+array(b)+','+array(a)+'},')
out.append('};');Path('npe-sdl/tests/scene_skill_resident_load_native.h').write_text('\n'.join(out)+'\n')
print('16 original loads/1536 complete bytes;7 padding bytes retained')
