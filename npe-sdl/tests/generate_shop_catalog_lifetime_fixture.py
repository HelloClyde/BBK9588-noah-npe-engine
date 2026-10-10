import json
from pathlib import Path
def read(p):return json.loads(Path(p).read_text(encoding='utf-8'))
rows=[]
for r in read('tmp/native-shop-catalog-retention.json'):
    if r['entry']!='system':continue
    a=r['beforeLoad']['resident'];b=r['after']['resident']
    assert all(a[k]==b[k] for k in ['catalog','scanFirst','scanLast','inventoryOnly'])
    rows.append(('system-'+r['cfg']['name'],0,b))
for r in read('tmp/shop-catalog-title-boot-native/report.json'):
    a=r['before'];b=r['after']
    assert all(a[k]==b[k] for k in ['catalog','scanFirst','scanLast','inventoryOnly'])
    rows.append(('normal-title-'+str(r['salt']),0,b))
original=read('tmp/shop-catalog-restart-native/report.json');assert original['original_only']
for r in original['rows']:
    a=r['before'];b=r['after']
    assert all(a[k]==b[k] for k in ['catalog','scanFirst','scanLast','inventoryOnly'])
    rows.append((r['route']+'-'+str(r['salt']),1,b))
assert len(rows)==11
out=['/* Original catalog lifetime; forced-title lower RAM is excluded. */',
     'typedef struct {const char *name;unsigned restart,first,last,inventory;uint8_t catalog[255];} ShopCatalogLifetime;',
     'static const ShopCatalogLifetime catalog_lifetime[]={']
for name,restart,r in rows:
    out.append('{"'+name+'",'+','.join(map(str,[restart,r['scanFirst'],r['scanLast'],r['inventoryOnly']]))+',{'+','.join(map(str,r['catalog']))+'}},')
out.append('};')
Path('npe-sdl/tests/shop_catalog_lifetime_native.h').write_text('\n'.join(out)+'\n',encoding='utf-8')
print('5 original warm loads and6 original constructors:255 catalog bytes and3 controls retained')
