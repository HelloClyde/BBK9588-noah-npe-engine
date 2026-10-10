import json
from pathlib import Path

out=['/* Original advance-end7A9E; the inspected action controller has no writes. */',
     'typedef struct {const char *name;unsigned frames;uint8_t actors[56];} MenuActorCase;',
     'static const MenuActorCase menu_actor_cases[]={']
paths=frames=0
for stem,count in [('group-animation',6),('attack-sequence',2),('combine-animation',6),('combine-repeat',2)]:
    rows=json.loads(Path('tmp/native-'+stem+'-menu-actor.json').read_bytes())
    assert len(rows)==count
    for row in rows:
        name=row['config']['name'];old=name.removesuffix('-menu-actor')
        key=old.removesuffix('-animation') if stem.startswith('combine') else old
        commits=json.loads(Path('tmp/native-'+name+'-commits.json').read_bytes())
        assert 0<len(commits)<=56
        # Inspect and retain every captured value; no host expected actor injection.
        assert all(c['menuActor']==0 for c in commits),name
        out.append('{'+json.dumps(key)+','+str(len(commits))+',{'+','.join(str(c['menuActor']) for c in commits)+'}},')
        paths+=1;frames+=len(commits)
assert paths==16 and frames==490
out.append('};')
Path('npe-sdl/tests/battle_menu_actor_native.h').write_text('\n'.join(out)+'\n',encoding='utf8')
print('16 original paths,490 menu actor values')
