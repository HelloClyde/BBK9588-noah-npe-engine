import json
from pathlib import Path
rows=json.loads(Path('tmp/native-teleport-service.json').read_text());assert len(rows)==12
keys=['mode','event','context','map','progress','init','sx','sy','vx','vy','direction','enabled','slot','visited','rng']
def cache(c):return '{'+','.join(str(c[k]) for k in ['map','x','y','marker','mode','nextId'])+'}'
def state(s):return '{'+','.join(str(s[k]) for k in keys)+',{'+','.join(cache(c) for c in s['cache'])+'}}'
out=['/* Unmodified original firmware; diagnostic two-slot service inputs. */','static const TeleCase tele_cases[]={']
for r in rows:
 out.append('{"'+r['name']+'",'+','.join(str(r[k]) for k in ['targetMap','x','y','marker','nextMode','nextId'])+','+str(len(r['commits']))+','+state(r['before'])+','+state(r['after'])+'},')
out+=['};',''];Path('npe-sdl/tests/teleport_service_native.h').write_text('\n'.join(out))
rows=json.loads(Path('tmp/native-teleport-cache-load.json').read_text());assert len(rows)==10
out=['/* Original UI group5 partial map loads; reserved+2/+3 remain unchanged. */','static const CacheCase cache_cases[]={']
for r in rows:
 assert all(a['reserved']==b['reserved'] for a,b in zip(r['beforeCache'],r['afterCache']))
 out.append('{"'+r['entry']+'-'+str(r['targetMap'])+'",'+str(r['targetMap'])+','+str(int(r['entry'] in ['new','empty']))+','+','.join('{'+','.join(cache(c) for c in r[k])+'}' for k in ['bootCache','beforeCache','afterCache'])+'},')
out+=['};',''];Path('npe-sdl/tests/teleport_cache_native.h').write_text('\n'.join(out))
print('12 original service paths/36 LCD commits,10 original UI cache loads')
