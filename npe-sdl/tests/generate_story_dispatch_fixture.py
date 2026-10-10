from pathlib import Path
import json
r=json.loads(Path('tmp/native-story-dispatch.json').read_text());out=['/* Original mode14 entry/return, including retained and invalid slots. */','static const StoryCase story_cases[]={']
for i,x in enumerate(r):
 a=x['after'];b=x['before'];frame=Path(f'tmp/native-story-dispatch-{i}-0.idx').read_bytes();bg=frame[0]
 assert all(v==bg for v in frame[:960]+frame[-960:]);plays=[e for e in x['audio'] if e['command']=='play']
 assert all(e['track_id']==22 and e['looping'] for e in plays)
 fields=[x[k] for k in ['inputEvent','nextMode','nextId','targetProgress','music']]+[len(x['commits']),a['mode'],a['event'],a['progress'],len(plays),bg,b['vx'],b['vy'],b['sx'],b['sy'],b['direction']]
 npc=','.join('{'+','.join(str(n[k]) for k in ['id','x','y','direction'])+'}' for n in b['npc'])
 out.append('{"'+x['name']+'",'+','.join(map(str,fields))+',{'+npc+'}},')
out.append('};');Path('npe-sdl/tests/story_dispatch_native.h').write_text('\n'.join(out)+'\n');print(len(r),sum(len(x['commits']) for x in r),'original story dispatch commits')
