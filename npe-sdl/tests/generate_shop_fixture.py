import json
from pathlib import Path
rows=json.loads(Path('tmp/native-shop.json').read_text(encoding='utf8'))['rows']
assert len(rows)==8
out=['/* Original mode4 quantity-confirm outcomes; no C-generated expectations. */',
     'static const ShopCase shop_cases[]={']
for r in rows:
 b,a=r['before'],r['after']
 assert b['selected']==45 and b['price']==10
 out.append('{"'+r['name']+'",'+','.join(str(v) for v in
  (b['money'],b['owned'],b['quantity'],b['sell'],a['money'],a['owned'],int(a['ui']==1)))+'},')
out.append('};')
Path('npe-sdl/tests/shop_native.h').write_text('\n'.join(out)+'\n',encoding='utf8')
