from pathlib import Path
import json
rows=json.loads(Path('tmp/native-story-cache-load.json').read_text())
out=['/* Original system/title warm loads and new/empty constructors. */','static const StoryCacheCase story_cache_cases[]={']
for r in rows:
 groups=[]
 for key in ['bootCache','beforeCache','afterCache']:
  groups.append('{'+','.join('{'+','.join(str(e[k]) for k in ['progress','mode','nextId'])+'}' for e in r[key])+'}')
 out.append('{"'+r['entry']+'-'+str(r['targetMap'])+'",'+str(r['targetMap'])+','+str(int(r['entry'] in ['new','empty']))+','+','.join(groups)+'},')
out.append('};');Path('npe-sdl/tests/story_cache_native.h').write_text('\n'.join(out)+'\n')
print('Generated',len(rows),'original 15-slot UI cases')
