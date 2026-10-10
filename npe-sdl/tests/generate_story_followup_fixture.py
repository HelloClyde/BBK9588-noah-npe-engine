from pathlib import Path
import json

rows=json.loads(Path('tmp/native-story-followup.json').read_text())
out=['/* Original trigger/dialogue -> mode14 -> map, music off/on. */',
     'static const FollowupCase followup_cases[]={']
for i,r in enumerate(rows):
    b,a=r['before'],r['after']
    plays=[e for e in r['audio'] if e['command']=='play']
    assert all(e['track_id']==22 and e['looping'] for e in plays)
    bg=Path(f'tmp/native-story-followup-{i}-before.idx').read_bytes()[0]
    fields=[r[k] for k in ['mode','event','map','progress','music']]+[
        len(r['commits']),a['mode'],a['event'],a['progress'],a['context'],a['npc4'],len(plays),bg,
        b['vx'],b['vy'],b['sx'],b['sy'],b['direction'],b['npc3'],b['npc4']]
    npc=','.join('{'+','.join(str(n[k]) for k in ['id','x','y','direction'])+'}' for n in b['npc'])
    out.append('{"'+r['name']+'",'+','.join(map(str,fields))+',{'+npc+'}},')
out.append('};')
Path('npe-sdl/tests/story_followup_native.h').write_text('\n'.join(out)+'\n')
print(len(rows),sum(len(r['commits']) for r in rows),'original followup commits')
