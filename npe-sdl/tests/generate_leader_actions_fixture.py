import json
from pathlib import Path

rows=json.loads(Path('tmp/native-leader-actions.json').read_text())
rows=[r for r in rows if r.get('scope')!='numeric-object']
assert len(rows)==7 and sum(len(r['commits']) for r in rows)==11
fields=['mode','event','context','map','progress','init','sx','sy','vx','vy','direction','menuCount','steps']
def array(values):return '{'+','.join(map(str,values))+'}'
def state(s):
 actors='{'+','.join(array(a['raw']) for a in s['actors'])+'}'
 npc='{'+','.join(array(n[k] for k in ['id','x','y','direction']) for n in s['npc'])+'}'
 return '{'+','.join(str(s[k]) for k in fields)+','+actors+','+npc+','+array(s['inventory'][:255])+','+array(s['objects'])+'}'
out=['/* Five original public-input paths plus two source-backed C Enter checks. */','static const LeaderCase leader_cases[]={']
for r in rows:
 phases='{'+','.join('{'+str(p['key'])+','+str(p['commits'])+','+state(p['after'])+'}' for p in r['phases'])+'}'
 out.append('{"'+r['name']+'",'+str(len(r['commits']))+','+str(len(r['phases']))+','+str(int(all(k!=256 for k in r['keys'])))+','+state(r['before'])+','+phases+'},')
out+=['};',''];Path('npe-sdl/tests/leader_actions_native.h').write_text('\n'.join(out))
print('Five original public-input paths/11 LCD;two source-backed Enter checks;unused resident-key object attempt excluded')
