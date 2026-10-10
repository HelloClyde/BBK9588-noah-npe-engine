import json
from pathlib import Path

rows = json.loads(Path('tmp/native-object-record-load.json').read_text())
assert len(rows) == 16
out = ['/* Captured original public UI loads; not generated from C output. */',
       'static const ObjectLoadCase object_load_cases[]={']
for row in rows:
    before, after = row['beforeBytes'], row['afterBytes']
    assert len(before) == len(after) == 64
    array = lambda values: '{' + ','.join(map(str, values)) + '}'
    out.append('{' + json.dumps(row['entry']) + ',' + str(row['targetMap']) + ',' +
               array(before) + ',' + array(after) + '},')
out.append('};')
Path('npe-sdl/tests/object_record_load_native.h').write_text('\n'.join(out) + '\n')
print('16 original object loads, 1024 captured bytes')
