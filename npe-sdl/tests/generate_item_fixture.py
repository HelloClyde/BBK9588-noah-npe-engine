import json
from pathlib import Path
rows=json.loads(Path('tmp/native-item-service.json').read_text());assert len(rows)==22
fields=['mode','event','context','map','progress','quantity','init','sx','sy','vx','vy','direction','enabled','slot']
def state(s):
    npc=','.join('{'+','.join(str(n[k]) for k in ['id','x','y','direction'])+'}' for n in s['npc'])
    return '{'+','.join(str(s[k]) for k in fields)+',{'+npc+'}}'
out=['/* Original item service; two unchanged NPE records,twenty diagnostic cache inputs. */','static const ItemCase item_cases[]={']
for r in rows:
    out.append('{"'+r['name']+'",'+','.join(str(r[k]) for k in ['item','quantity','adding'])+','+str(r.get('record',{}).get('nextId',789))+','+str(int(r.get('real_npe_record',False)))+','+str(len(r['commits']))+','+state(r['before'])+','+state(r['after'])+',{'+','.join(map(str,r['after']['itemName']))+'}},')
out+=['};',''];Path('npe-sdl/tests/item_service_native.h').write_text('\n'.join(out))
rows=json.loads(Path('tmp/native-item-cache-load.json').read_text());assert len(rows)==8
def cache(c):return '{'+','.join(str(c[k]) for k in ['quantity','item','nextId','adding','mode'])+'}'
out=['/* Original UI item cache partial map loads. */','static const ItemCacheCase item_cache_cases[]={']
for r in rows:
    out.append('{"'+r['entry']+'-'+str(r['targetMap'])+'",'+str(r['targetMap'])+','+str(int(r['entry'] in ['new','empty']))+','+','.join('{'+','.join(cache(c) for c in r[k])+'}' for k in ['bootCache','beforeCache','afterCache'])+'},')
out+=['};',''];Path('npe-sdl/tests/item_cache_native.h').write_text('\n'.join(out))
print('22 service cases,8 cache cases')
