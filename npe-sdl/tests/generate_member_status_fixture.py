import json
from pathlib import Path
rows=json.loads(Path('tmp/native-member-status-menu.json').read_text(encoding='utf-8'))
fields=['mode','event','map','progress','sx','sy','vx','vy','direction','money','enabled','slot','context','menu','member','equipmentSlot']
def state(s):
    npc=','.join('{'+','.join(str(n[k]) for k in ['id','x','y','direction'])+'}' for n in s['npc'])
    return '{'+','.join(str(s[k]) for k in fields)+',{'+npc+'},{'+','.join('{'+','.join(map(str,m))+'}' for m in s['guards'])+'}}'
out=['/* Original real-boot snapshot, actual property menu input and independently observed resident RNG. */','static const MenuCase menu_cases[]={']
for r in rows:
    phases=','.join('{'+str(p['mask'])+','+str(len(p['commits']))+','+state(p['after'])+'}' for p in r['stages'])
    out.append('{"'+r['name']+'",'+str(r.get('partyCount',1))+',{'+','.join('{'+','.join(map(str,m))+'}' for m in r['guards'])+'},'+state(r['before'])+','+str(r['rngBefore'])+'u,'+str(r['rngAfter'])+'u,'+str(len(r['stages']))+',{'+phases+'}},')
out.append('};')
Path('npe-sdl/tests/member_status_native.h').write_text('\n'.join(out)+'\n',encoding='utf-8')
print(len(rows),'cases,',sum(len(p['commits']) for r in rows for p in r['stages']),'commits,',sum(not p['commits'] for r in rows for p in r['stages']),'ignored inputs')
