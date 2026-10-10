from pathlib import Path
import json
rows=json.loads(Path('tmp/walk-live-cache-native.json').read_text(encoding='utf-8'))['rows']
out=['/* Original live viewport words and resulting refresh. */', 'static const WalkCacheCase walk_cache_cases[]={']
for c in rows:
    arrays=['{'+','.join(map(str,c[s]['cache']))+'}' for s in ['original','before','after']]
    after=c['after']
    out.append('{"%s","%s",%d,%d,%d,%d,{%s},%s},'%(
        c['name'],c['prefix'],[2,0,3,1][{1:0,2:1,4:2,8:3}[c['mask']]],len(c['frames']),c['lower'],c['upper'],
        ','.join(map(str,after['view'])),','.join(arrays)))
out.append('};')
Path('npe-sdl/tests/walk_live_cache_native.h').write_text('\n'.join(out)+'\n',encoding='utf-8')
print('original live-cache cases',len(rows),'frames',sum(len(c['frames']) for c in rows))
