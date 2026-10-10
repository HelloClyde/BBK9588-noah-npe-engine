import json
from pathlib import Path
rows=json.loads(Path('tmp/role-bank-lifetime-native-final/report.json').read_text(encoding='utf-8'))['rows']
# Title samples remain excluded until their actual lower-RAM location is verified.
rows=[r for r in rows if r['route']!='title-load']
lines=['/* Frozen original runtime bytes; never generated from C results. */',
       'static const RoleLifetimeCase role_lifetime_cases[]={']
def array(v): return '{'+','.join(map(str,v))+'}'
for r in rows:
    lines.append('{"%s",%d,%d,%s,%s,%s,%s,%s},'%(r['route'],r['salt'],r['afterState']['map'],
        array(r['before']['battle']),array(r['before']['map']),array(r['after']['battle']),
        array(r['after']['map']),array(r.get('saved',{}).get('map',[0]*444))))
lines.append('};')
Path('npe-sdl/tests/role_bank_lifetime_native.h').write_bytes(('\r\n'.join(lines)+'\r\n').encode())
print('Original cases',len(rows))
