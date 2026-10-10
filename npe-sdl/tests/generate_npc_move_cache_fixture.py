import json
from pathlib import Path

rows=json.loads(Path('tmp/native-npc-move-cache-load.json').read_text(encoding='utf-8'))
def cache(c): return '{'+','.join(str(c[k]) for k in ['npc','nextMode','nextId','x','y'])+'}'
out=['/* Original UI cache results. Unused resident byte+3 is not modeled. */',
     'static const CacheCase cache_cases[]={']
for r in rows:
    values=','.join('{'+','.join(cache(c) for c in r[key])+'}'
                    for key in ['bootCache','beforeCache','afterCache'])
    restart=int(r['entry'] in ['new','empty'])
    out.append('{"'+r['entry']+'-'+str(r['targetMap'])+'",'+str(r['targetMap'])+','+str(restart)+','+values+'},')
out.append('};')
Path('npe-sdl/tests/npc_move_cache_native.h').write_text('\n'.join(out)+'\n',encoding='utf-8')
print(len(rows),'original NPC move cache cases')
