import json
from pathlib import Path
rows=json.loads(Path('tmp/native-money-service.json').read_text())
assert len(rows)==18
fields=['mode','event','context','map','progress','money','init','sx','sy','vx','vy','direction','enabled','slot']
def state(s):
    npc=','.join('{'+','.join(str(n[k]) for k in ['id','x','y','direction'])+'}' for n in s['npc'])
    return '{'+','.join(str(s[k]) for k in fields)+',{'+npc+'}}'
out=['/* Original money service with diagnostic five-slot cached records. */','static const MoneyCase money_cases[]={']
for r in rows:
    out.append('{"'+r['name']+'",'+','.join(str(r[k]) for k in ['amount','adding'])+','+str(len(r['commits']))+','+state(r['before'])+','+state(r['after'])+'},')
out+=['};','']
Path('npe-sdl/tests/money_service_native.h').write_text('\n'.join(out))
rows=json.loads(Path('tmp/native-money-cache-load.json').read_text())
assert len(rows)==8
def cache(c):return '{'+','.join(str(c[k]) for k in ['amount','nextId','adding','mode'])+'}'
out=['/* Original UI money cache partial map loads; padding is unused. */','static const MoneyCacheCase money_cache_cases[]={']
for r in rows:
    out.append('{"'+r['entry']+'-'+str(r['targetMap'])+'",'+str(r['targetMap'])+','+str(int(r['entry'] in ['new','empty']))+','+','.join('{'+','.join(cache(c) for c in r[k])+'}' for k in ['bootCache','beforeCache','afterCache'])+'},')
out+=['};','']
Path('npe-sdl/tests/money_cache_native.h').write_text('\n'.join(out))
print('18 service cases,8 cache cases')
