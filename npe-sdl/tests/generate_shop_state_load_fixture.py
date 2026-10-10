import json
from pathlib import Path

rows=json.loads(Path('tmp/native-shop-state-load.json').read_text(encoding='utf-8'))
assert len(rows)==6
fields=['context','npcContext','selected','first','last','row','ui','quantity','choice','descStart','descEnd']
out=['/* Original save/load resident fields. No LCD/position/audio proof. */',
     'typedef struct {const char *name;unsigned title,kind,mode,event,map,progress,'+','.join(fields)+';} ShopLoadCase;',
     'static const ShopLoadCase shop_load_cases[]={']
for r in rows:
 before=r['beforeLoad']['resident'];after=r['after']['resident'];s=r['after']
 assert before==after and s['mode']==1 and s['progress']==1
 values=[int(r['entry']=='title'),after['kind'],s['mode'],s['event'],s['map'],s['progress']]+[after[f] for f in fields]
 out.append('{"'+r['cfg']['name']+'",'+','.join(map(str,values))+'},')
out.append('};')
Path('npe-sdl/tests/shop_state_load_native.h').write_text('\n'.join(out)+'\n',encoding='utf-8')
