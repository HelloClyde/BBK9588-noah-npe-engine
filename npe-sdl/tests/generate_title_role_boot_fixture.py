import json
from pathlib import Path
rows=json.loads(Path('tmp/title-role-boot-native/report.json').read_text(encoding='utf-8'))
assert len(rows)==2
lines=['/* Actual original title boot with original persistent data; never C-derived. */',
       'static const RoleLifetimeCase title_role_boot_cases[]={']
def array(v):return '{'+','.join(map(str,v))+'}'
for row in rows:
    assert row['beforeState']['mode']==7 and row['afterState']['mode']==1
    assert row['afterState']['map']==1 and row['afterState']['progress']==1
    assert len(row['commits'])==1
    assert row['before']['battle']==row['after']['battle']
    assert len(row['after']['battle'])==888 and len(row['after']['map'])==444
    lines.append('{"cold-title-load",%d,1,%s,%s,%s,%s,%s},'%(row['salt'],
        array(row['before']['battle']),array(row['before']['map']),array(row['after']['battle']),
        array(row['after']['map']),array(row['after']['map'])))
assert rows[0]['after']['map']==rows[1]['after']['map']
lines.append('};')
Path('npe-sdl/tests/title_role_boot_native.h').write_bytes(('\r\n'.join(lines)+'\r\n').encode())
print('Two original title boot role banks frozen')
