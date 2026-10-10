import json
from pathlib import Path

rows = json.loads(Path('tmp/terrain-record-lifetime-native-used/report.json').read_text(encoding='utf-8'))['rows']
lines = ['/* Original runtime only; no C-produced expected bytes. */',
         'static const TerrainLifetimeCase terrain_lifetime_cases[]={']
for row in rows:
    arrays = ['{' + ','.join(map(str, row[key]['terrain'])) + '}' for key in ('before', 'after')]
    lines.append('{"%s",%d,%d,%d,%s,%s},' % (row['route'], row['salt'],
                 row['afterState']['map'], row['afterState']['progress'], *arrays))
lines.append('};')
Path('npe-sdl/tests/terrain_record_lifetime_native.h').write_bytes(('\r\n'.join(lines)+'\r\n').encode())
print('Original terrain cases:', len(rows))
