import json
from pathlib import Path

def array(v):return '{'+','.join(map(str,v))+'}'
def state(s):return array(s[k] for k in ('mode','event','map','progress','direction','sx','sy','vx','vy','quantity','flag'))
rows=json.loads(Path('tmp/native-object-story-alias.json').read_text());assert len(rows)==13
out=['/* Original public object flag writes into the adjacent story cache. */','static const ObjectPublicCase object_public_cases[]={']
for row in rows:
    assert len(row['commits']) in (0,2)
    commits='{'+','.join(state(c['state']) for c in row['commits'])+'}' if row['commits'] else '{{0}}'
    out.append('{'+json.dumps(row['name'])+','+str(len(row['commits']))+','+state(row['before'])+','+state(row['after'])+','+commits+','+
               ','.join(array(row[k]) for k in ('objects','flags','npc','monsters','chase','afterObjects','afterFlags'))+','+
               str(row['id'])+','+str(row['itemAlias'])+','+array(row['story'])+','+array(row['afterStory'])+'},')
out.append('};');Path('npe-sdl/tests/object_story_alias_native.h').write_text('\n'.join(out)+'\n')
print('13 original alias paths,',sum(len(r['commits']) for r in rows),'LCD commits')
