import json
from pathlib import Path
rows=json.loads(Path('tmp/native-shop-control-retention.json').read_text(encoding='utf-8'))
fields=['context','npcContext','kind','sellFlag','selected','first','last','row','ui','quantity','choice','descStart','descEnd']
out=['/* Original warm-load controls; late restart observations are not used. */',
     'typedef struct {unsigned '+','.join(fields)+';} ShopControlLoad;',
     'static const ShopControlLoad shop_control_load[]={']
assert len(rows)==6
for r in rows:
    a=r['beforeLoad']['resident'];b=r['after']['resident'];assert a==b
    out.append('{'+','.join(str(b[f]) for f in fields)+'},')
out.append('};')
Path('npe-sdl/tests/shop_control_load_native.h').write_text('\n'.join(out)+'\n',encoding='utf-8')
restart=json.loads(Path('tmp/shop-control-restart-native/report.json').read_text(encoding='utf-8'))
assert restart['original_only'] and len(restart['rows'])==6
assert all(r['before']['sellFlag']==r['after']['sellFlag']==r['salt'] for r in restart['rows'])
print('6 original warm loads and6 constructor outcomes verified')
