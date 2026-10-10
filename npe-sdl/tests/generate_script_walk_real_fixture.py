from pathlib import Path
import json
rows=json.loads(Path('tmp/native-script-walk-real.json').read_text());out=['/* Actual current NPE map126 mode19 -> story181 -> map1. */','static const RealCase real_cases[]={']
for r in rows:
 a=r['after'];frame=Path(f"tmp/native-script-walk-real-{r['music']}-0.idx").read_bytes();bg=frame[0]
 assert all(v==bg for v in frame[:960]+frame[-960:])
 plays=[e for e in r['audio'] if e['command']=='play'];assert all(e['track_id']==22 and e['looping'] for e in plays)
 fields=[r['music'],len(r['commits']),a['mode'],a['event'],a['progress'],len(plays),bg,a['vx'],a['vy'],a['sx'],a['sy'],a['direction']]
 npc=','.join('{'+','.join(str(n[k]) for k in ['id','x','y','direction'])+'}' for n in a['npc'])
 cache=','.join('{'+','.join(str(c[k]) for k in ['x','y','nextId','marker','nextMode'])+'}' for c in r['beforeCache'])
 out.append('{'+','.join(map(str,fields))+',{'+npc+'},{'+cache+'}},')
out.append('};');Path('npe-sdl/tests/script_walk_real_native.h').write_text('\n'.join(out)+'\n')
print('Original real mode19:',sum(len(r['commits']) for r in rows),'commits')
