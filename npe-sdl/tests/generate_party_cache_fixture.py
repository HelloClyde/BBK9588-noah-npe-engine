import json
from pathlib import Path
rows=json.loads(Path('tmp/native-party-cache-load.json').read_text(encoding='utf-8'))
def cache(c):return '{'+','.join(str(c[k]) for k in ['add','character','nextMode','nextId'])+'}'
out=['/* Original UI save/load cache results; byte+3 is unused and not modeled. */','static const CacheCase cache_cases[]={']
for r in rows:
 out.append('{"'+r['entry']+'-'+str(r['targetMap'])+'",'+str(r['targetMap'])+',{'+','.join(cache(c) for c in r['bootCache'])+'},{'+','.join(cache(c) for c in r['beforeCache'])+'},{'+','.join(cache(c) for c in r['afterCache'])+'}},')
out.append('};')
Path('npe-sdl/tests/party_cache_native.h').write_text('\n'.join(out)+'\n',encoding='utf-8')
print(len(rows),'original cache-load cases')
