import json
from pathlib import Path
rows=json.loads(Path('tmp/native-item-resident-public.json').read_text());assert len(rows)==16
fields=['mode','event','context','map','progress','quantity','init','sx','sy','vx','vy','direction','enabled','slot']
def state(s):
 npc=','.join('{'+','.join(str(n[k]) for k in ['id','x','y','direction'])+'}' for n in s['npc'])
 return '{'+','.join(str(s[k]) for k in fields)+',{'+npc+'}}'
def array(v):return '{'+','.join(map(str,v))+'}'
out=['/* Original public mode12,full-word inventory aliases and all80 table bytes. */','static const ItemCase item_cases[]={']
for r in rows:
 out.append('{"'+r['name']+'",'+','.join(str(r[k]) for k in ['item','quantity','adding'])+','+str(r['record']['nextId'])+',0,'+str(len(r['commits']))+','+state(r['before'])+','+state(r['after'])+','+array(r['after']['itemName'])+'},')
out+=['};','static const ItemResidentCase item_resident_cases[]={']
for r in rows:
 assert len(r['beforeBytes'])==len(r['afterBytes'])==80
 out.append('{'+array(r['beforeBytes'])+','+array(r['afterBytes'])+'},')
out.append('};');Path('npe-sdl/tests/item_resident_public_native.h').write_text('\n'.join(out)+'\n')
print('16 public mode12 paths,%d original LCD commits'%sum(len(r['commits']) for r in rows))
