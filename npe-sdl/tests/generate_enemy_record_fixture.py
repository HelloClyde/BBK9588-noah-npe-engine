import json
from pathlib import Path
r=json.loads(Path('tmp/enemy-record-native/report.json').read_text(encoding='utf8'))
assert r['passed']
def a(values):
    text=','.join(str(v) for v in values)
    return '{'+(text or '0')+'}'
lines=['/* Generated from unchanged original enemy loader; file IO is an explicit boundary. */','static const EnemyRecordCase enemy_record_cases[]={']
for row in r['cases']:
    c=row['input'];chunks=[bytes.fromhex(s) for s in row['supplies']]
    lines.append('{'+','.join([json.dumps(c['name']),json.dumps(row['initial']),json.dumps(row['ram']),
        str(c['party']),str(not bool(c['caps'])).lower().replace('true','1').replace('false','0'),str(len(chunks)),str(len(row['seeks'])),
        a(row['seeks']),a(row['requests']),a(map(len,chunks)),a(row['read_destinations']),
        '{'+','.join(a(d) for d in chunks)+'}' if chunks else '{{0}}'])+'},')
lines.append('};')
Path('npe-sdl/tests/enemy_record_native.h').write_text('\n'.join(lines)+'\n',encoding='utf8')
print(len(r['cases']),'original fixtures')
