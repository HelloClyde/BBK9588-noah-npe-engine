import json
from pathlib import Path
rows=json.loads(Path('tmp/native-party-service.json').read_text(encoding='utf-8'))+json.loads(Path('tmp/native-party-chain.json').read_text(encoding='utf-8'))+json.loads(Path('tmp/native-party-empty.json').read_text(encoding='utf-8'))
fields=['mode','event','map','progress','sx','sy','vx','vy','direction','money','enabled','slot','context','menu','member','equipmentSlot','gearSelected','descLength','descStart','descEnd','menuPartyCount']
def actor(a):
    fields=['id','active','portrait','level','maxHp','hp','maxQi','qi','attack','defense','experience','experienceToNext']
    values=[str(a[k]) for k in fields]+['{'+','.join(map(str,a[k]))+'}' for k in ['otherStats','gear','guards','skills']]
    return '{'+','.join(values)+'}'
def state(s):
    npc=','.join('{'+','.join(str(n[k]) for k in ['id','x','y','direction'])+'}' for n in s['npc'])
    return '{'+','.join(str(s[k]) for k in fields)+',{'+npc+'},{'+','.join(actor(a) for a in s['actors'])+'}}'

out=['/* Original PRG party service with unchanged NPE group-6 cached inputs. */','static const PartyCase party_cases[]={']
for r in rows:
    e=r['event']
    out.append('{"'+r['name']+'",'+str(e['map'])+','+str(e['event'])+','+str(len(r['commits']))+','+state(r['before'])+','+state(r['after'])+'},')
out.append('};')
Path('npe-sdl/tests/party_service_native.h').write_text('\n'.join(out)+'\n',encoding='utf-8')
print(len(rows),'cases,',sum(len(r['commits']) for r in rows),'original commits')
