import json
from pathlib import Path

rows = json.loads(Path('tmp/native-object-public.json').read_text())
assert len(rows) == 16
def array(v): return '{' + ','.join(map(str,v)) + '}'
def state(s): return array(s[k] for k in ('mode','event','map','progress','direction','sx','sy','vx','vy','quantity','flag'))
out = ['/* Actual original public input and LCD commits. */', 'static const ObjectPublicCase object_public_cases[]={']
for row in rows:
    assert row['before']['mode'] == row['after']['mode'] == 1
    assert row['before']['map'] == row['after']['map'] == 80
    assert len(row['commits']) in (0,2)
    commits = '{' + ','.join(state(c['state']) for c in row['commits']) + '}' if row['commits'] else '{{0}}'
    out.append('{' + json.dumps(row['name']) + ',' + str(len(row['commits'])) + ',' + state(row['before']) + ',' + state(row['after']) + ',' + commits + ',' +
               ','.join(array(row[k]) for k in ('objects','flags','npc','monsters','chase','afterObjects','afterFlags')) + '},')
out.append('};')
Path('npe-sdl/tests/object_public_native.h').write_text('\n'.join(out)+'\n')
print('16 original public pickup paths,',sum(len(r['commits']) for r in rows),'LCD commits')
