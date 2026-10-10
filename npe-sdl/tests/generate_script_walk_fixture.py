from pathlib import Path
import json
rows=json.loads(Path('tmp/native-script-walk.json').read_text())
out=['/* Original mode19 commits through the7AB4 map return. */','static const ScriptCase script_cases[]={']
for i,r in enumerate(rows):
 a=r['after'];frame=Path(f'tmp/native-script-walk-{i}-0.idx').read_bytes();bg=frame[0]
 assert all(v==bg for v in frame[:960]+frame[-960:])
 fields=[r[k] for k in ['inputEvent','x','y','targetX','targetY','blockedNpc']]+[len(r['commits']),a['mode'],a['event'],a['progress'],a['vx'],a['vy'],a['sx'],a['sy'],a['direction'],bg]
 npc=','.join('{'+','.join(str(n[k]) for k in ['id','x','y','direction'])+'}' for n in a['npc'])
 out.append('{"'+r['name']+'",'+','.join(map(str,fields))+',{'+npc+'}},')
out.append('};')
Path('npe-sdl/tests/script_walk_native.h').write_text('\n'.join(out)+'\n')
print(len(rows),sum(len(r['commits']) for r in rows),'original diagnostic script walk commits')
