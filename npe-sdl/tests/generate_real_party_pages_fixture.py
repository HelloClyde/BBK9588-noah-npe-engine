import json
from pathlib import Path
rows=json.loads(Path('tmp/native-real-party-menu.json').read_text(encoding='utf-8'))
fields=['mode','event','map','progress','sx','sy','vx','vy','direction','money','enabled','slot','context','menu','member','equipmentSlot','gearSelected','descLength','descStart','descEnd','menuPartyCount']
def actor(a):
    fields=['id','active','portrait','level','maxHp','hp','maxQi','qi','attack','defense','experience','experienceToNext']
    values=[str(a[k]) for k in fields]+['{'+','.join(map(str,a[k]))+'}' for k in ['otherStats','gear','guards','skills']]
    return '{'+','.join(values)+'}'
def state(s):
    npc=','.join('{'+','.join(str(n[k]) for k in ['id','x','y','direction'])+'}' for n in s['npc'])
    return '{'+','.join(str(s[k]) for k in fields)+',{'+npc+'},{'+','.join(actor(a) for a in s['actors'])+'}}'
out=['/* Original real-boot snapshot, actual property menu input and independently observed resident RNG. */','static const MenuCase menu_cases[]={']
for r in rows:
    phases=','.join('{'+str(p['mask'])+','+str(len(p['commits']))+','+state(p['after'])+'}' for p in r['stages'])
    out.append('{"'+r['name']+'",'+str(int(r.get('beforeEntry',False)))+',{'+','.join(map(str,r['active']))+'},'+state(r['before'])+','+str(r['rngBefore'])+'u,'+str(r['rngAfter'])+'u,'+str(len(r['stages']))+',{'+phases+'}},')
out.append('};')
Path('npe-sdl/tests/real_party_pages_native.h').write_text('\n'.join(out)+'\n',encoding='utf-8')
print(len(rows),'cases,',sum(len(p['commits']) for r in rows for p in r['stages']),'commits,',sum(not p['commits'] for r in rows for p in r['stages']),'ignored inputs')
